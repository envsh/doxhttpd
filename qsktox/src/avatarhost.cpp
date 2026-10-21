#include "avatarhost.h"

#include "avatar_manager.h"
#include "eventpoller.h"

#include <QDebug>

namespace {

// mxc://HOST/MEDIAID → https://HOST/_matrix/media/v3/download/HOST/MEDIAID
// 非 mxc（如 https:// 直链）原样返回；格式非法返回空。
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
        return;
    }
    if (m_pending.contains(avatarUrl)) {
        return;
    }
    const QString httpUrl = mxcToHttp(avatarUrl);
    if (httpUrl.isEmpty()) {
        return;
    }
    m_pending.insert(avatarUrl);
    HttpRequest req(std::string(httpUrl.toUtf8().constData()), "GET");
    req.followRedirects = true;
    EventPoller::addRequest(req, [](const HttpResponse& resp, void* udata) {
        const QString* url = static_cast<const QString*>(udata);
        AvatarDownloadEvent* ev = new AvatarDownloadEvent();
        ev->mxcUrl = std::string(url->toUtf8().constData());
        if (resp.httpCode != 200 || resp.body.empty()) {
            ev->success = false;
            ev->errorInfo = "HTTP " + std::to_string(resp.httpCode);
        } else if (!ev->pixmap.loadFromData(
                       (const uchar*)resp.body.data(), resp.body.size())) {
            ev->success = false;
            ev->errorInfo = "pixmap load failed";
        } else {
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
        if (evt->success && !evt->pixmap.isNull()) {
            AvatarManager::inst().store(key, evt->pixmap, kAvatarSize);
            Q_EMIT avatarUpdated();
        } else {
            AvatarManager::inst().removePending(key);
        }
        return;
    }
    QObject::customEvent(event);
}
