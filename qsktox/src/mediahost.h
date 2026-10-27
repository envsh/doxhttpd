#ifndef MEDIA_HOST_H
#define MEDIA_HOST_H

#include "eventpoller.h"

#include <QObject>
#include <QString>
#include <QSet>
#include <QMap>
#include <QImage>

// 消息媒体下载完成事件（qsk 本地，不动 qlappcom 共用 eventpoller.h 的事件表）。
// 泵线程只搬字节与 header mime，GUI customEvent 内做 decode/落盘/入缓存。
extern const EventType34 MediaHostReadyType;   // QEvent::User + 130（qsk 本地段）

class MediaReadyEvent : public CustomEventBase {
public:
    MediaReadyEvent() : CustomEventBase(MediaHostReadyType) {}
    std::string mxcUrl;
    std::string body;
    std::string mimeType;
    std::string errorInfo;
    bool success = false;
};

extern const EventType34 MediaHostProgressType;  // QEvent::User + 131（qsk 本地段）

// 下载进度事件：EventPoller progress 回调（泵线程，xferinfoCb 内已 100ms 节流）
// → post GUI，GUI 侧转发 mediaProgress 信号。
class MediaHostProgressEvent : public CustomEventBase {
public:
    MediaHostProgressEvent() : CustomEventBase(MediaHostProgressType) {}
    std::string mxcUrl;
    long long received = 0;
    long long total = 0;
    long long speedBps = 0;
};

// 下载四态（qltox ChatElement::DownloadState 口径，chatview.h:90）
enum MediaDlState {
    MediaDlNotRequested = 0,
    MediaDlInProgress   = 1,
    MediaDlCompleted    = 2,
    MediaDlFailed       = 3,
};

// 消息媒体下载宿主：mxc:// 按 Matrix 标准转 https 直连，https:// 直链直接请求。
// 下载成功落 CacheDb（"msg_" 前缀键，tag=2，原字节不转码）+ 内存缩略缓存
//（窄边 ≤720px，上限 64 张），发 mediaUpdated(url) → MessageListWidget 定向刷新。
// 自动抓取口径对齐 qltox settings 预设（media/downloadImage=true、
// downloadGif=true、downloadVideo/File=false）：仅 image 类（含 GIF 静帧）。
class MediaHost : public QObject
{
    Q_OBJECT
public:
    static MediaHost* instance();

    Q_DISABLE_COPY(MediaHost)

    // 触发一次媒体下载（内部去重：内存命中或在途则跳过）。
    void download(const QString& url);

    // qltox 口径：msgtype(image/m.image) 或 mimetype image/* 才自动抓。
    void autoDownload(const QString& msgType, const QString& mime,
                      const QString& url);

    // CS 标准 /_matrix/media/v3/thumbnail 构造（mxc 且有宽高才有意义）；
    // 非 mxc 或缺宽高返回空（无服务端生成能力 → 调用方保持纯占位）。
    static QString thumbnailUrl(const QString& url, int width, int height);

    // 原始字节同步读（CacheDb "msg_" 键，GUI 线程；供 GIF QMovie/QBuffer 用，
    // 与 get() 的内存 QImage 路径独立）
    std::vector<uint8_t> getBytes(const QString& url);

    // 内存 → CacheDb（同步读）。无缓存返回空 QImage（调用方走占位绘制）。
    QImage get(const QString& url);

Q_SIGNALS:
    void mediaUpdated(const QString& url);
    void mediaProgress(const QString& url, int pct, int speedBps);
    void mediaStateChanged(const QString& url, int state);
    // 失败详情（qltox ChatElement::mediaErrorMsg 口径，chatview.h:129）：
    // 信号新增而不改 mediaStateChanged 签名（既有多处 emit/连接不受影响）
    void mediaFailed(const QString& url, const QString& errorInfo);

protected:
    void customEvent(QEvent* event) override;

private:
    MediaHost();
    void insertCache(const QString& key, const QImage& img);
    void msgDbStore(const QString& key, const std::string& body,
                    const std::string& mime);

    QSet<QString> m_pending;
    QMap<QString, QImage> m_cache;   // FIFO 上限淘汰（近似 LRU，P1 不做严格时效）
    // 已落盘但非图片字节（video/file/audio）：MediaHost 侧持久去重标记
    QSet<QString> m_files;
};

#endif // MEDIA_HOST_H
