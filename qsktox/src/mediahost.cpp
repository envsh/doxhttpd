#include "mediahost.h"

#include "limelog.h"
#include "storage.h"
#include "cache_db.h"

#include <QDebug>

const EventType34 MediaHostReadyType = toEventType34(QEvent::User + 130);

namespace {

// mxc://HOST/MEDIAID → https://HOST/_matrix/media/v3/download/HOST/MEDIAID
// 与 avatarhost.cpp:14 同款（各自独立，不引入对既有文件的共享改动）；
// 非 mxc（https 直链）原样返回，格式非法返回空。
QString mxcToHttp(const QString& url)
{
    if (!url.startsWith(QStringLiteral("mxc://"))) {
        return url;
    }
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

constexpr int kMediaMaxDim = 720;   // 内存缩略缓存窄边上限
constexpr int kMediaCacheCap = 64;  // 入口数上限（FIFO 淘汰）

} // namespace

MediaHost::MediaHost()
    : QObject(nullptr)
{
}

MediaHost* MediaHost::instance()
{
    static MediaHost* s_instance = nullptr;
    if (!s_instance) {
        s_instance = new MediaHost();
    }
    return s_instance;
}

void MediaHost::autoDownload(const QString& msgType, const QString& mime,
                             const QString& url)
{
    if (url.isEmpty()) {
        return;
    }
    // m.image / image 两写法统一判含 image；mime 兜底（push 载荷两键都缺时仅占位）
    if (!msgType.contains(QStringLiteral("image"))
        && !mime.startsWith(QStringLiteral("image/"))) {
        return;
    }
    download(url);
}

void MediaHost::download(const QString& url)
{
    if (url.isEmpty()) {
        ALOG_WARN("media download skip: empty url");
        return;
    }
    if (m_cache.contains(url) || m_pending.contains(url)) {
        return;
    }
    const QString httpUrl = mxcToHttp(url);
    if (httpUrl.isEmpty()) {
        ALOG_WARN("media download skip: bad url", url);
        return;
    }
    m_pending.insert(url);
    // 泵线程回调只搬字节（avatarhost.cpp:82 同款注释口径：QPixmap/解码禁泵线程）
    HttpRequest req(std::string(httpUrl.toUtf8().constData()), "GET");
    req.followRedirects = true;
    EventPoller::addRequest(req, [](const HttpResponse& resp, void* udata) {
        const QString* url = static_cast<const QString*>(udata);
        MediaReadyEvent* ev = new MediaReadyEvent();
        ev->mxcUrl = std::string(url->toUtf8().constData());
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
            auto itH = resp.headers.find("content-type");
            if (itH != resp.headers.end()) {
                ev->mimeType = itH->second;
            }
        }
        qAppPostEvent(MediaHost::instance(), ev);
        delete url;
    }, new QString(url));
}

QImage MediaHost::get(const QString& url)
{
    if (url.isEmpty()) {
        return QImage();
    }
    auto it = m_cache.find(url);
    if (it != m_cache.end()) {
        return it.value();
    }
    // 与 AvatarManager::get（avatar_manager.cpp:76）同款同步读（GUI 线程）
    std::vector<uint8_t> data = Storage::instance().cacheDb()->get(
        mediaCacheKey("msg", url).c_str());
    if (data.empty()) {
        return QImage();
    }
    QImage img;
    if (!img.loadFromData(data.data(), (int)data.size())) {
        ALOG_WARN("media cache decode failed", url, (int)data.size());
        return QImage();
    }
    insertCache(url, img);
    return img;
}

void MediaHost::insertCache(const QString& key, const QImage& img)
{
    if (img.isNull()) {
        return;
    }
    QImage thumb = img;
    if (thumb.width() > kMediaMaxDim || thumb.height() > kMediaMaxDim) {
        thumb = thumb.scaled(kMediaMaxDim, kMediaMaxDim,
            Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    m_cache.insert(key, thumb);
    while (m_cache.size() > kMediaCacheCap) {
        m_cache.erase(m_cache.begin());
    }
}

void MediaHost::customEvent(QEvent* event)
{
    if (event->type() == MediaHostReadyType) {
        MediaReadyEvent* evt = static_cast<MediaReadyEvent*>(event);
        const QString key = QString::fromUtf8(evt->mxcUrl.c_str());
        m_pending.remove(key);
        bool stored = false;
        if (evt->success && !evt->body.empty()) {
            QImage img;
            if (img.loadFromData(
                    reinterpret_cast<const uchar*>(evt->body.data()),
                    (int)evt->body.size())) {
                // 原字节落盘（qltox storeMedia 同口径，不做 PNG 转码）；
                // mime 缺省用 image/png 占位（仅 CacheDb 记录，不参与 decode）
                std::string mime = "image/png";
                if (!evt->mimeType.empty()) {
                    mime = evt->mimeType;
                }
                std::string dbKey = mediaCacheKey("msg", key);
                const auto* raw =
                    reinterpret_cast<const uint8_t*>(evt->body.data());
                std::vector<uint8_t> data(raw, raw + evt->body.size());
                Storage::instance().cacheDbAsync()->storeMedia(
                    std::move(dbKey), std::move(data), mime, 2, nullptr);
                insertCache(key, img);
                stored = true;
            } else {
                ALOG_WARN("media decode failed", key, (int)evt->body.size());
            }
        }
        if (stored) {
            Q_EMIT mediaUpdated(key);
        } else if (!evt->success) {
            ALOG_WARN("media fetch failed", key, evt->errorInfo);
        }
        return;
    }
    QObject::customEvent(event);
}
