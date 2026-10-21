#ifndef MSGDB_BRIDGE_H
#define MSGDB_BRIDGE_H

#include "messagelist.h"
#include <QString>
#include <QVector>

namespace MsgDbBridge {

struct ChannelRecord {
    QString protocol;
    QString chatId;
    QString name;
    QString status;
    QString lastMessageText;
    QString lastMessageTime;
    int     unreadCount = 0;
    qint64  lastActive = 0;
    bool    isConnected = false;
};

QString chanKey(const QString& protocol, const QString& chatId);

void ensureChannel(const QString& protocol, const QString& chatId,
                   const QString& name);

QVector<ChannelRecord> loadChannels();

void upsertChannel(const QString& protocol, const QString& chatId,
                   const QString& name, const QString& status,
                   bool isConnected, const QString& lastMessageText,
                   const QString& lastMessageTime, qint64 lastActive);

void incrementUnread(const QString& protocol, const QString& chatId,
                     int64_t msgRowid);

void markRead(const QString& protocol, const QString& chatId,
              int64_t lastReadRowid);

QVector<MessageItem> loadMessages(const QString& protocol,
                                  const QString& chatId, int limit);

typedef void (*RowidCallback)(void* ctx, const QString& protocol,
                              const QString& chatId, qint64 localId,
                              int64_t rowid);
void insertMessage(const QString& protocol, const QString& chatId,
                   const MessageItem& item, RowidCallback cb, void* ctx);

void updateSendStateByRowid(int64_t rowid, MessageItem::SendState state);

} // namespace MsgDbBridge

#endif
