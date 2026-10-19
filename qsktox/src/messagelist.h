#ifndef MESSAGE_LIST_H
#define MESSAGE_LIST_H

#include "myscrollarea.h"
#include <QskAnimator.h>
#include <QskPaintedNode.h>
#include <QQuickItem>
#include <QColor>
#include <QVector>
#include <QMap>
#include <QPointer>
#include <QPointF>
struct MessageItem {
    enum SendState { SendSending, SendSent, SendFailed };
    QString sender;
    QString content;
    QString time;
    bool    isSelf;
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
};

class MessageRowNode : public QskPaintedNode
{
public:
    void setItem(const MessageItem& item);
    void triggerUpdate(QQuickWindow* window, const QRectF& rect, const QSizeF& size);
    void paint(QPainter*, const QSize&, const void*) override;
    QskHashValue hash(const void*) const override;

private:
    MessageItem m_item;
    QRectF m_statusRect;
};

class MessageRowItem : public QQuickItem
{
public:
    MessageRowItem(QQuickItem* parent = nullptr);

    void setMessageData(const MessageItem& item);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    MessageItem m_item;
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
    void setChannel(const QString& protocol, const QString& chatId);
    int messageCount() const { return m_items.size(); }
    const MessageItem& messageItem(int row) const { return m_items[row]; }
    void setSendStatus(quint64 reqseq, bool success, const QString& error);
    void updateSendState(int row, MessageItem::SendState state,
                         quint64 reqseq, const QString& error);
    bool updateSendStateByLocalId(qint64 localId, MessageItem::SendState state,
                                  const QString& error);
    int64_t rowidForLocalId(qint64 localId) const;
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

    QQuickItem* m_contentView = nullptr;
    QString m_chatId;
    QString m_chanProtocol;
    QVector<MessageItem> m_items;
    QVector<qreal> m_rowHeights;
    QVector<qreal> m_rowYOffsets;
    QMap<int, MessageRowItem*> m_visibleRows;
    MessageListAnimator* m_fadeAnimator = nullptr;
    bool m_rebuildingLayout = false;
};

void rebuildLayout(MessageListWidget*);

class MessageListAnimator : public QskAnimator
{
public:
    MessageListAnimator(QQuickItem* target, QObject* parent = nullptr);

protected:
    void advance(qreal value) override;

private:
    QPointer<QQuickItem> m_target;
};

#endif
