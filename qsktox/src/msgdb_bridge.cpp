#include "msgdb_bridge.h"
#include "storage.h"
#include "message_db.h"
#include "channel_db.h"
#include <QDebug>
#include <QCoreApplication>
#include <QMetaObject>
#include <QPointer>
#include <QDateTime>

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

static QString protocolOfChanid(const QString& chanid)
{
    const int sep = chanid.lastIndexOf(QLatin1Char('_'));
    if (sep <= 0) {
        return QString();
    }
    return chanid.left(sep);
}

static QString chatIdOfChanid(const QString& chanid)
{
    const int sep = chanid.lastIndexOf(QLatin1Char('_'));
    if (sep <= 0 || sep >= chanid.size() - 1) {
        return QString();
    }
    return chanid.mid(sep + 1);
}

void ensureChannel(const QString& protocol, const QString& chatId,
                   const QString& name)
{
    upsertChannel(protocol, chatId, name, QStringLiteral("online"),
                  false, QString(), QString(),
                  QDateTime::currentSecsSinceEpoch());
}

QVector<ChannelRecord> loadChannels()
{
    QVector<ChannelRecord> out;
    auto* cdb = Storage::instance().channelDb();
    if (!cdb) {
        return out;
    }
    std::vector<ChannelRow> rows = cdb->load_all_channels();
    for (const auto& r : rows) {
        const QString chanid = QString::fromUtf8(r.chanid.c_str());
        ChannelRecord rec;
        rec.protocol        = protocolOfChanid(chanid);
        rec.chatId          = chatIdOfChanid(chanid);
        if (rec.chatId.isEmpty()) {
            continue;
        }
        rec.name            = QString::fromUtf8(r.name.c_str());
        rec.status          = QString::fromUtf8(r.status.c_str());
        rec.lastMessageText = QString::fromUtf8(r.last_message_text.c_str());
        rec.lastMessageTime = QString::fromUtf8(r.last_message_time.c_str());
        rec.unreadCount     = r.unread_count;
        rec.lastActive      = r.last_active;
        rec.isConnected     = r.is_connected != 0;
        out.append(rec);
    }
    return out;
}

void upsertChannel(const QString& protocol, const QString& chatId,
                   const QString& name, const QString& status,
                   bool isConnected, const QString& lastMessageText,
                   const QString& lastMessageTime, qint64 lastActive)
{
    if (protocol.isEmpty() || chatId.isEmpty()) {
        return;
    }
    auto* async = Storage::instance().channelDbAsync();
    if (!async) {
        return;
    }
    ChannelRow row;
    row.chanid            = chanKey(protocol, chatId).toUtf8().constData();
    row.proto_type        = protocol.toUtf8().constData();
    row.name              = name.toUtf8().constData();
    row.status            = status.toUtf8().constData();
    row.is_connected      = isConnected ? 1 : 0;
    row.last_message_text = lastMessageText.toUtf8().constData();
    row.last_message_time = lastMessageTime.toUtf8().constData();
    row.last_active       = lastActive;
    async->update_contact_channel(std::move(row), nullptr);
}

void incrementUnread(const QString& protocol, const QString& chatId,
                     int64_t msgRowid)
{
    if (protocol.isEmpty() || chatId.isEmpty()) {
        return;
    }
    auto* async = Storage::instance().channelDbAsync();
    if (!async) {
        return;
    }
    async->increment_unread(chanKey(protocol, chatId).toUtf8().constData(),
                            1, msgRowid, nullptr);
}

void markRead(const QString& protocol, const QString& chatId,
              int64_t lastReadRowid)
{
    if (protocol.isEmpty() || chatId.isEmpty()) {
        return;
    }
    auto* async = Storage::instance().channelDbAsync();
    if (!async) {
        return;
    }
    async->mark_read(chanKey(protocol, chatId).toUtf8().constData(),
                     lastReadRowid, nullptr);
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
