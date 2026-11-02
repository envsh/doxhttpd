#ifndef PICK_MEDIA_NAV_H
#define PICK_MEDIA_NAV_H

#include <functional>
#include <QString>

// 📎 附件选择（Android SAF 元信息探测；x64 空实现）
// PickMediaActivity.java 只取 名字/类型/大小（零内容拷贝），JNI 通知回 Qt，
// 文案与外发 share 同款（pickMetaToMessage → shareMetaToMessage），
// 发送只发元信息文本（MozpushSender 普通文本消息管线）。
struct PendingPickMeta {
    QString displayName;
    QString mime;
    qint64 size = 0;
};

// kind: "image" / "video" / "audio" / "file"（PickMediaActivity.java 同表）；
// 非 Android 平台为空实现（messagepage 桌面菜单保持占位 toast）。
void startFilePick(const QString& kind);

// PickMediaActivity.onActivityResult → notifyFilePicked JNI → host（主线程）
void registerPickNotifyHost(std::function<void(const PendingPickMeta&)> host);

// 元信息 → 消息文案（shareMetaToMessage 同款口径：
// "📷 x.png · 12 KB" / "📎 x.pdf · 1.5 MB"，内容不可读兜底同源）
QString pickMetaToMessage(const PendingPickMeta& meta);

#endif // PICK_MEDIA_NAV_H
