#ifndef CHANNEL_LIST_H
#define CHANNEL_LIST_H

#include "myscrollarea.h"
#include <QskAnimator.h>
#include <QskPaintedNode.h>
#include <QQuickItem>
#include <QColor>
#include <QVector>
#include <QMap>
#include <QPointer>
#include <QPointF>
#include <QHash>

struct ChannelItem {
    QString avatarLetter;
    QColor  avatarColor;
    QString title;
    QString lastMessage;
    QString time;
    int     unreadCount;
    QString chatId;
    QString chatType;
    QString protocol;
};

class ChannelRowNode : public QskPaintedNode
{
public:
    void setItem(const ChannelItem& item);
    void triggerUpdate(QQuickWindow* window, const QRectF& rect, const QSizeF& size);
    void paint(QPainter*, const QSize&, const void*) override;
    QskHashValue hash(const void*) const override;

private:
    ChannelItem m_item;
};

class ChannelRowItem : public QQuickItem
{
public:
    ChannelRowItem(QQuickItem* parent = nullptr);

    void setChannelData(const ChannelItem& item);

protected:
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;

private:
    ChannelItem m_item;
    bool m_dirty = true;
};

class StaggerFadeAnimator;

class ChannelListWidget : public MyScrollArea
{
    Q_OBJECT
public:
    ChannelListWidget(QQuickItem* parent = nullptr);
    ~ChannelListWidget() override;

    void populateData();
    void reloadFromDb();
    void insertOrUpdateChannel(const QString& chatId, const ChannelItem& item);
    QString chatIdForRow(int row) const;
    QString chatTypeForRow(int row) const;
    QString protocolForRow(int row) const;
    int count() const { return m_items.size(); }
    const ChannelItem& channelItem(int row) const {
        static ChannelItem dummy;
        if (row >= 0 && row < m_items.size()) return m_items[row];
        return dummy;
    }

Q_SIGNALS:
    void rowClicked(int row, const QString& chatName);
    void rowLongPressed(int row, const QPointF& scenePos);

protected:
    void geometryChangeEvent(QskGeometryChangeEvent*) override;

private:
    void updateVisibleRows();
    void startFadeIn();
    void appendDbChannels();

    QQuickItem* m_contentView = nullptr;
    QVector<ChannelItem> m_items;
    QHash<QString, int> m_chatIdIndex;
    QMap<int, ChannelRowItem*> m_visibleRows;
    StaggerFadeAnimator* m_fadeAnimator = nullptr;
};

class StaggerFadeAnimator : public QskAnimator
{
public:
    StaggerFadeAnimator(QQuickItem* target, QObject* parent = nullptr);

protected:
    void advance(qreal value) override;
    void done() override;

private:
    QPointer<QQuickItem> m_target;
};

#endif
