#ifndef MEDIA_PREVIEW_BRIDGE_H
#define MEDIA_PREVIEW_BRIDGE_H

#include <QQuickItem>

struct MessageItem;

// qsktox → qskcomp MediaViewerPopup 薄壳：
//   - MediaHost 取字节（GUI 线程同步 getBytes / localPath QFile 直读）
//   - 字节缺 → pending 打开 + download + mediaUpdated 到达自动递料
//   - GIF 链以 msgMediaTypeOf 分类结果为准（signal 传入，不二次判）
// 组件本体（缩放/平移/双击/GIF 逐帧/Esc/Back 关闭）在 qskcomp/mediaviewer。
namespace MediaPreview {
// mediaType = MsgMediaType 枚举 int 值（messagelist.h:43）
void showFor(QQuickItem* parent, const MessageItem& item, int mediaType);
// Video 型分壳（同一 resolveBytes/pending watcher 基建，组件换 videoviewer）：
// 字节齐 → 自动播放；缺 → pending + download + mediaUpdated 递料自动播放
void showVideoFor(QQuickItem* parent, const MessageItem& item);
}

#endif // MEDIA_PREVIEW_BRIDGE_H
