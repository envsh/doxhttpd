#ifndef MSGDB_BRIDGE_H
#define MSGDB_BRIDGE_H

#include "messagelist.h"
#include <QString>
#include <QVector>

namespace MsgDbBridge {

QString chanKey(const QString& protocol, const QString& chatId);

void ensureChannel(const QString& protocol, const QString& chatId,
                   const QString& name);

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
