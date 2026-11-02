#include "pickmedianav.h"
#include "shareintentreceiver.h"
#include <QCoreApplication>
#include <QDebug>
#include <QThread>

namespace {

std::function<void(const PendingPickMeta&)> s_host;

// shareintentreceiver.cpp:22-30 同款主线程收口
void runOnMainThread(std::function<void()> fn)
{
    auto* app = QCoreApplication::instance();
    if (!app) {
        return;
    }
    if (QThread::currentThread() == app->thread()) {
        fn();
    } else {
        QMetaObject::invokeMethod(app, std::move(fn), Qt::QueuedConnection);
    }
}

} // namespace

void registerPickNotifyHost(std::function<void(const PendingPickMeta&)> host)
{
    s_host = std::move(host);
}

void startFilePick(const QString& kind)
{
#if defined(Q_OS_ANDROID)
    // androidutils.cpp shareText 同款：GUI 线程 JNI（自动附加），context 直调
    QNativeInterface::QAndroidApplication::runOnAndroidMainThread([kind]() {
        QJniObject ctx = QNativeInterface::QAndroidApplication::context();
        if (!ctx.isValid()) {
            return;
        }
        QJniObject::callStaticMethod<void>(
            "io/fedlet/mobutil/PickMediaActivity",
            "start",
            "(Landroid/content/Context;Ljava/lang/String;)V",
            ctx.object(), QJniObject::fromString(kind).object());
    });
#else
    Q_UNUSED(kind)   // x64 桌面无 SAF/无 QtWidgets：占位，菜单侧 toast
#endif
}

QString pickMetaToMessage(const PendingPickMeta& meta)
{
    // 复用 share 元信息 → 文案（同款格式/字节数/兜底口径）
    PendingShareMeta sm;
    sm.mime = meta.mime;
    sm.displayName = meta.displayName;
    sm.totalBytes = meta.size;
    if (!meta.displayName.isEmpty()) {
        sm.files << meta.displayName;
    }
    return shareMetaToMessage(sm);
}

#if defined(Q_OS_ANDROID)
#include <QJniObject>
#include <jni.h>

// Java 的 modified UTF-8 → QString：ASCII/CJK 名称均可解
static QString jstrToQString(JNIEnv* env, jstring s)
{
    if (!s) {
        return QString();
    }
    const char* utf = env->GetStringUTFChars(s, nullptr);
    if (!utf) {
        return QString();
    }
    const QString out = QString::fromUtf8(utf);
    env->ReleaseStringUTFChars(s, utf);
    return out;
}

// PickMediaActivity.onActivityResult 取齐元信息后的 JNI 通知
//（ShareActivity notifyPendingShare 同款 static native 收口）
extern "C" JNIEXPORT void JNICALL
Java_io_fedlet_mobutil_PickMediaActivity_notifyFilePicked(
    JNIEnv* env, jobject /*thiz*/, jstring name, jstring mime, jlong size)
{
    PendingPickMeta meta;
    meta.displayName = jstrToQString(env, name);
    meta.mime = jstrToQString(env, mime);
    meta.size = (qint64)size;
    runOnMainThread([meta]() {
        if (s_host) {
            s_host(meta);
        }
    });
}
#endif // Q_OS_ANDROID
