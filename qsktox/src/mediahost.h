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

    // 内存 → CacheDb（同步读）。无缓存返回空 QImage（调用方走占位绘制）。
    QImage get(const QString& url);

Q_SIGNALS:
    void mediaUpdated(const QString& url);

protected:
    void customEvent(QEvent* event) override;

private:
    MediaHost();
    void insertCache(const QString& key, const QImage& img);

    QSet<QString> m_pending;
    QMap<QString, QImage> m_cache;   // FIFO 上限淘汰（近似 LRU，P1 不做严格时效）
};

#endif // MEDIA_HOST_H
