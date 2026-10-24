#include "msgdb_bridge.h"
#include "storage.h"
#include "message_db.h"
#include "channel_db.h"
#include "avatarutil.h"
#include <QDebug>
#include <QCoreApplication>
#include <QMetaObject>
#include <QPointer>
#include <QDateTime>
#include <QHash>
#include <QUuid>
#include <atomic>

namespace {

// 进程实例标签（每个进程一个 UUID，仅 UI 线程访问）
QString g_instanceTag;

MessageItem itemFromRow(const MessageRow& row)
{
    MessageItem it;
    it.sender      = QString::fromUtf8(row.sender_name.c_str());
    it.avatarUrl   = QString::fromUtf8(row.avatar_url.c_str());
    it.peerNumber  = row.peer_number;
    it.content     = QString::fromUtf8(row.data.c_str());
    it.time        = QString::fromUtf8(row.time_text.c_str());
    it.isSelf      = (row.category == "self");
    it.sendState   = (MessageItem::SendState)row.send_state;
    it.mediaUrl    = QString::fromUtf8(row.media_url.c_str());
    it.audioMime   = QString::fromUtf8(row.media_mime.c_str());
    it.durationSec = row.duration_sec;
    it.isAudio     = !it.mediaUrl.isEmpty()
                     && row.media_mime.rfind("audio/", 0) == 0;
    it.avatarLetter = msgAvatarLetterFor(it.sender);
    it.avatarColor  = msgAvatarColorFor(it.sender);
    it.dbRowid     = row.rowid;
    return it;
}

MessageRow rowFromItem(const QString& chanid, const MessageItem& it)
{
    MessageRow row;
    row.chanid       = chanid.toUtf8().constData();
    row.data         = it.content.toUtf8().constData();
    row.etype        = it.isSelf ? 1 : 0;
    row.sender_name  = it.sender.toUtf8().constData();
    row.avatar_url   = it.avatarUrl.toUtf8().constData();
    row.peer_number  = it.peerNumber;
    row.time_text    = it.time.toUtf8().constData();
    row.category     = it.isSelf ? "self" : "other";
    row.media_url    = it.mediaUrl.toUtf8().constData();
    row.media_mime   = it.audioMime.toUtf8().constData();
    row.duration_sec = it.durationSec;
    row.send_state   = (int)it.sendState;
    if (it.localId != 0) {
        // localId 是进程内自增，跨进程会重复（都从 local_1 开始），
        // 撞上 messages.event_id 的 UNIQUE 约束 → 消息不落库。
        row.event_id = std::string("local_")
            + MsgDbBridge::instanceTag().toStdString()
            + "_"
            + std::to_string(it.localId);
    } else {
        // messages.event_id 是 UNIQUE 列：留空会让第二条起 INSERT 失败
        // （UNIQUE constraint failed: messages.event_id），消息不落库，
        // 连带频道 lastmsg 不更新。此处必须保证全局唯一。
        static std::atomic<uint64_t> s_eventSeq{0};
        row.event_id = std::string("auto_")
            + std::to_string(++s_eventSeq)
            + "_"
            + std::to_string(QDateTime::currentMSecsSinceEpoch());
    }
    return row;
}

} // namespace

namespace MsgDbBridge {

UnreadNotifier& UnreadNotifier::instance()
{
    static UnreadNotifier s_notifier;
    return s_notifier;
}

void UnreadNotifier::notifyUnreadChanged(const QString& chanid)
{
    emit unreadChanged(chanid);
}

namespace {

// chatId → 该 chatId 在 channels 表里的所有权威 chanid。
// 同一 chatId 可能出现在多个 protocol 下（纯数字 ID 常见），因此按列表存。
struct ChanidEntry {
    QString protocol;
    QString chanid;
};
QHash<QString, QVector<ChanidEntry>> g_chanidIndex;
bool g_indexReady = false;

void rebuildChanidIndex(const QVector<ChannelRecord>& recs)
{
    g_chanidIndex.clear();
    for (const auto& r : recs) {
        if (r.chatId.isEmpty() || r.chanId.isEmpty()) {
            continue;
        }
        auto& list = g_chanidIndex[r.chatId];
        bool dup = false;
        for (const auto& e : list) {
            if (e.chanid == r.chanId) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            list.append(ChanidEntry{ r.protocol, r.chanId });
        }
    }
}

} // namespace

QString chanKey(const QString& protocol, const QString& chatId)
{
    return protocol + QStringLiteral("_") + chatId;
}

void setInstanceTag(const QString& tag)
{
    g_instanceTag = tag;
}

QString instanceTag()
{
    if (g_instanceTag.isEmpty()) {
        g_instanceTag = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    return g_instanceTag;
}

QString resolveChanid(const QString& protocol, const QString& chatId)
{
    if (chatId.isEmpty()) {
        return QString();
    }
    const QString direct = chanKey(protocol, chatId);
    if (!g_indexReady) {
        // 索引未建立：读一次 channels 表重建
        rebuildChanidIndex(loadChannels());
        g_indexReady = true;
    }
    auto it = g_chanidIndex.constFind(chatId);
    if (it == g_chanidIndex.constEnd()) {
        return direct;
    }
    const auto& list = it.value();
    for (const auto& e : list) {
        if (e.chanid == direct) {
            return direct;
        }
    }
    // protocol 前缀与库内不一致：仅当该 chatId 在库内唯一时才认库内那条，
    // 避免把不同协议的同名会话合并。
    if (list.size() == 1) {
        return list.first().chanid;
    }
    qWarning() << "[MsgDbBridge] ambiguous chanid, chatId:" << chatId
               << "protocol:" << protocol
               << "candidates:" << list.size();
    return direct;
}

static QString protocolOfChanid(const QString& chanid)
{
    const int sep = chanid.indexOf(QLatin1Char('_'));
    if (sep <= 0) {
        return QString();
    }
    return chanid.left(sep);
}

static QString chatIdOfChanid(const QString& chanid)
{
    const int sep = chanid.indexOf(QLatin1Char('_'));
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
        g_chanidIndex.clear();
        return out;
    }
    std::vector<ChannelRow> rows = cdb->load_all_channels();
    for (const auto& r : rows) {
        const QString chanid = QString::fromUtf8(r.chanid.c_str());
        ChannelRecord rec;
        rec.chanId          = chanid;
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
        rec.lastMessageRowid = r.last_message_rowid;
        rec.lastReadRowid    = r.last_read_rowid;
        rec.lastActive      = r.last_active;
        rec.isConnected     = r.is_connected != 0;
        out.append(rec);
    }
    rebuildChanidIndex(out);
    g_indexReady = true;
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
    // 已有会话一律更新库内那条记录（chanid 权威），不新建同 chatId 的新会话
    const QString chanid = resolveChanid(protocol, chatId);
    row.chanid            = chanid.toUtf8().constData();
    row.proto_type        = protocol.toUtf8().constData();
    row.name              = name.toUtf8().constData();
    row.status            = status.toUtf8().constData();
    row.is_connected      = isConnected ? 1 : 0;
    row.last_message_text = lastMessageText.toUtf8().constData();
    row.last_message_time = lastMessageTime.toUtf8().constData();
    row.last_active       = lastActive;
    async->update_contact_channel(std::move(row), nullptr);

    // 索引同步：新建的会话立即可被 resolveChanid 命中
    auto& list = g_chanidIndex[chatId];
    bool known = false;
    for (const auto& e : list) {
        if (e.chanid == chanid) {
            known = true;
            break;
        }
    }
    if (!known) {
        list.append(ChanidEntry{ protocol, chanid });
    }
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
    const QString chanid = resolveChanid(protocol, chatId);
    async->increment_unread(chanid.toUtf8().constData(), 1, msgRowid,
        [chanid](bool) {
            // done 在写队列线程回调 → 切回 UI 线程通知列表同步
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [chanid]() {
                    UnreadNotifier::instance().notifyUnreadChanged(chanid);
                },
                Qt::QueuedConnection);
        });
}

void updateChannelLastMessage(const QString& protocol, const QString& chatId,
                              const QString& text, const QString& timeStr,
                              int64_t msgRowid)
{
    if (protocol.isEmpty() || chatId.isEmpty()) {
        return;
    }
    auto* async = Storage::instance().channelDbAsync();
    if (!async) {
        return;
    }
    ChannelUpdate upd;
    upd.hasLastMessageText = true;
    upd.last_message_text  = text.toUtf8().constData();
    upd.hasLastMessageTime = true;
    upd.last_message_time  = timeStr.toUtf8().constData();
    upd.hasLastActive      = true;
    upd.last_active        = (int64_t)QDateTime::currentSecsSinceEpoch();
    if (msgRowid > 0) {
        upd.hasLastMessageRowid = true;
        upd.last_message_rowid  = msgRowid;
    }
    async->update_channel(resolveChanid(protocol, chatId).toUtf8().constData(),
                          std::move(upd), nullptr);
}

namespace {
struct IncomingCtx {
    QString protocol;
    QString chatId;
    QString name;
    QString text;
    QString timeStr;
    bool    isCurrentChat;
    IncomingDone cb;
    void*   userCtx;
};
} // namespace

void onIncomingMessage(const QString& protocol, const QString& chatId,
                       const QString& name, const MessageItem& item,
                       bool isCurrentChat, IncomingDone cb, void* ctx)
{
    if (protocol.isEmpty() || chatId.isEmpty()) {
        return;
    }
    ensureChannel(protocol, chatId, name);

    auto* db = Storage::instance().messageDbAsync();
    if (!db) {
        return;
    }
    MessageRow row = rowFromItem(resolveChanid(protocol, chatId), item);
    IncomingCtx* c = new IncomingCtx;
    c->protocol      = protocol;
    c->chatId        = chatId;
    c->name          = name;
    c->text          = item.content;
    c->timeStr       = item.time;
    c->isCurrentChat = isCurrentChat;
    c->cb            = cb;
    c->userCtx       = ctx;
    db->insert_message(std::move(row), [c](int64_t rowid) {
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [c, rowid]() {
                if (rowid > 0) {
                    updateChannelLastMessage(c->protocol, c->chatId,
                                             c->text, c->timeStr, rowid);
                    if (!c->isCurrentChat) {
                        incrementUnread(c->protocol, c->chatId, rowid);
                    }
                    if (c->cb) {
                        c->cb(c->userCtx, c->protocol, c->chatId, rowid);
                    }
                } else {
                    qWarning() << "[MsgDbBridge] insert_message FAILED"
                               << "protocol:" << c->protocol
                               << "chatId:" << c->chatId
                               << "text:" << c->text.left(60);
                }
                delete c;
            },
            Qt::QueuedConnection);
    });
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
    const QString chanid = resolveChanid(protocol, chatId);
    async->mark_read(chanid.toUtf8().constData(), lastReadRowid,
        [chanid](bool) {
            QMetaObject::invokeMethod(
                QCoreApplication::instance(),
                [chanid]() {
                    UnreadNotifier::instance().notifyUnreadChanged(chanid);
                },
                Qt::QueuedConnection);
        });
}

int unreadCountOf(const QString& chanid)
{
    if (chanid.isEmpty()) {
        return -1;
    }
    auto* cdb = Storage::instance().channelDb();
    if (!cdb) {
        return -1;
    }
    auto row = cdb->get_channel(chanid.toUtf8().constData());
    if (!row) {
        return -1;
    }
    return row->unread_count;
}

QVector<MessageItem> loadMessages(const QString& protocol,
                                  const QString& chatId, int limit)
{
    QVector<MessageItem> out;
    auto* db = Storage::instance().messageDb();
    if (!db) {
        return out;
    }
    const QByteArray cid = resolveChanid(protocol, chatId).toUtf8();
    std::vector<MessageRow> rows = db->load_messages(cid.constData(), limit);
    for (const auto& r : rows) {
        out.append(itemFromRow(r));
    }
    return out;
}

QVector<MessageItem> loadMessagesByChatId(const QString& protocol,
                                         const QString& chatId, int limit)
{
    QVector<MessageItem> out;
    if (chatId.isEmpty()) {
        return out;
    }
    const QVector<ChannelRecord> recs = loadChannels();
    // protocol 已知：只接受 protocol 完全一致的记录。
    // 找不到就返回空 —— 回退到「任意同 chatId 的会话」会把别的协议的消息
    // 加载到当前会话（群组 id 与好友 id 同数字时必现）。
    if (!protocol.isEmpty()) {
        for (const auto& rec : recs) {
            if (rec.chatId == chatId && rec.protocol == protocol
                && !rec.chanId.isEmpty()) {
                return loadMessagesByChanid(rec.chanId, limit);
            }
        }
        return out;
    }
    // protocol 未知：仅在 chatId 唯一命中时使用（多命中无法判定归属）。
    QString found;
    for (const auto& rec : recs) {
        if (rec.chatId == chatId && !rec.chanId.isEmpty()) {
            if (!found.isEmpty()) {
                return out;
            }
            found = rec.chanId;
        }
    }
    if (!found.isEmpty()) {
        return loadMessagesByChanid(found, limit);
    }
    return out;
}

QVector<MessageItem> loadMessagesByChanid(const QString& chanId, int limit)
{
    QVector<MessageItem> out;
    if (chanId.isEmpty()) {
        return out;
    }
    auto* db = Storage::instance().messageDb();
    if (!db) {
        return out;
    }
    const QByteArray cid = chanId.toUtf8();
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
    MessageRow row = rowFromItem(resolveChanid(protocol, chatId), item);
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

#include "moc_msgdb_bridge.cpp"
