#include "shareintentreceiver.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QMetaObject>
#include <QSettings>
#include <QStandardPaths>
#include <QThread>
#include <functional>

namespace {

std::function<void(const QVariantMap& args)> s_host;

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

QString formatShareBytes(qint64 n)
{
    if (n < 1024) {
        return QStringLiteral("%1 B").arg(n);
    }
    if (n < 1024 * 1024) {
        return QStringLiteral("%1 KB").arg(n / 1024.0, 0, 'f', 1);
    }
    if (n < 1024LL * 1024 * 1024) {
        return QStringLiteral("%1 MB").arg(n / (1024.0 * 1024), 0, 'f', 1);
    }
    return QStringLiteral("%1 GB").arg(n / (1024.0 * 1024 * 1024), 0, 'f', 2);
}

QString shareMetaToMessage(const PendingShareMeta& meta)
{
    const int count = meta.files.size();
    const bool imageIntent = meta.mime.startsWith(QStringLiteral("image/"));
    if (count == 0) {
        // 纯文本分享 → 原样转发；有文件意图但全部读取失败 → 元信息兜底
        if (!meta.text.isEmpty()) {
            return meta.text;
        }
        return imageIntent
            ? QString::fromUtf8("📷 图片（内容未能读取）")
            : QString::fromUtf8("📎 文件（内容未能读取）");
    }
    const QString icon = imageIntent
        ? QString::fromUtf8("📷") : QString::fromUtf8("📎");
    QString msg = icon + QLatin1Char(' ') + meta.displayName;
    if (count > 1) {
        msg += QString::fromUtf8(" 等 %1 个").arg(count);
    }
    if (meta.totalBytes > 0) {
        msg += QString::fromUtf8(" · %1").arg(formatShareBytes(meta.totalBytes));
    }
    return msg;
}

QVariantMap shareMetaToLaunchArgs(const PendingShareMeta& meta)
{
    const bool imageIntent = meta.mime.startsWith(QStringLiteral("image/"));
    QString kind;
    if (!meta.files.isEmpty()) {
        kind = imageIntent ? QStringLiteral("image") : QStringLiteral("file");
    } else if (!meta.text.isEmpty()) {
        kind = QStringLiteral("text");
    } else {
        kind = imageIntent ? QStringLiteral("image") : QStringLiteral("file");
    }
    QVariantMap out;
    out.insert(QStringLiteral("kind"), kind);
    out.insert(QStringLiteral("sourceApp"), meta.sourceApp);
    out.insert(QStringLiteral("displayName"), meta.displayName);
    out.insert(QStringLiteral("mime"), meta.mime);
    out.insert(QStringLiteral("totalBytes"), meta.totalBytes);
    out.insert(QStringLiteral("imageCount"), meta.imageCount);
    out.insert(QStringLiteral("receivedAt"), meta.receivedAt);
    out.insert(QStringLiteral("messageText"), shareMetaToMessage(meta));
    return out;
}

void registerShareReceiverHost(std::function<void(const QVariantMap& args)> host)
{
    s_host = std::move(host);
}

void deferShare(const QVariantMap& args)
{
    QSettings().setValue(QStringLiteral("share_deferred"), args);
    qDebug() << "[shareintentreceiver] deferred until main page is up";
}

void flushDeferredShare()
{
    const QVariantMap args =
        QSettings().value(QStringLiteral("share_deferred")).toMap();
    if (args.isEmpty()) {
        return;
    }
    QSettings().remove(QStringLiteral("share_deferred"));
    if (s_host) {
        s_host(args);
    }
}

void scanPendingShareDir()
{
    auto* app = QCoreApplication::instance();
    if (!app) {
        return;
    }

    // Java 端用 context.getFilesDir()/pending_shares 落盘。Qt 的
    // AppDataLocation 在 Android 上的具体映射与 getFilesDir() 存在歧义，
    // 因此探测两个候选路径（anystik same 口径）。
    const QString base = QStandardPaths::writableLocation(
        QStandardPaths::AppDataLocation);
    const QStringList candidates{
        base + QStringLiteral("/pending_shares"),
        base + QStringLiteral("/files/pending_shares"),
    };

    QString dir;
    for (const auto& c : candidates) {
        if (QFile::exists(c + QStringLiteral("/pending_meta.json"))) {
            dir = c;
            break;
        }
    }
    if (dir.isEmpty()) {
        return;
    }

    QFile metaFile(dir + QStringLiteral("/pending_meta.json"));
    if (!metaFile.open(QIODevice::ReadOnly)) {
        qWarning() << "[shareintentreceiver] open meta failed:" << dir;
        return;
    }
    const QByteArray raw = metaFile.readAll();
    metaFile.close();
    if (raw.isEmpty()) {
        return;
    }

    QJsonParseError parseErr{};
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[shareintentreceiver] meta parse error:"
                   << parseErr.errorString();
        return;
    }

    const QJsonObject obj = doc.object();
    PendingShareMeta meta;
    meta.action      = obj.value(QStringLiteral("action")).toString();
    meta.mime        = obj.value(QStringLiteral("mime")).toString();
    meta.text        = obj.value(QStringLiteral("text")).toString();
    meta.sourceApp   = obj.value(QStringLiteral("sourceApp")).toString();
    meta.imageCount  = obj.value(QStringLiteral("imageCount")).toInt();
    meta.receivedAt  = obj.value(QStringLiteral("receivedAt")).toVariant().toLongLong();
    meta.totalBytes  = obj.value(QStringLiteral("totalBytes")).toVariant().toLongLong();
    meta.displayName = obj.value(QStringLiteral("displayName")).toString();
    meta.pendingDir  = dir;
    const QJsonArray filesArr = obj.value(QStringLiteral("files")).toArray();
    for (const QJsonValue& v : filesArr) {
        const QString n = v.toString();
        if (!n.isEmpty()) {
            meta.files << n;
        }
    }
    if (meta.sourceApp.isEmpty()) {
        meta.sourceApp = QString::fromUtf8("未知来源");
    }

    // 消费即清理：出站只发元信息，落盘 bin 不再被任何路径读取
    if (!QDir(dir).removeRecursively()) {
        qWarning() << "[shareintentreceiver] cleanup pending failed:" << dir;
    }

    qDebug() << "[shareintentreceiver] share consumed"
             << "kind:" << meta.mime
             << "count:" << meta.files.size()
             << "source:" << meta.sourceApp
             << "bytes:" << meta.totalBytes;

    PendingShareMeta ready = std::move(meta);
    runOnMainThread([ready]() {
        if (s_host) {
            s_host(shareMetaToLaunchArgs(ready));
        }
    });
}

#if defined(Q_OS_ANDROID)
#include <jni.h>

// 热启动通知：receiver 已落盘分享，扫描 pending_shares
extern "C" JNIEXPORT void JNICALL
Java_io_fedlet_mobutil_ShareActivity_notifyPendingShare(
    JNIEnv* /*env*/, jobject /*thiz*/)
{
    runOnMainThread([]() { scanPendingShareDir(); });
}
#endif // Q_OS_ANDROID
