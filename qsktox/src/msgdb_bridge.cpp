#include "msgdb_bridge.h"
#include "storage.h"
#include "message_db.h"
#include "channel_db.h"
#include <QDebug>
#include <QCoreApplication>
#include <QMetaObject>
#include <QPointer>

namespace {

MessageItem itemFromRow(const MessageRow& row)
{
    MessageItem it;
    it.sender      = QString::fromUtf8(row.sender_name.c_str());
    it.content     = QString::fromUtf8(row.data.c_str());
    it.time        = QString::fromUtf8(row.time_text.c_str());
    it.isSelf      = (row.category == "self");
    it.sendState   = (MessageItem::SendState)row.send_state;
    it.mediaUrl    = QString::fromUtf8(row.media_url.c_str());
    it.audioMime   = QString::fromUtf8(row.media_mime.c_str());
    it.durationSec = row.duration_sec;
    it.isAudio     = !it.mediaUrl.isEmpty()
                     && row.media_mime.rfind("audio/", 0) == 0;
    it.avatarLetter = it.sender.isEmpty() ? QChar(' ')
                                          : it.sender.at(0).toUpper();
    it.avatarColor = QColor("#1565C0");
    return it;
}

MessageRow rowFromItem(const QString& chanid, const MessageItem& it)
{
    MessageRow row;
    row.chanid       = chanid.toUtf8().constData();
    row.data         = it.content.toUtf8().constData();
    row.etype        = it.isSelf ? 1 : 0;
    row.sender_name  = it.sender.toUtf8().constData();
    row.time_text    = it.time.toUtf8().constData();
    row.category     = it.isSelf ? "self" : "";
    row.media_url    = it.mediaUrl.toUtf8().constData();
    row.media_mime   = it.audioMime.toUtf8().constData();
    row.duration_sec = it.durationSec;
    row.send_state   = (int)it.sendState;
    if (it.localId != 0) {
        row.event_id = std::string("local_") + std::to_string(it.localId);
    }
    return row;
}

} // namespace

namespace MsgDbBridge {

QString chanKey(const QString& protocol, const QString& chatId)
{
    return protocol + QStringLiteral("_") + chatId;
}

void ensureChannel(const QString& protocol, const QString& chatId,
                   const QString& name)
{
    auto* cdb = Storage::instance().channelDb();
    if (!cdb) {
        return;
    }
    ChannelRow row;
    row.chanid       = chanKey(protocol, chatId).toUtf8().constData();
    row.proto_type   = protocol.toUtf8().constData();
    row.name         = name.toUtf8().constData();
    row.status       = "online";
    row.is_connected = 0;
    cdb->update_contact_channel(row);
}

QVector<MessageItem> loadMessages(const QString& protocol,
                                  const QString& chatId, int limit)
{
    QVector<MessageItem> out;
    auto* db = Storage::instance().messageDb();
    if (!db) {
        return out;
    }
    const QByteArray cid = chanKey(protocol, chatId).toUtf8();
    std::vector<MessageRow> rows = db->load_messages(cid.constData(), limit);
    for (const auto& r : rows) {
        out.append(itemFromRow(r));
    }
    return out;
}

void insertMessage(const QString& protocol, const QString& chatId,
                   const MessageItem& item, RowidCallback cb, void* ctx)
{
    auto* db = Storage::instance().messageDbAsync();
    if (!db) {
        return;
    }
    MessageRow row = rowFromItem(chanKey(protocol, chatId), item);
    const std::string cid = row.chanid;
    const qint64 localId = item.localId;
    db->insert_message(std::move(row),
        [cid, localId, cb, ctx, protocol, chatId](int64_t rowid) {
            if (rowid <= 0) {
                qWarning("msgdb: insert failed chanid=%s", cid.c_str());
                return;
            }
            if (cb) {
                QPointer<QObject> guard(static_cast<QObject*>(ctx));
                QMetaObject::invokeMethod(
                    QCoreApplication::instance(),
                    [guard, cb, ctx, protocol, chatId, localId, rowid]() {
                        if (guard) {
                            cb(ctx, protocol, chatId, localId, rowid);
                        }
                    },
                    Qt::QueuedConnection);
            }
        });
}

void updateSendStateByRowid(int64_t rowid, MessageItem::SendState state)
{
    if (rowid <= 0) {
        return;
    }
    auto* db = Storage::instance().messageDbAsync();
    if (!db) {
        return;
    }
    MessageUpdate upd;
    upd.hasSendState = true;
    upd.send_state   = (int)state;
    db->update_message(rowid, std::move(upd), [](bool) {});
}

} // namespace MsgDbBridge
