#include "linkpreviewhost.h"
#include "eventpoller.h"

#include <QDebug>

// qsk 本地事件段（mediahost 用 130/131，此处顺延）
const EventType34 LinkPreviewReadyType = toEventType34(QEvent::User + 132);

namespace {

// 泵线程 → GUI 回传载荷（字节/头原样搬运；解析一律 GUI customEvent）
class LpReadyEvent : public CustomEventBase {
public:
    LpReadyEvent() : CustomEventBase(LinkPreviewReadyType) {}
    std::string urlKey;
    std::string body;
    std::string contentType;
    // 泵线程汇总错误（HTTP 码 / curl / 大小全量）；GUI 只补解析类错误
    std::string httpError;
    bool httpOk = false;
};

// 泵线程回调共享 udata
struct LpCtx {
    QString urlKey;
};
constexpr qint64 kMaxBodyBytes = 512 * 1024;

bool contentLooksHtml(const std::string& ct)
{
    return ct.find("text/html") != std::string::npos
        || ct.find("application/xhtml") != std::string::npos;
}

} // namespace

LinkPreviewHost* LinkPreviewHost::s_instance = nullptr;
bool LinkPreviewHost::s_enabled = true;

LinkPreviewHost* LinkPreviewHost::instance()
{
    if (!s_instance) {
        s_instance = new LinkPreviewHost();
    }
    return s_instance;
}

void LinkPreviewHost::requestPreview(const QString& url)
{
    if (!s_enabled || url.isEmpty()) {
        return;
    }
    const QString urlKey = linkPreviewNormalizeUrl(url);
    if (urlKey.isEmpty()) {
        qDebug() << "[linkpreview] reject: empty url key";
        return;
    }
    LinkPreviewData cached;
    if (LinkPreviewCache::inst().get(urlKey, cached)) {
        // 命中（含未过期负缓存）→ 调用方自行 filter；直接转发
        Q_EMIT linkReady(urlKey, cached);
        return;
    }
    if (LinkPreviewCache::inst().isInflight(urlKey)) {
        return;
    }
    LinkPreviewCache::inst().markInflight(urlKey);
    HttpRequest req(std::string(urlKey.toUtf8().constData()), "GET",
                    "", 12, {{"User-Agent",
        "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) "
        "Chrome/120.0.0.0 Safari/537.36"}});
    req.followRedirects = true;
    LpCtx* ctx = new LpCtx();
    ctx->urlKey = urlKey;
    EventPoller::addRequest(req, [](const HttpResponse& resp, void* udata) {
        auto* pumpCtx = static_cast<LpCtx*>(udata);
        LpReadyEvent* ev = new LpReadyEvent();
        ev->urlKey = std::string(pumpCtx->urlKey.toUtf8().constData());
        if (resp.httpCode != 200 || resp.body.empty()) {
            ev->httpOk = false;
            ev->httpError = "HTTP " + std::to_string(resp.httpCode)
                + " curl[" + resp.curlErrStr + "]"
                + " size[" + std::to_string(resp.body.size()) + "]"
                + " final[" + resp.finalUrl + "]"
                + " time[" + std::to_string(resp.elapsedMs) + "ms]";
        } else {
            ev->httpOk = true;
            ev->body = resp.body;
            auto itH = resp.headers.find("content-type");
            if (itH != resp.headers.end()) {
                ev->contentType = itH->second;
            }
        }
        qAppPostEvent(LinkPreviewHost::instance(), ev);
        delete pumpCtx;
    }, ctx);
}

void LinkPreviewHost::customEvent(QEvent* event)
{
    if (event->type() != LinkPreviewReadyType) {
        QObject::customEvent(event);
        return;
    }
    auto* ev = static_cast<LpReadyEvent*>(event);
    const QString urlKey = QString::fromUtf8(ev->urlKey.c_str());
    LinkPreviewCache::inst().clearInflight(urlKey);
    if (!ev->httpOk) {
        qDebug() << "[linkpreview] fail" << urlKey
                 << QString::fromUtf8(ev->httpError.c_str());
        LinkPreviewData bad;
        bad.ok = false;
        bad.url = urlKey;
        LinkPreviewCache::inst().put(urlKey, bad);
        Q_EMIT linkReady(urlKey, bad);
        return;
    }
    const QString contentType = QString::fromUtf8(ev->contentType.c_str());
    QByteArray body(ev->body.data(), (int)qMin<qint64>(ev->body.size(),
                                                       kMaxBodyBytes));
    if ((qint64)ev->body.size() > kMaxBodyBytes) {
        qDebug() << "[linkpreview] body truncated" << urlKey
                 << "size=" << (qint64)ev->body.size();
    }
    // 非 html 站点（pdf/img/二进制）不出卡也不进负缓存轰炸：负缓存照写
    //（失败口径）；解析只对 html 体有意义
    if (!contentLooksHtml(ev->contentType)) {
        qDebug() << "[linkpreview] reject non-html" << urlKey << contentType;
        LinkPreviewData bad;
        bad.ok = false;
        bad.url = urlKey;
        LinkPreviewCache::inst().put(urlKey, bad);
        Q_EMIT linkReady(urlKey, bad);
        return;
    }
    LinkPreviewData data;
    if (!parseLinkPreviewHtml(body, contentType, urlKey, data)) {
        qDebug() << "[linkpreview] parse miss" << urlKey << contentType;
        LinkPreviewData bad;
        bad.ok = false;
        bad.url = urlKey;
        LinkPreviewCache::inst().put(urlKey, bad);
        Q_EMIT linkReady(urlKey, bad);
        return;
    }
    LinkPreviewCache::inst().put(urlKey, data);
    Q_EMIT linkReady(urlKey, data);
}
