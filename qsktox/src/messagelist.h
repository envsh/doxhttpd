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
};

class MessageRowNode : public QskPaintedNode
{
public:
    MessageRowNode();
    // 头像由 GUI 线程预解析为 QImage（渲染线程禁止构造/使用 QPixmap）
    void setItem(const MessageItem& item, const QImage& avatarImage,
                 quint32 avatarTick);
    void triggerUpdate(QQuickWindow* window, const QRectF& rect, const QSizeF& size);
    void paint(QPainter*, const QSize&, const void*) override;
    QskHashValue hash(const void*) const override;

private:
    MessageItem m_item;
    QImage m_avatarImage;
    quint32 m_avatarTick = 0;
    QRectF m_statusRect;
};

class MessageRowItem : public QQuickItem
{
public:
    MessageRowItem(QQuickItem* parent = nullptr);

    void setMessageData(const MessageItem& item);
    void setMessageDataNoUpdate(const MessageItem& item);
    // 只在 GUI 线程调用：把 AvatarManager 的 QPixmap 转成 QImage 缓存，
    // 并递增 avatarTick 参与 hash，保证下载完成后精准重绘。
    void refreshAvatar();

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    MessageItem m_item;
    QImage m_avatarImage;
    quint32 m_avatarTick = 0;
    bool m_avatarResolved = false;
    bool m_dirty = true;
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
    void retryRequested(int row);

protected:
    void geometryChangeEvent(QskGeometryChangeEvent*) override;

private:
    friend void rebuildLayout(MessageListWidget*);

    int rowFromPosition(const QPointF& localPos) const;
    int rowFromContentY(qreal contentY) const;
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
