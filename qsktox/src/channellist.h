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
#include <QSet>
#include <QTimer>

struct ChannelItem {
    QString avatarLetter;
    QColor  avatarColor;
    QString title;
    QString lastMessage;
    QString time;
    int     unreadCount = 0;    // 显示用徽标数（= effectiveUnread()）
    QString chatId;
    QString chatType;
    QString protocol;
    QString chanId;          // channels 表主键（权威会话标识，可为空）
    // ── 以下字段放在末尾：s_mockData 依赖既有聚合初始化顺序 ──
    int     dbUnreadCount = 0;  // DB channels.unread_count 镜像（唯一真源）
    int     readWatermark = 0;  // 本地已读、DB 尚未确认清零的条数
    int     unreadDelta = 0;    // 本次事件新增的未读条数（推送到达 +1）
    qint64  lastActive = 0;     // 排序键：最后活跃时间（epoch 秒），与 DB last_active 同源

    // 徽标 = DB 未读 - 本地已读水位 + 尚未落库的本地增量。
    // DB 是唯一真源：UI 只做乐观显示（点开会话时先隐藏徽标），
    // DB 写入确认后由 UnreadNotifier 通知重新同步。
    int effectiveUnread() const
    {
        return qMax(0, dbUnreadCount - readWatermark) + unreadDelta;
    }
    void syncUnread() { unreadCount = effectiveUnread(); }
};

class ChannelRowNode : public QskPaintedNode
{
public:
    ChannelRowNode();
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
    void setChannelDataNoUpdate(const ChannelItem& item);

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
    void stopFadeIn();
    void insertOrUpdateChannel(const QString& chatId, const ChannelItem& item);
    QString chatIdForRow(int row) const;
    QString chatTypeForRow(int row) const;
    QString protocolForRow(int row) const;
    QString chanIdForRow(int row) const {
        if (row >= 0 && row < m_items.size()) {
            return m_items[row].chanId;
        }
        return {};
    }
    int count() const { return m_items.size(); }
    const ChannelItem& channelItem(int row) const {
        static ChannelItem dummy;
        if (row >= 0 && row < m_items.size()) return m_items[row];
        return dummy;
    }

    // 乐观清零：用户点开会话时立即隐藏徽标（DB 写入确认前）
    void markLocallyRead(const QString& protocol, const QString& chatId,
                         const QString& chanId);
    // 从 DB 重新同步全部会话的未读数（回到列表时调用）
    void reloadUnreadFromDb();
    // 从 DB 同步单个会话的未读数（DB 写入完成后调用）
    void syncUnreadFromDb(const QString& chanid);
    // 刷新指定行的显示内容（未读徽标变化后）
    void refreshRow(int index);
    // 会话有新活动（收到/发出消息）：更新排序键并重排（该会话上浮）
    void markChannelActive(const QString& protocol, const QString& chatId,
                           qint64 lastActive);

Q_SIGNALS:
    void rowClicked(int row, const QString& chatName);
    void rowLongPressed(int row, const QPointF& scenePos);
    void avatarClicked(int row, const QPointF& scenePos);
    void avatarLongPressed(int row, const QPointF& scenePos);

protected:
    void geometryChangeEvent(QskGeometryChangeEvent*) override;

private:
    void updateVisibleRows();
    bool isAvatarHit(int row, const QPointF& contentViewPos) const;
    void startFadeIn();
    // 淡入计时器超时槽（构造时连接一次）
    void onFadeTimeout();
    void appendDbChannels();
    // 排序：虚拟联系人置顶 → 真实会话按 lastActive 倒序 → mock 演示会话垫底
    // （与 DB 的 ORDER BY last_active DESC 一致）
    void sortChannels();
    // 重建行号索引（排序/插入后行号会变，索引必须同步）
    void rebuildIndexes();
    // 会话行定位：权威 chanid → protocol+chatId → chatId 唯一兜底
    int indexOfChannel(const QString& protocol, const QString& chatId,
                       const QString& chanId) const;
    // 滚动状态追踪：滚动中延迟可见行刷新，避开 QskScrollArea ClipItem 竞态
    void onScrollPosChangedForTracking();
    void flushPendingVisibleRefresh();
    void flushCoalescedRefresh();
    void queueVisibleRefresh(int idx);
    void forceClipRefresh();

    QQuickItem* m_contentView = nullptr;
    QVector<ChannelItem> m_items;
    QHash<QString, int> m_chanKeyIndex;
    // chatId → 行号列表（同一 chatId 可能出现在多个 protocol 下）
    QHash<QString, QVector<int>> m_chatIdRows;
    QMap<int, ChannelRowItem*> m_visibleRows;
    StaggerFadeAnimator* m_fadeAnimator = nullptr;
    // 淡入延迟计时器：可被 stopFadeIn() 取消，避免页面切走后仍启动动画
    QTimer m_fadeTimer;
    // 滚动中更新延迟
    bool m_isScrolling = false;
    QTimer m_scrollIdleTimer;
    // 全量合并：不管是否滚动都合并可见行视觉刷新（应对高频更新）
    QTimer m_coalesceTimer;
    QSet<int> m_pendingVisibleRefresh;
    QSet<int> m_coalescedRefresh;
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
