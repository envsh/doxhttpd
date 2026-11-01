#ifndef SHARE_INTENT_RECEIVER_H
#define SHARE_INTENT_RECEIVER_H

#include <functional>
#include <QString>
#include <QStringList>
#include <QVariantMap>

// ── 分享元信息（Java 端 ShareReceiverActivity 落盘 pending_meta.json）──
struct PendingShareMeta {
    QString action;
    QString mime;
    QString text;
    QString sourceApp;          // 来源 app 全包名
    int imageCount = 0;
    qint64 receivedAt = 0;      // 接收时间（epoch ms）
    qint64 totalBytes = 0;      // 落盘字节总和
    QString displayName;        // 首文件原始文件名
    QString pendingDir;         // scan 命中的 pending_shares 目录
    QStringList files;          // meta["files"]：落盘 bin 文件名清单（保序）
};

// 出站只发元信息：把分享整理成一条文本消息（文本原样转发；图片/文件只带
// 文件名/个数/总大小元信息，不携带内容本身）
QString shareMetaToMessage(const PendingShareMeta& meta);

// meta → 联系人选择页 launchArgs（kind/sourceApp/displayName/mime/
// totalBytes/imageCount/messageText）
QVariantMap shareMetaToLaunchArgs(const PendingShareMeta& meta);

// 注册"打开联系人选择页"回调（分享扫描命中后在主线程调用）
void registerShareReceiverHost(std::function<void(const PendingShareMeta&)> host);

// 扫描 pending_shares 的 pending_meta.json（冷启动 main.cpp 直调、热启动
// ShareActivity onNewIntent → notifyPendingShare 共用收口）：命中 → 解析 →
// 整目录删除（媒体只发元信息，bin 不再读取）→ host 回调
void scanPendingShareDir();

#endif // SHARE_INTENT_RECEIVER_H
