#include "mozpushsender.h"
#include "androidutils.h"
#include "webpushcrypto.h"
#include "pushhandler.h"

#include <QDateTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSettings>
#include <QUrl>

#include <thread>

#include "cJSON.h"

namespace {

// 全部为 CPU/JNI 重活，仅供工作线程调用（勿在主线程调用）。
bool buildRequestBody(const QString& deriveUaid, const QString& postEndpoint,
                      const QString& pushType, const QString& payEndpoint,
                      const QString& payUaid, quint64 seq,
                      QByteArray* body, QString* authz)
{
    QByteArray p256dh;
    QByteArray auth;
    if (!WebPushCrypto::deriveMobSubKeys(deriveUaid, &p256dh, &auth)) {
        qWarning() << "[mozpush] derive keys failed";
        return false;
    }

    const QString deviceName = androidDeviceName();

    cJSON* o = cJSON_CreateObject();
    if (!o) {
        return false;
    }
    cJSON_AddStringToObject(o, "proto_type", "mobsub");
    cJSON_AddStringToObject(o, "push_type", pushType.toUtf8().constData());
    cJSON_AddStringToObject(o, "uaid", payUaid.toUtf8().constData());
    cJSON_AddStringToObject(o, "virtip4", "10.0.0.0");
    cJSON_AddStringToObject(o, "peerid7", deviceName.toUtf8().constData());
    cJSON_AddNumberToObject(o, "seq", static_cast<double>(seq));
    cJSON_AddNumberToObject(o, "ts", static_cast<double>(QDateTime::currentSecsSinceEpoch()));
    cJSON_AddStringToObject(o, "endpoint", payEndpoint.toUtf8().constData());
    char* js = cJSON_PrintUnformatted(o);
    cJSON_Delete(o);
    if (!js) {
        return false;
    }
    const QByteArray payload(js);
    cJSON_free(js);

    const QByteArray enc = WebPushCrypto::encryptAes128gcm(payload, p256dh, auth);
    if (enc.isEmpty()) {
        qWarning() << "[mozpush] encrypt failed";
        return false;
    }
    const QString az = WebPushCrypto::vapidHeader(postEndpoint);
    if (az.isEmpty()) {
        qWarning() << "[mozpush] vapid header failed";
        return false;
    }

    *body = enc;
    *authz = az;
    return true;
}

} // namespace

MozpushSender::MozpushSender(QObject* parent)
    : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
    m_timer.setInterval(39000);
    connect(&m_timer, &QTimer::timeout, this, &MozpushSender::sendNow);
}

MozpushSender* MozpushSender::instance()
{
    static MozpushSender* s = new MozpushSender();
    return s;
}

void MozpushSender::start()
{
    MozpushSender* self = instance();
    self->reloadSettings();
    androidRequestBluetoothNamePermission();
    self->m_timer.start();
    // 延后到事件循环启动后（start() 可能在 app.exec() 之前被调用）
    QTimer::singleShot(0, self, &MozpushSender::sendNow);
    qDebug() << "[mozpush] started uaid=" << self->m_uaid
             << "endpoint=" << self->m_endpoint;
}

void MozpushSender::resume()
{
#ifdef Q_OS_ANDROID
    m_timer.start();
#endif
    sendNow();
}

void MozpushSender::pause()
{
#ifdef Q_OS_ANDROID
    m_timer.stop();
#endif
}

void MozpushSender::sendNow()
{
    buildSend();
}

void MozpushSender::reloadSettings()
{
    QSettings s;
    m_uaid = s.value("mozpushUaid").toString().trimmed();
    m_endpoint = s.value("mozpushEndpoint").toString().trimmed();
}

void MozpushSender::buildSend()
{
    reloadSettings();
    if (m_uaid.isEmpty() || m_endpoint.isEmpty()) {
        qDebug() << "[mozpush] 未设置 mozpushUaid/mozpushEndpoint, 跳过发送";
        return;
    }

    // push_type 依据当前激活分发器判定（与状态栏同源）
    QString dist;
    if (auto* ph = PushHandler::instance()) {
        dist = ph->currentDistributor();
    }

    QString pushType;
    if (dist == QLatin1String("io.heckel.ntfy")) {
        pushType = QStringLiteral("ntfysh");
    } else if (dist == QLatin1String("org.unifiedpush.distributor.sunup")
               || dist.isEmpty()) {
        pushType = QStringLiteral("sunup");
    } else {
        qDebug() << "[mozpush] backend" << dist << "not ntfysh/sunup, skip";
        return;
    }

    // payload 的 endpoint 只取“设备侧注册到的推送 endpoint”(pushDeviceEndpoint)。
    // 注意：mozpushUaid/mozpushEndpoint 是传输用途（派生密钥 / VAPID 签名 / POST 目标），
    // 绝不允许出现在发送的数据（payload）中。
    const QString payEndpoint =
        QSettings().value("pushDeviceEndpoint").toString().trimmed();
    const QString payUaid;   // sunup 下可能为空；空值即空值

    const QString deriveUaid = m_uaid;
    const QString endpoint = m_endpoint;
    const quint64 seq = ++m_seq;

    MozpushSender* self = this;
    // 重活移出 UI 线程；完成后经队列连接把 POST 派回主线程
    std::thread([self, deriveUaid, endpoint, pushType, payEndpoint, payUaid, seq]() {
        QByteArray body;
        QString authz;
        if (!buildRequestBody(deriveUaid, endpoint, pushType, payEndpoint,
                              payUaid, seq, &body, &authz)) {
            return;
        }
        QMetaObject::invokeMethod(self, [self, body, authz, endpoint]() {
            self->postRequest(body, authz, endpoint);
        }, Qt::QueuedConnection);
    }).detach();
}

void MozpushSender::postRequest(const QByteArray& body, const QString& authz,
                                const QString& endpoint)
{
    QNetworkRequest req{QUrl(endpoint)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/octet-stream");
    req.setRawHeader("Content-Encoding", "aes128gcm");
    req.setRawHeader("TTL", "39");
    req.setRawHeader("Authorization", authz.toUtf8());

    QNetworkReply* reply = m_nam->post(req, body);
    connect(reply, &QNetworkReply::finished, reply, [reply]() {
        const int status =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (reply->error() != QNetworkReply::NoError) {
            qWarning() << "[mozpush] send failed: status=" << status
                       << reply->errorString();
        } else {
            qDebug() << "[mozpush] send ok: status=" << status;
        }
        reply->deleteLater();
    });
}
