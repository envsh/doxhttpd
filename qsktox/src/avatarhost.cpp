#include "avatarhost.h"

#include "avatar_manager.h"
#include "eventpoller.h"
#include "limelog.h"

#include <QDebug>
#include <QImage>

namespace {

// mxc://HOST/MEDIAID → https://HOST/_matrix/media/v3/download/HOST/MEDIAID
// 仅接受 mxc:// 或 http(s)://，其余视为非法。
QString mxcToHttp(const QString& url)
{
    if (url.startsWith(QStringLiteral("mxc://"))) {
        const QString rest = url.mid(6);
        const int slash = rest.indexOf(QLatin1Char('/'));
        if (slash <= 0 || slash >= rest.size() - 1) {
            return QString();
        }
        const QString host = rest.left(slash);
        const QString mediaId = rest.mid(slash + 1);
        return QStringLiteral("https://%1/_matrix/media/v3/download/%1/%2")
            .arg(host, mediaId);
    }
    if (url.startsWith(QStringLiteral("http://"))
        || url.startsWith(QStringLiteral("https://"))) {
        return url;
    }
    return QString();
}

constexpr int kAvatarSize = 36;

} // namespace

AvatarHost::AvatarHost()
    : QObject(nullptr)
{
}

AvatarHost* AvatarHost::instance()
{
    static AvatarHost* s_instance = nullptr;
    if (!s_instance) {
        s_instance = new AvatarHost();
    }
    return s_instance;
}

void AvatarHost::download(const QString& avatarUrl)
{
    if (avatarUrl.isEmpty()) {
        ALOG_WARN("avatar download skip: empty url");
        return;
    }
    // 下载门槛（对齐 qltox requestDownload 调用约定，mainwindow.cpp:1472/1671/
    // 2574/2684/2882 同款）：AvatarManager 内存缓存/在途命中即不再发起请求。
    // 背景：message 页 Transient（main.cpp:354），每次进会话页面销毁重建 →
    // populate 逐条 download（messagelist.cpp:1094/868），AvatarHost 只挡在途
    // （m_pending 完成即清，customEvent:96）→ 此前每次进会话重复 GET 该会话
    // 全部头像。磁盘 CacheDb 命中依赖会话首次绘制的 get() 把图装回内存后
    // 生效（与 qltox 相同的顺序语义）。user icon / chan icon 同 mxc 键，
    // 天然同时生效。成功/失败收尾无需配套改动：
    //   成功 → store() 已 erase manager 侧 pending（avatar_manager.cpp:96）
    //   失败 → customEvent 已 removePending（avatarhost.cpp:109-112）
    if (!AvatarManager::inst().requestDownload(avatarUrl)) {
        return;
    }
    if (m_pending.contains(avatarUrl)) {
        return;
    }
    const QString httpUrl = mxcToHttp(avatarUrl);
    if (httpUrl.isEmpty()) {
        ALOG_WARN("avatar download skip: bad url", avatarUrl);
        return;
    }
    m_pending.insert(avatarUrl);
    HttpRequest req(std::string(httpUrl.toUtf8().constData()), "GET");
    req.followRedirects = true;
    EventPoller::addRequest(req, [](const HttpResponse& resp, void* udata) {
        const QString* url = static_cast<const QString*>(udata);
        AvatarDownloadEvent* ev = new AvatarDownloadEvent();
        ev->mxcUrl = std::string(url->toUtf8().constData());
        // QPixmap 仅限 GUI 线程（Android 无 ThreadedPixmaps），
        // 泵线程只搬运字节，解码放在 customEvent 完成
        if (resp.httpCode != 200 || resp.body.empty()) {
            ev->success = false;
            std::string type;
            auto itH = resp.headers.find("content-type");
            if (itH != resp.headers.end()) {
                type = itH->second;
            }
            ev->errorInfo = "HTTP " + std::to_string(resp.httpCode)
                + " curl[" + resp.curlErrStr + "]"
                + " size[" + std::to_string(resp.body.size()) + "]"
                + " type[" + type + "]"
                + " final[" + resp.finalUrl + "]"
                + " time[" + std::to_string(resp.elapsedMs) + "ms]";
        } else {
            ev->body = resp.body;
            ev->success = true;
        }
        delete url;
        qAppPostEvent(AvatarHost::instance(), ev);
    }, new QString(avatarUrl));
}

void AvatarHost::customEvent(QEvent* event)
{
    if (event->type() == AvatarDownloadReadyType) {
        AvatarDownloadEvent* evt = static_cast<AvatarDownloadEvent*>(event);
        const QString key = QString::fromUtf8(evt->mxcUrl.c_str());
        m_pending.remove(key);
        bool stored = false;
        if (evt->success && !evt->body.empty()) {
            QImage img;
            if (img.loadFromData(
                    reinterpret_cast<const uchar*>(evt->body.data()),
                    (int)evt->body.size())) {
                AvatarManager::inst().store(key, QPixmap::fromImage(img),
                                            kAvatarSize);
                stored = true;
            } else {
                ALOG_WARN("avatar decode failed", key,
                          (int)evt->body.size());
            }
        }
        if (stored) {
            Q_EMIT avatarUpdated();
        } else {
            AvatarManager::inst().removePending(key);
            if (!evt->success) {
                ALOG_WARN("avatar fetch failed", key, evt->errorInfo);
            }
        }
        return;
    }
    QObject::customEvent(event);
}
