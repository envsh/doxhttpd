#include "networkmonitor.h"
#include "mozpushsender.h"
#include <QDebug>
#include <QCoreApplication>
#include <QMutex>
#include <QMutexLocker>

// netstat 去重锁 + lastSent（lastKey 初始为空 → 首个真实回调必广播；
// 无启动预写，符合"只在变化时发送"）
static QMutex s_netStateMutex;
static QString s_lastNetStateKey;

void NetworkMonitor::onNetworkChanged(bool connected, const QString& netType) {
    const QString state = connected ? QStringLiteral("connected")
                                    : QStringLiteral("disconnected");
    const QString key = state + QLatin1Char('/') + netType;
    {
        QMutexLocker lock(&s_netStateMutex);
        if (!s_lastNetStateKey.isEmpty() && key == s_lastNetStateKey) {
            qDebug() << "[NetworkMonitor] netstat dedup, skip:" << key;
            return;
        }
        s_lastNetStateKey = key;
    }
    // sendNetState 碰 QSettings 与 NAM → 一律编组回主线程（qApp 上下文）
    QCoreApplication* app = QCoreApplication::instance();
    if (!app) { return; }
    QMetaObject::invokeMethod(app, [state, netType]() {
        MozpushSender::instance()->sendNetState(state, netType);
    }, Qt::QueuedConnection);
    qDebug() << "[NetworkMonitor] netstat queued:" << key;
}

#if defined(Q_OS_ANDROID)

#include "androidutils.h"
#include <jni.h>
#include <QCoreApplication>
#include <QJniObject>

extern "C" JNIEXPORT void JNICALL
Java_io_fedlet_mobutil_NetworkMonitor_onNetworkChanged(
    JNIEnv* env, jobject /*thiz*/, jboolean isConnected, jstring jNetworkType)
{
    const char* raw = env->GetStringUTFChars(jNetworkType, nullptr);
    QString networkType = QString::fromUtf8(raw);
    env->ReleaseStringUTFChars(jNetworkType, raw);

    QString msg;
    if (isConnected) {
        msg = "网络已连接: " + networkType;
    } else {
        msg = "网络已断开";
    }
    qDebug() << "[NetworkMonitor]" << msg;
    NetworkMonitor::onNetworkChanged(isConnected, networkType);
    showAndroidToast(msg);
}

void NetworkMonitor::checkNetwork()
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
        auto ctx = QNativeInterface::QAndroidApplication::context();
        QJniObject::callStaticMethod<void>(
            "io/fedlet/mobutil/NetworkMonitor",
            "checkCurrentNetwork",
            "(Landroid/content/Context;)V",
            ctx.object());
    });
}

void NetworkMonitor::start()
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
        auto ctx = QNativeInterface::QAndroidApplication::context();
        QJniObject::callStaticMethod<void>(
            "io/fedlet/mobutil/NetworkMonitor",
            "startMonitoring",
            "(Landroid/content/Context;)V",
            ctx.object());
        qDebug() << "[NetworkMonitor] started (Android)";
    });
}

void NetworkMonitor::stop()
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
        auto ctx = QNativeInterface::QAndroidApplication::context();
        QJniObject::callStaticMethod<void>(
            "io/fedlet/mobutil/NetworkMonitor",
            "stopMonitoring",
            "(Landroid/content/Context;)V",
            ctx.object());
        qDebug() << "[NetworkMonitor] stopped";
    });
}

#elif defined(Q_OS_LINUX)

#include <QNetworkInformation>
#include <QProcess>

static void showDesktopNotification(const QString& title, const QString& message) {
    QProcess::startDetached("notify-send", {"-t", "7000", title, message});
}

static QString transportMediumLabel(QNetworkInformation::TransportMedium tm) {
    if (tm == QNetworkInformation::TransportMedium::WiFi) { return QStringLiteral("wifi"); }
    if (tm == QNetworkInformation::TransportMedium::Cellular) { return QStringLiteral("cellular"); }
    if (tm == QNetworkInformation::TransportMedium::Ethernet) { return QStringLiteral("ethernet"); }
    return QString();
}

static QNetworkInformation* s_netInfo = nullptr;

void NetworkMonitor::start()
{
    if (s_netInfo) return;
    if (!QNetworkInformation::loadDefaultBackend()) {
        qWarning() << "[NetworkMonitor] loadDefaultBackend failed (Linux)";
        return;
    }
    s_netInfo = QNetworkInformation::instance();
    QObject::connect(s_netInfo, &QNetworkInformation::reachabilityChanged,
        [](QNetworkInformation::Reachability r) {
            QString msg;
            if (r == QNetworkInformation::Reachability::Online) {
                msg = "网络已连接";
                onNetworkChanged(true, transportMediumLabel(s_netInfo->transportMedium()));
            } else if (r == QNetworkInformation::Reachability::Disconnected) {
                msg = "网络已断开";
                onNetworkChanged(false, QString());
            } else {
                msg = "网络状态未知";
                // Unknown 不广播（仅本地通知保持原状）
            }
            qDebug() << "[NetworkMonitor]" << msg;
            showDesktopNotification("qsktox", msg);
        });
    qDebug() << "[NetworkMonitor] started (Linux)";
}

void NetworkMonitor::stop()
{
    if (!s_netInfo) return;
    QObject::disconnect(s_netInfo, &QNetworkInformation::reachabilityChanged, nullptr, nullptr);
    s_netInfo = nullptr;
    qDebug() << "[NetworkMonitor] stopped (Linux)";
}

#elif defined(Q_OS_WINDOWS)

#include <QNetworkInformation>
#include <QProcess>

static void showDesktopNotification(const QString& title, const QString& message) {
    QString psScript = QString(
        "[Windows.UI.Notifications.ToastNotificationManager, Windows.UI.Notifications, ContentType = WindowsRuntime] | Out-Null; "
        "[Windows.Data.Xml.Dom.XmlDocument, Windows.Data.Xml.Dom.XmlDocument, ContentType = WindowsRuntime] | Out-Null; "
        "$template = @\"<toast duration='long'><visual><binding template='ToastText02'><text id='1'>%1</text><text id='2'>%2</text></binding></visual></toast>\"; "
        "$xml = New-Object Windows.Data.Xml.Dom.XmlDocument; "
        "$xml.LoadXml($template); "
        "$toast = [Windows.UI.Notifications.ToastNotification]::new($xml); "
        "[Windows.UI.Notifications.ToastNotificationManager]::CreateToastNotifier('qsktox').Show($toast);"
    ).arg(title, message);
    QProcess::startDetached("powershell", {"-Command", psScript});
}

static QString transportMediumLabel(QNetworkInformation::TransportMedium tm) {
    if (tm == QNetworkInformation::TransportMedium::WiFi) { return QStringLiteral("wifi"); }
    if (tm == QNetworkInformation::TransportMedium::Cellular) { return QStringLiteral("cellular"); }
    if (tm == QNetworkInformation::TransportMedium::Ethernet) { return QStringLiteral("ethernet"); }
    return QString();
}

static QNetworkInformation* s_netInfo = nullptr;

void NetworkMonitor::start()
{
    if (s_netInfo) return;
    if (!QNetworkInformation::loadDefaultBackend()) {
        qWarning() << "[NetworkMonitor] loadDefaultBackend failed (Windows)";
        return;
    }
    s_netInfo = QNetworkInformation::instance();
    QObject::connect(s_netInfo, &QNetworkInformation::reachabilityChanged,
        [](QNetworkInformation::Reachability r) {
            QString msg;
            if (r == QNetworkInformation::Reachability::Online) {
                msg = "网络已连接";
                onNetworkChanged(true, transportMediumLabel(s_netInfo->transportMedium()));
            } else if (r == QNetworkInformation::Reachability::Disconnected) {
                msg = "网络已断开";
                onNetworkChanged(false, QString());
            } else {
                msg = "网络状态未知";
                // Unknown 不广播（仅本地通知保持原状）
            }
            qDebug() << "[NetworkMonitor]" << msg;
            showDesktopNotification("qsktox", msg);
        });
    qDebug() << "[NetworkMonitor] started (Windows)";
}

void NetworkMonitor::stop()
{
    if (!s_netInfo) return;
    QObject::disconnect(s_netInfo, &QNetworkInformation::reachabilityChanged, nullptr, nullptr);
    s_netInfo = nullptr;
    qDebug() << "[NetworkMonitor] stopped (Windows)";
}

#elif defined(Q_OS_MACOS)

#include <QNetworkInformation>
#include <QProcess>

static void showDesktopNotification(const QString& title, const QString& message) {
    QString script = QString("display notification \"%1\" with title \"%2\"")
        .arg(message, title);
    QProcess::startDetached("osascript", {"-e", script});
}

static QString transportMediumLabel(QNetworkInformation::TransportMedium tm) {
    if (tm == QNetworkInformation::TransportMedium::WiFi) { return QStringLiteral("wifi"); }
    if (tm == QNetworkInformation::TransportMedium::Cellular) { return QStringLiteral("cellular"); }
    if (tm == QNetworkInformation::TransportMedium::Ethernet) { return QStringLiteral("ethernet"); }
    return QString();
}

static QNetworkInformation* s_netInfo = nullptr;

void NetworkMonitor::start()
{
    if (s_netInfo) return;
    if (!QNetworkInformation::loadDefaultBackend()) {
        qWarning() << "[NetworkMonitor] loadDefaultBackend failed (macOS)";
        return;
    }
    s_netInfo = QNetworkInformation::instance();
    QObject::connect(s_netInfo, &QNetworkInformation::reachabilityChanged,
        [](QNetworkInformation::Reachability r) {
            QString msg;
            if (r == QNetworkInformation::Reachability::Online) {
                msg = "网络已连接";
                onNetworkChanged(true, transportMediumLabel(s_netInfo->transportMedium()));
            } else if (r == QNetworkInformation::Reachability::Disconnected) {
                msg = "网络已断开";
                onNetworkChanged(false, QString());
            } else {
                msg = "网络状态未知";
                // Unknown 不广播（仅本地通知保持原状）
            }
            qDebug() << "[NetworkMonitor]" << msg;
            showDesktopNotification("qsktox", msg);
        });
    qDebug() << "[NetworkMonitor] started (macOS)";
}

void NetworkMonitor::stop()
{
    if (!s_netInfo) return;
    QObject::disconnect(s_netInfo, &QNetworkInformation::reachabilityChanged, nullptr, nullptr);
    s_netInfo = nullptr;
    qDebug() << "[NetworkMonitor] stopped (macOS)";
}

#endif
