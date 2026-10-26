#ifndef MSGDB_BRIDGE_H
#define MSGDB_BRIDGE_H

#include "messagelist.h"
#include <QObject>
#include <QString>
#include <QVector>

namespace MsgDbBridge {

// 未读数变更通知。
// DB（channels.unread_count）是唯一真源：UI 侧的徽标只是乐观显示，
// DB 写入完成（成功或失败）后由本通知把真实值同步回列表。
class UnreadNotifier : public QObject
{
    Q_OBJECT
public:
    static UnreadNotifier& instance();
    void notifyUnreadChanged(const QString& chanid);
Q_SIGNALS:
    void unreadChanged(const QString& chanid);
};

struct ChannelRecord {
    QString protocol;
    QString chatId;
    QString chanId;          // channels 表主键（权威会话标识）
    QString name;
    QString status;
    QString icon;            // channels.icon_url（chan_icon 持久化；空=无/沿用旧值）
    QString lastMessageText;
    QString lastMessageTime;
    int     unreadCount = 0;
    qint64  lastMessageRowid = 0;
    qint64  lastReadRowid = 0;
    qint64  lastActive = 0;
    bool    isConnected = false;
};

QString chanKey(const QString& protocol, const QString& chatId);

// 权威 chanid：channels 表里该会话真实的主键。
// chanid 形如 "<proto>_<chatId>"，但推送侧 protocol 与库内 proto_type 可能
// 不一致（协议别名 / 历史数据），此时拼出来的 key 命中不到任何行：消息会
// 写进另一个会话，未读也清不掉。统一以库内已有记录为准。
// 库内同一 chatId 存在多条时不做猜测，避免把不同协议的同名会话合并。
QString resolveChanid(const QString& protocol, const QString& chatId);

// 进程实例标签：MessageItem.localId 是进程内自增，桌面端与 Go 服务会各自
// 产生相同的 localId；event_id 带上实例标签后跨进程唯一。
// 生产环境自动生成（UUID），测试可显式设置以模拟不同进程实例。
void setInstanceTag(const QString& tag);
QString instanceTag();

void ensureChannel(const QString& protocol, const QString& chatId,
                   const QString& name);

QVector<ChannelRecord> loadChannels();

// chan_icon URL 落库（push 随消息送达时调用；空 icon 由函数自身跳过）。
// 复用 update_contact_channel 列级空值保护：仅 icon_url 非空时覆盖，
// 其余字段保持旧值（SQL CASE，channel_db.cpp:364）；新会话 insert 首建即带入。
void upsertChannelIcon(const QString& protocol, const QString& chatId,
                       const QString& icon);

void upsertChannel(const QString& protocol, const QString& chatId,
                   const QString& name, const QString& status,
                   bool isConnected, const QString& lastMessageText,
                   const QString& lastMessageTime, qint64 lastActive);

void incrementUnread(const QString& protocol, const QString& chatId,
                     int64_t msgRowid);

void updateChannelLastMessage(const QString& protocol, const QString& chatId,
                              const QString& text, const QString& timeStr,
                              int64_t msgRowid);

typedef void (*IncomingDone)(void* ctx, const QString& protocol,
                             const QString& chatId, int64_t rowid);
void onIncomingMessage(const QString& protocol, const QString& chatId,
                       const QString& name, const MessageItem& item,
                       bool isCurrentChat, IncomingDone cb, void* ctx);

void markRead(const QString& protocol, const QString& chatId,
              int64_t lastReadRowid);

// 读取会话当前未读数（DB 唯一真源）；chanid 为空或会话不存在返回 -1
int unreadCountOf(const QString& chanid);

QVector<MessageItem> loadMessages(const QString& protocol,
                                  const QString& chatId, int limit);

// 按 channels 表主键（权威 chanid）加载消息
QVector<MessageItem> loadMessagesByChanid(const QString& chanId, int limit);

// 兜底：按 chatId 在 channels 表反查权威 chanid 后加载。
// protocol 非空时只接受 protocol 完全一致的记录 —— 群组 id 与好友 id
// 数字相同时，取「首个同 chatId 命中」会把别的协议的消息加载进来；
// protocol 为空时仅在 chatId 唯一命中时使用，多命中直接返回空（不猜）。
QVector<MessageItem> loadMessagesByChatId(const QString& protocol,
                                          const QString& chatId, int limit);

typedef void (*RowidCallback)(void* ctx, const QString& protocol,
                              const QString& chatId, qint64 localId,
                              int64_t rowid);
void insertMessage(const QString& protocol, const QString& chatId,
                   const MessageItem& item, RowidCallback cb, void* ctx);

void updateSendStateByRowid(int64_t rowid, MessageItem::SendState state);

} // namespace MsgDbBridge

#endif
