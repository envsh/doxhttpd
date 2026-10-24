#include "rawdatautil.h"

#include "message_db.h"
#include "storage.h"

#include <QString>
#include <memory>

namespace {

constexpr int kMaxDataChars = 100000;

QString fromStd(const std::string& value)
{
    return QString::fromUtf8(value.data(), int(value.size()));
}

QString orDash(const QString& value)
{
    return value.isEmpty() ? QString::fromUtf8("—") : value;
}

QString line(const char* key, const QString& value)
{
    return QString::fromLatin1(key) + QStringLiteral(" = ")
        + orDash(value) + QLatin1Char('\n');
}

template <typename T>
QString numberLine(const char* key, T value)
{
    return line(key, QString::number(value));
}

QString boolLine(const char* key, bool value)
{
    return line(key, value ? QStringLiteral("true") : QStringLiteral("false"));
}

QString sendStateText(MessageItem::SendState state)
{
    switch (state) {
    case MessageItem::SendSending:
        return QStringLiteral("SendSending");
    case MessageItem::SendSent:
        return QStringLiteral("SendSent");
    case MessageItem::SendFailed:
        return QStringLiteral("SendFailed");
    }
    return QStringLiteral("Unknown");
}

QString buildItemSection(const MessageItem& item)
{
    QString text;
    text += QString::fromUtf8("== 消息对象 ==\n");
    text += line("sender", item.sender);
    text += line("content", item.content);
    text += line("time", item.time);
    text += boolLine("isSelf", item.isSelf);
    text += line("avatarUrl", item.avatarUrl);
    text += numberLine("peerNumber", item.peerNumber);
    text += line("avatarLetter", item.avatarLetter);
    text += line("avatarColor", item.avatarColor.name());
    text += line("sendState", sendStateText(item.sendState));
    text += line("sendError", item.sendError);
    text += numberLine("reqSeq", item.reqSeq);
    text += numberLine("localId", item.localId);
    text += numberLine("dbRowid", item.dbRowid);
    text += boolLine("isAudio", item.isAudio);
    text += line("mediaUrl", item.mediaUrl);
    text += line("audioMime", item.audioMime);
    text += numberLine("durationSec", item.durationSec);
    return text;
}

QString buildRowSection(const MessageRow& row)
{
    QString dataText = fromStd(row.data);
    const int dataSize = dataText.size();
    if (dataSize > kMaxDataChars) {
        dataText = dataText.left(kMaxDataChars)
            + QString::fromUtf8("…（已截断，共 %1 字符）").arg(dataSize);
    }

    QString text;
    text += QString::fromUtf8("\n== 数据库行 ==\n");
    text += numberLine("rowid", row.rowid);
    text += line("event_id", fromStd(row.event_id));
    text += line("chanid", fromStd(row.chanid));
    text += line("data", dataText);
    text += numberLine("etype", row.etype);
    text += line("sender_name", fromStd(row.sender_name));
    text += line("sender_nick", fromStd(row.sender_nick));
    text += numberLine("peer_number", row.peer_number);
    text += line("sender_pubkey", fromStd(row.sender_pubkey));
    text += line("signature", fromStd(row.signature));
    text += line("avatar_url", fromStd(row.avatar_url));
    text += line("time_text", fromStd(row.time_text));
    text += line("ip_address", fromStd(row.ip_address));
    text += line("category", fromStd(row.category));
    text += line("caption", fromStd(row.caption));
    text += line("media_url", fromStd(row.media_url));
    text += line("media_mime", fromStd(row.media_mime));
    text += numberLine("media_width", row.media_width);
    text += numberLine("media_height", row.media_height);
    text += line("file_name", fromStd(row.file_name));
    text += numberLine("file_size", row.file_size);
    text += numberLine("duration_sec", row.duration_sec);
    text += line("local_path", fromStd(row.local_path));
    text += line("gif_path", fromStd(row.gif_path));
    text += line("thumbnail_key", fromStd(row.thumbnail_key));
    text += numberLine("cache_tag", row.cache_tag);
    text += numberLine("send_state", row.send_state);
    text += numberLine("relates_to_rowid", row.relates_to_rowid);
    text += numberLine("edited", row.edited);
    text += numberLine("redacted", row.redacted);
    text += line("forwarded_from", fromStd(row.forwarded_from));
    text += numberLine("mention", row.mention);
    text += line("relates_to_ids", fromStd(row.relates_to_ids));
    text += line("mentions_text", fromStd(row.mentions_text));
    return text;
}

} // namespace

QString buildRawDataText(const MessageItem& item)
{
    QString text = buildItemSection(item);

    if (item.dbRowid <= 0) {
        text += QString::fromUtf8("\n== 数据库行 ==\n（尚未落库，无数据库行）\n");
        return text;
    }

    auto* db = Storage::instance().messageDb();
    if (!db) {
        text += QString::fromUtf8("\n== 数据库行 ==\n（数据库未初始化）\n");
        return text;
    }

    std::unique_ptr<MessageRow> row = db->get_message(item.dbRowid);
    if (!row) {
        text += QString::fromUtf8("\n== 数据库行 ==\n（未找到 rowid = %1 的记录）\n")
                    .arg(QString::number(item.dbRowid));
        return text;
    }

    text += buildRowSection(*row);
    return text;
}
