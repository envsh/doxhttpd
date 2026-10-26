#include "androidutils.h"
#include <QCoreApplication>
#include <QDebug>
#include <QHostInfo>

#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <android/log.h>
#include <cstdio>

static void logHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    const char *file = ctx.file ? ctx.file : "unknown";
    int line = ctx.line;
    int prio = ANDROID_LOG_DEBUG;
    switch (type) {
        case QtDebugMsg:    prio = ANDROID_LOG_DEBUG; break;
        case QtInfoMsg:     prio = ANDROID_LOG_INFO; break;
        case QtWarningMsg:  prio = ANDROID_LOG_WARN; break;
        case QtCriticalMsg: prio = ANDROID_LOG_ERROR; break;
        case QtFatalMsg:    prio = ANDROID_LOG_FATAL; break;
    }
    __android_log_print(prio, "qsktox", "%s:%d %s", file, line, msg.toUtf8().constData());
}
#else
#include <cstdio>

static void logHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    const char *file = ctx.file ? ctx.file : "unknown";
    int line = ctx.line;
    FILE *out = (type == QtWarningMsg || type == QtCriticalMsg || type == QtFatalMsg)
                ? stderr : stdout;
    fprintf(out, "%s:%d %s\n", file, line, msg.toUtf8().constData());
    fflush(out);
    if (type == QtFatalMsg) abort();
}
#endif

void installLogHandler()
{
    qInstallMessageHandler(logHandler);
}

#ifdef Q_OS_ANDROID
#include <QJniObject>

void showAndroidToast(const QString& message) {
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([message]() {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (!context.isValid()) return;
        QJniObject jmsg = QJniObject::fromString(message);
        QJniObject toast = QJniObject::callStaticObjectMethod(
            "android/widget/Toast",
            "makeText",
            "(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;",
            context.object(), jmsg.object(), 1);
        if (toast.isValid()) {
            toast.callMethod<void>("show", "()V");
        }
    });
}
#else
void showAndroidToast(const QString& message) {
    Q_UNUSED(message)
}
#endif

#ifdef Q_OS_ANDROID
#include <QJniObject>

QString androidDeviceName()
{
    QString name;
    auto future = QNativeInterface::QAndroidApplication::runOnAndroidMainThread([&name]() {
        QJniObject ctx = QNativeInterface::QAndroidApplication::context();
        if (!ctx.isValid()) {
            return;
        }
        QJniObject result = QJniObject::callStaticObjectMethod(
            "io/fedlet/mobutil/PermissionHelper",
            "getDeviceName",
            "(Landroid/app/Activity;)Ljava/lang/String;",
            ctx.object());
        if (result.isValid()) {
            name = result.toString();
        }
    });
    future.waitForFinished();
    if (name.isEmpty()) {
        name = QHostInfo::localHostName();
    }
    if (name.isEmpty()) {
        name = QStringLiteral("android");
    }
    return name;
}

void androidRequestBluetoothNamePermission()
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([]() {
        QJniObject ctx = QNativeInterface::QAndroidApplication::context();
        if (!ctx.isValid()) {
            return;
        }
        QJniObject::callStaticMethod<void>(
            "io/fedlet/mobutil/PermissionHelper",
            "requestBluetoothConnectPermission",
            "(Landroid/app/Activity;)V",
            ctx.object());
    });
}

void shareText(const QString& text)
{
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([text]() {
        QJniObject ctx = QNativeInterface::QAndroidApplication::context();
        if (!ctx.isValid()) {
            return;
        }
        QJniObject intent("android/content/Intent");
        intent.callObjectMethod("setAction",
            "(Ljava/lang/String;)Landroid/content/Intent;",
            QJniObject::fromString("android.intent.action.SEND").object());
        intent.callObjectMethod("setType",
            "(Ljava/lang/String;)Landroid/content/Intent;",
            QJniObject::fromString("text/plain").object());
        intent.callObjectMethod("putExtra",
            "(Ljava/lang/String;Ljava/lang/CharSequence;)Landroid/content/Intent;",
            QJniObject::fromString("android.intent.extra.TEXT").object(),
            QJniObject::fromString(text).object());
        QJniObject chooser = QJniObject::callStaticObjectMethod(
            "android/content/Intent", "createChooser",
            "(Landroid/content/Intent;Ljava/lang/CharSequence;)Landroid/content/Intent;",
            intent.object(), QJniObject::fromString("分享").object());
        ctx.callMethod<void>("startActivity",
            "(Landroid/content/Intent;)V", chooser.object());
    });
}

#else

QString androidDeviceName()
{
    return QHostInfo::localHostName();
}

void androidRequestBluetoothNamePermission()
{
}

void shareText(const QString& text)
{
    Q_UNUSED(text)
}

#endif
