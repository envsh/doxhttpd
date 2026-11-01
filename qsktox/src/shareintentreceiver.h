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

// 人类可读字节数（B/KB/MB/GB，与导入确认框、消息里的大小文本同一口径）
QString formatShareBytes(qint64 n);

// meta → 联系人选择页 launchArgs（kind/sourceApp/displayName/mime/
// totalBytes/imageCount/messageText）
QVariantMap shareMetaToLaunchArgs(const PendingShareMeta& meta);

// 注册"打开联系人选择页"回调（args = shareMetaToLaunchArgs 产出；
// scan 命中与 flushDeferredShare 两条路径共用）
void registerShareReceiverHost(std::function<void(const QVariantMap& args)> host);

// 冷启动 defer：当前栈顶是 login（未完成登录）时由 main.cpp host 调用，
// 暂存到 QSettings("share_deferred")；MainPage::onCreate 收尾调
// flushDeferredShare() 在 login→main replace 完成后补弹（规避
// pagemanager.cpp replace() 只替换栈顶、把 login 之上的 sharepick 吞掉）
void deferShare(const QVariantMap& args);
void flushDeferredShare();

// 扫描 pending_shares 的 pending_meta.json（冷启动 main.cpp 直调、热启动
// ShareActivity onNewIntent → notifyPendingShare 共用收口）：命中 → 解析 →
// 整目录删除（媒体只发元信息，bin 不再读取）→ host 回调
void scanPendingShareDir();

#endif // SHARE_INTENT_RECEIVER_H
