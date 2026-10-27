#ifndef MESSAGE_LIST_H
#define MESSAGE_LIST_H

#include "myscrollarea.h"
#include <QskAnimator.h>
#include <QskPaintedNode.h>
#include <QQuickItem>
#include <QImage>
#include <QColor>
#include <QVector>
#include <QMap>
#include <QPointer>
#include <QPointF>
#include <QSet>
#include <QTimer>
// GIF 逐帧动画（qltox QMovie 对位）：仅指针成员，头文件不引重头
class QMovie;
class QBuffer;
struct MessageItem {
    enum SendState { SendSending, SendSent, SendFailed };
    QString sender;
    QString content;
    QString time;
    bool    isSelf = false;
    QString avatarUrl;
    int     peerNumber = -1;
    QString avatarLetter;
    QColor  avatarColor;
    SendState sendState = SendSending;
    QString sendError;
    quint64 reqSeq = 0;
    qint64  localId = 0;
    int64_t dbRowid = 0;
    bool    isAudio = false;
    QString mediaUrl;
    QString audioMime;
    int     durationSec = 0;
    // 引用/提及：push 载荷字段回程解析（reply_tos=matrix event id 列表，
    // mentions=@user 列表，元素原文照存；DB 侧逗号 join 存既有 TEXT 列）
    QStringList replyTos;
    QStringList mentions;
    // == 接收字段（纯内存，push 原始载荷保真记录；重启/翻历史重载后不保留
    //    → 查看原始数据显示 "—"。不加 DB 列，data 列语义保留给正文/FTS） ==
    QString protocol;
    QString chatId;
    QString chatName;
    QString chatType;
    QString accountId;
    QString accountName;
    QString username;    // push username 原文（sender 为 usernick 为空时的折叠值）
    QString userId;      // push userid（发送者 id）
    QString usernick;    // push usernick 原文
    QString chanIcon;
    QString msgType;     // push msgtype
    QString msgFormat;   // push msgformat
    QString msgId;       // push msgid（Matrix $id 原文）
    qint64  timestampMs = 0;  // push timestamp 原值（含纳秒档）

    // == 媒体展示字段（纯内存，重启退化占位，与接收字段同口径）==
    // 对位 qltox ChatElement（chatview.h:93 mediaWidth 等），不落 DB。
    QString mediaMime;                // image/gif 识别、file 型 mimetype
    QString fileName;                 // file 型文件名
    QString fileSizeStr;              // 人工可读大小（qltox formatFileSize 口径）
    qint64  mediaWidth = 0;           // 图片/视频原始宽
    qint64  mediaHeight = 0;          // 原始高
    QString localPath;                // 本地可读源（P2 真绘唯一前提）
    // == 下载状态（纯内存，qltox DownloadState 四态口径；重启清零）==
    // 状态条覆盖媒体框底部 22px（NotRequested/Failed 可点 → tap 触发下载）
    int     downloadState = 0;     // 0=NotRequested 1=InProgress 2=Completed 3=Failed
    int     downloadProgress = 0;  // 0-100；-1=总长未知（显示 …）
    int     downloadSpeedBps = 0;  // 下载速率（mediaBarSpeedText 显示用；非进度更新归零）
    qint64  fileSizeBytes = 0;     // 原始字节（<1MB 自动预取判定 + 状态条大小后缀）
    QString thumbUrl;              // CS info.thumbnail_url（视频缩略图源）
    QString errorMsg;              // 下载失败详情（qltox mediaErrorMsg 触摸端口径：
                                   // 无 tooltip，Failed 状态条内直出，单行自动裁剪）
};

class MessageRowNode : public QskPaintedNode
{
public:
    MessageRowNode();
    // 头像与媒体本地源均由 GUI 线程预解码为 QImage（渲染线程禁 QPixmap）
    void setItem(const MessageItem& item, const QImage& avatarImage,
                 quint32 avatarTick, const QImage& mediaImage,
                 quint32 mediaTick);
    void triggerUpdate(QQuickWindow* window, const QRectF& rect, const QSizeF& size);
    void paint(QPainter*, const QSize&, const void*) override;
    QskHashValue hash(const void*) const override;

private:
    MessageItem m_item;
    QImage m_avatarImage;
    quint32 m_avatarTick = 0;
    QImage m_mediaImage;     // P2 本地媒体源预解码（GUI 线程已就绪的 QImage）
    quint32 m_mediaTick = 0;
    QRectF m_statusRect;
public:
    // header 右上角三钮锚点（空操作，本次仅绘制；后续补点击时直接消费）
    QRectF m_sourceRect;    // 📋 原文
    QRectF m_translateRect; // 🌐 翻译
    QRectF m_speakRect;     // 🔊 朗读（正文为空时置空）
};

class MessageRowItem : public QQuickItem
{
public:
    MessageRowItem(QQuickItem* parent = nullptr);
    ~MessageRowItem() override;

    void setMessageData(const MessageItem& item);
    void setMessageDataNoUpdate(const MessageItem& item);
    // 只在 GUI 线程调用：把 AvatarManager 的 QPixmap 转成 QImage 缓存，
    // 并递增 avatarTick 参与 hash，保证下载完成后精准重绘。
    void refreshAvatar();

    // 媒体真绘源解码 GUI 线程口径见 cpp；递增 mediaTick 参与 hash 保证重绘。
    void refreshMedia();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    MessageItem m_item;
    QImage m_avatarImage;
    quint32 m_avatarTick = 0;
    QImage m_mediaImage;
    QString m_mediaSrc;
    quint32 m_mediaTick = 0;
    bool m_avatarResolved = false;
    bool m_dirty = true;
    // GIF 逐帧动画（qltox QMovie 对位）：仅指针成员，头文件不引重头；
    // 帧变化只换 QImage+tick，渲染线程侧复用现有 hash 重绘门
    void setupGifMovie(const QString& src);
    void stopGifMovie();
    QMovie* m_gifMovie = nullptr;
    QBuffer* m_gifBuf = nullptr;
    QByteArray m_gifData;
    QString m_gifSrc;              // 当前动画源（相同则不重建）
};

class MessageRowItem;
class MessageListAnimator;

class MessageListWidget : public MyScrollArea
{
    Q_OBJECT
public:
    MessageListWidget(QQuickItem* parent = nullptr);
    ~MessageListWidget() override;

    void populateMessages();
    void appendMessage(const MessageItem& item);
    void setChannel(const QString& protocol, const QString& chatId,
                    const QString& chanId = QString());
    // 强制滚动到底部（切换频道后用于对齐 X14）
    void scrollToBottom();
    void stopFadeIn();
    int messageCount() const { return m_items.size(); }
    // 已创建的可见行数（回归测试用：换频道后必须 > 0，否则消息列表整表空白）
    int visibleRowCount() const { return m_visibleRows.size(); }
    // 越界安全：旧实现直接 m_items[row]，列表被清空/回填后调用方持有的
    // 旧下标会越界崩溃。
    const MessageItem& messageItem(int row) const {
        static const MessageItem s_empty;
        if (row < 0 || row >= m_items.size()) {
            return s_empty;
        }
        return m_items[row];
    }
    void setSendStatus(quint64 reqseq, bool success, const QString& error);
    void updateSendState(int row, MessageItem::SendState state,
                         quint64 reqseq, const QString& error);
    bool updateSendStateByLocalId(qint64 localId, MessageItem::SendState state,
                                  const QString& error);
    int64_t rowidForLocalId(qint64 localId) const;
    int64_t lastRowid() const;
    void backfillRowid(qint64 localId, int64_t rowid);
    void updateSendStateByRowid(int64_t rowid, MessageItem::SendState state);

Q_SIGNALS:
    void rowClicked(int row);
    void rowLongPressed(int row, const QPointF& scenePos);
    void rowDoubleClicked(int row);
    void avatarClicked(int row, const QPointF& pos);
    void avatarLongPressed(int row, const QPointF& scenePos);
    void retryRequested(int row);

protected:
    void geometryChangeEvent(QskGeometryChangeEvent*) override;

private:
    friend void rebuildLayout(MessageListWidget*);
    void kickMediaPrefetch(const MessageItem& item);

    int rowFromPosition(const QPointF& localPos) const;
    int rowFromContentY(qreal contentY) const;
    bool isAvatarHit(int row, qreal contentX, qreal contentY) const;
    void updateVisibleRows();
    void onScrollPosChangedForTracking();
    void flushPendingVisibleRefresh();
    void flushCoalescedRefresh();
    void queueVisibleRefresh(int idx);
    void forceClipRefresh();
    // 淡入计时器超时槽（构造时连接一次，避免重复 connect 累积）
    void onFadeTimeout();

    QQuickItem* m_contentView = nullptr;
    QString m_chatId;
    QString m_chanProtocol;
    QString m_chanId;
    QVector<MessageItem> m_items;
    QVector<qreal> m_rowHeights;
    QVector<qreal> m_rowYOffsets;
    QMap<int, MessageRowItem*> m_visibleRows;
    MessageListAnimator* m_fadeAnimator = nullptr;
    // 淡入延迟计时器：可被 stopFadeIn() 取消，避免页面切走后仍启动动画
    QTimer m_fadeTimer;
    bool m_rebuildingLayout = false;
    // 滚动中延迟可见行视觉刷新，避开 QskScrollArea ClipItem ContentUpdateMask defer
    bool m_isScrolling = false;
    bool m_flushPending = false;
    QTimer m_scrollIdleTimer;
    QTimer m_coalesceTimer;
    QSet<int> m_pendingVisibleRefresh;
    QSet<int> m_coalescedRefresh;
};

void rebuildLayout(MessageListWidget*);

class MessageListAnimator : public QskAnimator
{
public:
    MessageListAnimator(QQuickItem* target, QObject* parent = nullptr);

protected:
    void setup() override;
    void advance(qreal value) override;
    void done() override;

private:
    QPointer<QQuickItem> m_target;
};

#endif
