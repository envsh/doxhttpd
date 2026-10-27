#include "mediapreview.h"
#include "mediahost.h"
#include "messagelist.h"
#include "mediaviewer.h"
#include "videoviewer.h"

#include <QFile>
#include <QIODevice>
#include <QPointer>

namespace MediaPreview {

static QByteArray resolveBytes(const QString& src)
{
    if (src.isEmpty()) {
        return QByteArray();
    }
    if (!src.contains(QStringLiteral("://"))) {
        // 本地文件（发送方 localPath）：直读磁盘，不经 CacheDb
        QFile f(src);
        if (!f.open(QIODevice::ReadOnly)) {
            return QByteArray();
        }
        return f.readAll();
    }
    // URL 源（mxc）：MediaHost 同步读 CacheDb（GUI 线程，getBytes 约定）
    const std::vector<uint8_t> bytes = MediaHost::instance()->getBytes(src);
    return QByteArray(reinterpret_cast<const char*>(bytes.data()),
        static_cast<int>(bytes.size()));
}

void showFor(QQuickItem* parent, const MessageItem& item, int mediaType)
{
    const bool isGif = (mediaType == static_cast<int>(MsgMediaType::Gif));
    const QString src = !item.mediaUrl.isEmpty() ? item.mediaUrl
        : item.localPath;
    const bool remote = src.contains(QStringLiteral("://"));

    const QByteArray raw = resolveBytes(src);
    if (!raw.isEmpty()) {
        MediaViewerPopup::show(parent, raw, isGif, false, QString());
        return;
    }
    // 字节缺：pending 打开 + 下载 + mediaUpdated 到达自动递重料
    //（qltox 视频 pendingPlay :4350 模式泛化到图片/GIF）
    if (!remote) {
        MediaViewerPopup::show(parent, raw, isGif, false, QString());
        return;   // localPath 读失败 → 组件显示"无法加载"
    }
    MediaViewerPopup* popup = MediaViewerPopup::show(parent, raw, isGif,
        true, QString::fromUtf8("⏳ 下载中…"));
    MediaHost::instance()->download(src);
    const QPointer<MediaViewerPopup> popupGuard(popup);
    // watcher 挂 popup 上：弹窗关闭即自动断连
    QObject* watcher = new QObject(popup);
    QObject::connect(MediaHost::instance(), &MediaHost::mediaUpdated,
        watcher, [popupGuard, src, isGif](const QString& url) {
            if (url != src || popupGuard.isNull()) {
                return;
            }
            const QByteArray fresh = resolveBytes(src);
            if (!fresh.isEmpty()) {
                popupGuard->canvas()->setData(fresh, isGif, false, QString());
            } else {
                // 解不出图：没有 pending 重开意义，直接关闭现层
                popupGuard->close();
            }
        });
}

void showVideoFor(QQuickItem* parent, const MessageItem& item)
{
    const QString mime = !item.audioMime.isEmpty()
        ? item.audioMime : item.mediaMime;
    const QString src = !item.mediaUrl.isEmpty() ? item.mediaUrl
        : item.localPath;
    const bool remote = src.contains(QStringLiteral("://"));

    const QByteArray raw = resolveBytes(src);
    if (!raw.isEmpty() || !remote) {
        // 齐/本地读失败：非 pending；空字节 → 组件内"无法加载"
        VideoViewerPopup::show(parent, raw, mime, false, QString());
        return;
    }
    // pendingPlay（qltox :4350 泛化）：pending 打开 + download + 自动播
    VideoViewerPopup* popup = VideoViewerPopup::show(parent, raw, mime,
        true, QString::fromUtf8("⏳ 下载中…"));
    MediaHost::instance()->download(src);
    const QPointer<VideoViewerPopup> popupGuard(popup);
    // watcher 挂 popup 上：弹窗关闭即自动断连
    QObject* watcher = new QObject(popup);
    QObject::connect(MediaHost::instance(), &MediaHost::mediaUpdated,
        watcher, [popupGuard, src, mime](const QString& url) {
            if (url != src || popupGuard.isNull()) {
                return;
            }
            const QByteArray fresh = resolveBytes(src);
            if (!fresh.isEmpty()) {
                popupGuard->canvas()->setData(fresh, mime, false, QString());
            } else {
                // 解不出/字节缺：没有 pending 重开意义，直接关闭现层
                popupGuard->close();
            }
        });
}

} // namespace MediaPreview
