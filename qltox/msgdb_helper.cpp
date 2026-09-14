#include "msgdb_helper.h"
#include "compat34.h"
#include "storage.h"
#include "eventpoller.h"
#include <qapplication.h>

static QObject* s_rowidTarget = nullptr;

void setRowidEventTarget(QObject* target) {
    s_rowidTarget = target;
}

// ── MessageRow → ChatElement ──
ChatElement msgRowToElement(const MessageRow& row) {
    ChatElement el;
    el.etype        = (ChatElement::ElementType)row.etype;
    el.senderName   = qFromUtf8(row.sender_name);
    el.senderNickname = qFromUtf8(row.sender_nick);
    el.peerNumber   = row.peer_number;
    el.avatarUrl    = qFromUtf8(row.avatar_url);
    el.time         = qFromUtf8(row.time_text);
    el.ipAddress    = qFromUtf8(row.ip_address);
    el.messageText  = qFromUtf8(row.data);
    el.category     = qFromUtf8(row.category);
    el.caption      = qFromUtf8(row.caption);
    el.mediaUrl     = qFromUtf8(row.media_url);
    el.mediaWidth   = row.media_width;
    el.mediaHeight  = row.media_height;
    el.fileName     = qFromUtf8(row.file_name);
    el.fileSize     = row.file_size;
    el.localPath    = qFromUtf8(row.local_path);
    el.gifPath      = qFromUtf8(row.gif_path);
    el.durationSec  = row.duration_sec;
    el.sendState    = (ChatElement::SendState)row.send_state;
    el.messageId    = qFromUtf8(row.event_id);
    el.dbRowid      = row.rowid;
    el.redacted     = (row.redacted != 0);
    if (!row.relates_to_ids.empty())
        el.relatesTos = qSplit(qFromUtf8(row.relates_to_ids), QString(","));
    if (!row.mentions_text.empty())
        el.mentions = qSplit(qFromUtf8(row.mentions_text), QString(","));
    return el;
}

// ── ChatElement → MessageRow ──
static MessageRow elementToRow(int id, const std::string& type,
                                const ChatElement& el) {
    MessageRow row;
    row.chanid      = type + "_" + std::to_string(id);
    row.data        = std::string(qToUtf8(el.messageText).data());
    row.etype       = (int)el.etype;
    row.sender_name = std::string(qToUtf8(el.senderName).data());
    row.sender_nick = std::string(qToUtf8(el.senderNickname).data());
    row.peer_number = el.peerNumber;
    row.avatar_url  = std::string(qToUtf8(el.avatarUrl).data());
    row.time_text   = std::string(qToUtf8(el.time).data());
    row.ip_address  = std::string(qToUtf8(el.ipAddress).data());
    row.category    = std::string(qToUtf8(el.category).data());
    row.caption     = std::string(qToUtf8(el.caption).data());
    row.media_url   = std::string(qToUtf8(el.mediaUrl).data());
    row.media_width = el.mediaWidth;
    row.media_height= el.mediaHeight;
    row.file_name   = std::string(qToUtf8(el.fileName).data());
    row.file_size   = el.fileSize;
    row.duration_sec= el.durationSec;
    row.local_path  = std::string(qToUtf8(el.localPath).data());
    row.gif_path    = std::string(qToUtf8(el.gifPath).data());
    row.send_state  = (int)el.sendState;
    row.event_id    = std::string(qToUtf8(el.messageId).data());
    row.redacted    = el.redacted ? 1 : 0;
    {
        QStringList sl;
        for (const auto& s : el.relatesTos) sl.append(s);
        row.relates_to_ids = std::string(qToUtf8(sl.join(QString(","))).data());
    }
    {
        QStringList sl;
        for (const auto& s : el.mentions) sl.append(s);
        row.mentions_text = std::string(qToUtf8(sl.join(QString(","))).data());
    }
    if (row.event_id.empty()) {
        struct timespec ts;
        clock_gettime(CLOCK_REALTIME, &ts);
        row.event_id = "local_" + std::to_string(ts.tv_sec) + std::to_string(ts.tv_nsec);
    }
    return row;
}

// ── Async insert ──
void db_writeMessage(int id, const std::string& type, const ChatElement& el,
                     WriteMsgCallback cb) {
    auto* db = Storage::instance().messageDbAsync();
    if (!db) { return; }
    MessageRow row = elementToRow(id, type, el);
    std::string cid = row.chanid;
    std::string evid = row.event_id;
    QObject* rowidTarget = s_rowidTarget;
    // 插入完成（WriteQueue 线程）后回填元素 rowid：翻译缓存写库需要 rowid。
    // postEvent 线程安全，事件在主线程处理；目标为空或元素已不在缓冲内则跳过。
    db->insert_message(std::move(row), [cid, evid, cb, rowidTarget, id, type](int64_t rowid) {
        if (rowid <= 0) {
            qWarning("msgdb: insert_message failed chanid=%s", cid.c_str());
            return;
        }
        if (cb) { cb(rowid); }
        if (rowidTarget && !evid.empty()) {
            RowidBackfillEvent* ev = new RowidBackfillEvent();
            ev->chatId = id;
            ev->chatType = type;
            ev->rowid = rowid;
            ev->eventId = evid;
            QApplication::postEvent(rowidTarget, ev);
        }
    });
}
