#include "mediahost.h"

#include "limelog.h"
#include "storage.h"
#include "cache_db.h"

#include <QDebug>

const EventType34 MediaHostReadyType = toEventType34(QEvent::User + 130);
const EventType34 MediaHostProgressType = toEventType34(QEvent::User + 131);

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

// HTTP 回调共享 udata：url 供 progress/done 双回调取数
struct MediaFetchCtx {
    QString url;
};

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
    if (m_cache.contains(url) || m_files.contains(url)) {
        // 已缓存（图片类）/已落盘（video/file 非图片）→ 向调用方重同步状态
        Q_EMIT mediaStateChanged(url, MediaDlCompleted);
        return;
    }
    // 重启兜底（qltox startMediaDownload :4130 CacheDb loadMedia 命中同款
    // 语义，不新增 exists()）：下载入口同步读一次字节，非空 = 已下载过 →
    // 直接重同步 Completed；可解码顺带装回内存缩略缓存，不再发 GET
    {
        const std::vector<uint8_t> dbBytes = Storage::instance().cacheDb()->get(
            mediaCacheKey("msg", url).c_str());
        if (!dbBytes.empty()) {
            QImage img;
            img.loadFromData(dbBytes.data(), (int)dbBytes.size());
            if (!img.isNull()) {
                insertCache(url, img);
            } else {
                m_files.insert(url);
            }
            Q_EMIT mediaStateChanged(url, MediaDlCompleted);
            return;
        }
    }
    if (m_pending.contains(url)) {
        return;
    }
    const QString httpUrl = mxcToHttp(url);
    if (httpUrl.isEmpty()) {
        ALOG_WARN("media download skip: bad url", url);
        Q_EMIT mediaStateChanged(url, MediaDlFailed);
        return;
    }
    m_pending.insert(url);
    Q_EMIT mediaStateChanged(url, MediaDlInProgress);
    HttpRequest req(std::string(httpUrl.toUtf8().constData()), "GET");
    req.followRedirects = true;
    // 泵线程回调只搬数字/字节（解码/落盘一律在 GUI customEvent）
    MediaFetchCtx* ctx = new MediaFetchCtx();
    ctx->url = url;
    req.progress = [](long long received, long long total,
                      long long speedBps, void* udata) {
        auto* pumpCtx = static_cast<MediaFetchCtx*>(udata);
        MediaHostProgressEvent* ev = new MediaHostProgressEvent();
        ev->mxcUrl = std::string(pumpCtx->url.toUtf8().constData());
        ev->received = received;
        ev->total = total;
        ev->speedBps = speedBps;
        qAppPostEvent(MediaHost::instance(), ev);
    };
    EventPoller::addRequest(req, [](const HttpResponse& resp, void* udata) {
        MediaFetchCtx* pumpCtx = static_cast<MediaFetchCtx*>(udata);
        MediaReadyEvent* ev = new MediaReadyEvent();
        ev->mxcUrl = std::string(pumpCtx->url.toUtf8().constData());
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
        delete pumpCtx;
    }, ctx);
}

QString MediaHost::thumbnailUrl(const QString& url, int width, int height)
{
    if (!url.startsWith(QStringLiteral("mxc://"))
        || width <= 0 || height <= 0) {
        return QString();
    }
    const QString rest = url.mid(6);
    const int slash = rest.indexOf(QLatin1Char('/'));
    if (slash <= 0 || slash >= rest.size() - 1) {
        return QString();
    }
    return QStringLiteral(
        "https://%1/_matrix/media/v3/thumbnail/%1/%2?width=%3&height=%4&method=scale")
        .arg(rest.left(slash), rest.mid(slash + 1))
        .arg(width).arg(height);
}

std::vector<uint8_t> MediaHost::getBytes(const QString& url)
{
    if (url.isEmpty()) {
        return {};
    }
    // 与 AvatarManager::get（avatar_manager.cpp:76）同款同步读（GUI 线程）
    return Storage::instance().cacheDb()->get(
        mediaCacheKey("msg", url).c_str());
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

void MediaHost::msgDbStore(const QString& key, const std::string& body,
                           const std::string& mime)
{
    std::string dbKey = mediaCacheKey("msg", key);
    const auto* raw = reinterpret_cast<const uint8_t*>(body.data());
    std::vector<uint8_t> data(raw, raw + body.size());
    Storage::instance().cacheDbAsync()->storeMedia(
        std::move(dbKey), std::move(data), mime, 2, nullptr);
}

void MediaHost::customEvent(QEvent* event)
{
    if (event->type() == MediaHostProgressType) {
        MediaHostProgressEvent* evt =
            static_cast<MediaHostProgressEvent*>(event);
        const QString key = QString::fromUtf8(evt->mxcUrl.c_str());
        const int pct = (evt->total > 0)
            ? (int)(evt->received * 100 / evt->total) : -1;
        Q_EMIT mediaProgress(key, pct, (int)evt->speedBps);
        return;
    }
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
                // 图片类：原字节落盘（不做 PNG 转码，qltox storeMedia 口径）+
                // 内存缩略缓存
                std::string mime = "image/png";
                if (!evt->mimeType.empty()) {
                    mime = evt->mimeType;
                }
                msgDbStore(key, evt->body, mime);
                insertCache(key, img);
                stored = true;
            } else {
                // 非图片字节（video/file/audio）：原字节落盘，内存缓存不参与；
                // 不算失败（qltox Completed 档同义：字节到手，供后续消费）
                ALOG_WARN("media non-image stored", key, (int)evt->body.size());
                std::string mime = !evt->mimeType.empty()
                    ? evt->mimeType : "application/octet-stream";
                msgDbStore(key, evt->body, mime);
                m_files.insert(key);
                stored = true;
            }
        }
        if (stored) {
            Q_EMIT mediaUpdated(key);
            Q_EMIT mediaStateChanged(key, MediaDlCompleted);
        } else {
            ALOG_WARN("media fetch failed", key, evt->errorInfo);
            Q_EMIT mediaStateChanged(key, MediaDlFailed);
        }
        return;
    }
    QObject::customEvent(event);
}
