#include "rawdatautil.h"

#include "message_db.h"
#include "messagelist.h"
#include "storage.h"

#include <QString>
#include <QStringList>
#include <QVector>
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

// 键一律用 QString：避免临时 toLatin1().constData() 悬挂
QString line(const QString& key, const QString& value)
{
    return key + QStringLiteral(" = ")
        + orDash(value) + QLatin1Char('\n');
}

QString numberLine(const QString& key, qint64 value)
{
    return line(key, QString::number(value));
}

QString boolLine(const QString& key, bool value)
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

QString federatedText(int v)
{
    if (v == 1) {
        return QStringLiteral("true");
    }
    if (v == 0) {
        return QStringLiteral("false");
    }
    return QStringLiteral("unknown");
}

// 接收字段结构体（push 原文保真，attachments[] 保留整组数组不平铺）
struct ReceiveRaw {
    // 基本/上下文
    QString protocol;
    QString chatId;
    QString chatName;
    QString chatType;
    QString accountId;
    QString accountName;
    QString gateway;
    QString chatTopic;
    QString chatAlias;
    int chatMembers = 0;
    int chatFederated = -1;   // -1 未知 / 0 false / 1 true
    QString chanIcon;

    // 身份
    QString username;
    QString userId;
    QString usernick;
    QString userIcon;         // push user_icon（= item.avatarUrl 同源）

    // 消息标识/元数据
    QString msgType;
    QString msgFormat;
    QString msgId;
    QStringList replyTos;
    QStringList mentions;
    qint64 timestampMs = 0;

    // 顶层媒体（部分载荷只有顶层 media_url/media_mime，不等同 attachments）
    QString topMediaUrl;
    QString topMediaMime;

    // 附件数组（结构保留）
    QVector<AttachRaw> attachments;
    int attachCount = 0;
    QString attachBrief;
};

ReceiveRaw fromItem(const MessageItem& item)
{
    ReceiveRaw r;
    r.protocol      = item.protocol;
    r.chatId        = item.chatId;
    r.chatName      = item.chatName;
    r.chatType      = item.chatType;
    r.accountId     = item.accountId;
    r.accountName   = item.accountName;
    r.gateway       = item.gateway;
    r.chatTopic     = item.chatTopic;
    r.chatAlias     = item.chatAlias;
    r.chatMembers   = item.chatMembers;
    r.chatFederated = item.chatFederated;
    r.chanIcon      = item.chanIcon;

    r.username = item.username;
    r.userId   = item.userId;
    r.usernick = item.usernick;
    r.userIcon = item.avatarUrl;

    r.msgType     = item.msgType;
    r.msgFormat   = item.msgFormat;
    r.msgId       = item.msgId;
    r.replyTos    = item.replyTos;
    r.mentions    = item.mentions;
    r.timestampMs = item.timestampMs;

    r.topMediaUrl  = item.mediaUrl;
    r.topMediaMime = item.mediaMime;

    r.attachments = item.attachments;
    r.attachCount = item.attachCount > 0
        ? item.attachCount : item.attachments.size();
    r.attachBrief = item.attachBrief;
    if (r.attachBrief.isEmpty() && !r.attachments.isEmpty()) {
        QString brief;
        for (int ai = 0; ai < r.attachments.size(); ++ai) {
            const AttachRaw& ar = r.attachments.at(ai);
            brief += QStringLiteral("%1 %2 %3 %4\n")
                .arg(ai).arg(ar.mime).arg(ar.attachtype).arg(ar.url);
        }
        r.attachBrief = brief.trimmed();
    }
    return r;
}

// msgid 主链：接收原文 > DB event_id（local_/auto_ 合成键也如实展示）
QString pickMsgId(const MessageItem& item, const MessageRow* row)
{
    if (!item.msgId.isEmpty()) {
        return item.msgId;
    }
    if (row && !row->event_id.empty()) {
        return fromStd(row->event_id);
    }
    return QString();
}

QString pickContent(const MessageItem& item, const MessageRow* row)
{
    if (!item.content.isEmpty()) {
        return item.content;
    }
    if (row && !row->data.empty()) {
        return fromStd(row->data);
    }
    return QString();
}

QString buildMerged(const MessageItem& item, const MessageRow* row)
{
    const ReceiveRaw rr = fromItem(item);
    QString text;
    text += QString::fromUtf8("== 原始数据 ==\n");

    // ----【基本】----
    {
        QString g;
        if (!rr.protocol.isEmpty())      g += line("protocol", rr.protocol);
        if (!rr.accountId.isEmpty())     g += line("account_id", rr.accountId);
        if (!rr.accountName.isEmpty())   g += line("account_name", rr.accountName);
        if (!rr.gateway.isEmpty())       g += line("gateway", rr.gateway);
        if (!rr.chatId.isEmpty())        g += line("chat_id", rr.chatId);
        if (!rr.chatName.isEmpty())      g += line("chat_name", rr.chatName);
        if (!rr.chatType.isEmpty())      g += line("chat_type", rr.chatType);
        if (!rr.chatTopic.isEmpty())     g += line("chat_topic", rr.chatTopic);
        if (!rr.chatAlias.isEmpty())     g += line("chat_alias", rr.chatAlias);
        if (rr.chatMembers != 0)         g += numberLine("chat_members", rr.chatMembers);
        if (rr.chatFederated != -1)      g += line("chat_federated", federatedText(rr.chatFederated));
        if (!rr.chanIcon.isEmpty())      g += line("chan_icon", rr.chanIcon);
        // 历史消息：接收段空 → DB chanid 补足
        if (rr.chatId.isEmpty() && row && !row->chanid.empty()) {
            g += line("chat_id", fromStd(row->chanid));
        }
        if (row && !row->ip_address.empty()) {
            g += line("ip_address", fromStd(row->ip_address));
        }
        if (!g.isEmpty()) {
            text += QString::fromUtf8("【基本】\n") + g;
        }
    }

    // ----【身份】----
    {
        QString g;
        if (!item.sender.isEmpty())      g += line("sender", item.sender);
        if (!rr.usernick.isEmpty())      g += line("usernick", rr.usernick);
        if (!rr.username.isEmpty())      g += line("username", rr.username);
        if (!rr.userId.isEmpty())        g += line("userid", rr.userId);
        if (!rr.userIcon.isEmpty())      g += line("user_icon", rr.userIcon);
        // 历史消息：接收段空 → DB 行补足（sender_name/sender_nick/avatar_url）
        if (item.sender.isEmpty()) {
            if (row && !row->sender_name.empty()) {
                g += line("sender", fromStd(row->sender_name));
            }
            if (row && !row->sender_nick.empty()) {
                g += line("usernick", fromStd(row->sender_nick));
            }
        }
        if (rr.userIcon.isEmpty() && row && !row->avatar_url.empty()) {
            g += line("user_icon", fromStd(row->avatar_url));
        }
        if (row && !row->sender_pubkey.empty()) {
            g += line("sender_pubkey", fromStd(row->sender_pubkey));
        }
        if (!g.isEmpty()) {
            text += QString::fromUtf8("【身份】\n") + g;
        }
    }

    // ----【消息标识】----
    {
        QString g;
        const QString msgId = pickMsgId(item, row);
        if (!msgId.isEmpty())            g += line("msgid", msgId);
        if (row) {
            g += numberLine("rowid", row->rowid);
            if (!row->signature.empty()) g += line("signature", fromStd(row->signature));
        } else if (item.dbRowid > 0) {
            // 尚未读到行也要标出方向：落库但读不到 → 明示
            g += numberLine("rowid", item.dbRowid);
        }
        if (item.localId != 0)           g += numberLine("localId", item.localId);
        if (!g.isEmpty()) {
            text += QString::fromUtf8("【消息标识】\n") + g;
        }
    }

    // ----【正文】----
    {
        QString g;
        QString content = pickContent(item, row);
        if (!content.isEmpty()) {
            if (content.size() > kMaxDataChars) {
                content = content.left(kMaxDataChars)
                    + QString::fromUtf8("…（已截断，共 %1 字符）").arg(content.size());
            }
            g += line("content", content);
        }
        if (row && !row->caption.empty()) {
            g += line("caption", fromStd(row->caption));
        }
        if (!g.isEmpty()) {
            text += QString::fromUtf8("【正文】\n") + g;
        }
    }

    // ----【消息类型】----
    {
        QString msgType = rr.msgType;
        if (msgType.isEmpty() && row && !row->msg_type.empty()) {
            msgType = fromStd(row->msg_type);
        }
        const QString msgFormat = rr.msgFormat;
        if (!msgType.isEmpty() || !msgFormat.isEmpty()) {
            text += QString::fromUtf8("【消息类型】\n");
            if (!msgType.isEmpty()) {
                text += line("msgtype", msgType);
            }
            if (!msgFormat.isEmpty()) {
                text += line("msgformat", msgFormat);
            }
        }
    }

    // ----【媒体（顶层）】----
    {
        QString g;
        QString topUrl = rr.topMediaUrl;
        QString topMime = rr.topMediaMime;
        if (topUrl.isEmpty() && row && !row->media_url.empty()) {
            topUrl = fromStd(row->media_url);
        }
        if (topMime.isEmpty() && row && !row->media_mime.empty()) {
            topMime = fromStd(row->media_mime);
        }
        // 顶层字段与 attachments[] 是两回事：仅当有顶层字段或无附件数组时展示，
        // 避免多附件场景用单一顶层字段代表全部附件
        if (!topUrl.isEmpty())           g += line("topMediaUrl", topUrl);
        if (!topMime.isEmpty())          g += line("topMediaMime", topMime);
        if (!g.isEmpty()) {
            text += QString::fromUtf8("【媒体（顶层）】\n") + g;
        }
    }

    // ----【附件】（结构保留，逐索引输出，绝不平铺）----
    if (!rr.attachments.isEmpty()) {
        text += QString::fromUtf8("【附件】\n");
        text += numberLine("attachCount", rr.attachCount);
        if (!rr.attachBrief.isEmpty()) {
            text += line("attachBrief", rr.attachBrief);
        }
        for (int ai = 0; ai < rr.attachments.size(); ++ai) {
            const AttachRaw& ar = rr.attachments.at(ai);
            text += QString::fromUtf8("[附件 %1]\n").arg(ai);
            if (!ar.url.isEmpty())         text += line("  url", ar.url);
            if (!ar.mime.isEmpty())        text += line("  mime", ar.mime);
            if (!ar.filename.isEmpty())    text += line("  filename", ar.filename);
            if (!ar.attachtype.isEmpty())  text += line("  attachtype", ar.attachtype);
            if (ar.size != 0)              text += line("  size", QString::number(ar.size));
            if (ar.width != 0)             text += line("  width", QString::number(ar.width));
            if (ar.height != 0)            text += line("  height", QString::number(ar.height));
            if (ar.durationSec != 0)       text += line("  duration_sec", QString::number(ar.durationSec));
            if (!ar.thumbnailUrl.isEmpty())text += line("  thumbnailUrl", ar.thumbnailUrl);
        }
    }

    // ----【引用/提及】----
    {
        QString g;
        if (!rr.replyTos.isEmpty()) {
            g += line("reply_tos", rr.replyTos.join(QLatin1Char(',')));
        }
        if (!rr.mentions.isEmpty()) {
            g += line("mentions", rr.mentions.join(QLatin1Char(',')));
        }
        if (row) {
            if (row->relates_to_rowid != 0) {
                g += numberLine("relates_to_rowid", row->relates_to_rowid);
            }
            if (!row->relates_to_ids.empty()) {
                g += line("relates_to_ids", fromStd(row->relates_to_ids));
            }
            if (!row->mentions_text.empty()) {
                g += line("mentions_text", fromStd(row->mentions_text));
            }
            if (row->edited != 0)        g += numberLine("edited", row->edited);
            if (row->redacted != 0)      g += numberLine("redacted", row->redacted);
            if (row->mention != 0)       g += numberLine("mention", row->mention);
            if (!row->forwarded_from.empty()) {
                g += line("forwarded_from", fromStd(row->forwarded_from));
            }
        }
        if (!g.isEmpty()) {
            text += QString::fromUtf8("【引用/提及】\n") + g;
        }
    }

    // ----【时间】----
    {
        QString g;
        if (rr.timestampMs != 0) {
            g += numberLine("timestamp", rr.timestampMs);
        }
        if (row && !row->time_text.empty()) {
            g += line("time_text", fromStd(row->time_text));
        } else if (!item.time.isEmpty()) {
            // 无 DB 行时才用内存折损值，避免与 time_text 同义重复
            g += line("time", item.time);
        }
        if (!g.isEmpty()) {
            text += QString::fromUtf8("【时间】\n") + g;
        }
    }

    // ----【本地/缓存】----
    {
        QString g;
        if (!item.localPath.isEmpty()) {
            g += line("local_path", item.localPath);
        } else if (row && !row->local_path.empty()) {
            g += line("local_path", fromStd(row->local_path));
        }
        // gif_path 是 DB 既有列（qltox 侧 giflike 转码产物语义）；qsktox
        // 不写该列（转码链用 mediaUrl+"@gif@" 伪源键），仅历史行可能有值
        if (row && !row->gif_path.empty()) {
            g += line("gif_path", fromStd(row->gif_path));
        }
        if (row && row->cache_tag != 0) {
            g += numberLine("cache_tag", row->cache_tag);
        }
        if (!g.isEmpty()) {
            text += QString::fromUtf8("【本地/缓存】\n") + g;
        }
    }

    // ----【运行态】（仅非默认/非空时渲染整个小节）----
    {
        QString g;
        if (item.isSelf)               g += boolLine("isSelf", true);
        if (item.sendState != MessageItem::SendSending
            || !item.sendError.isEmpty()) {
            g += line("send_state", sendStateText(item.sendState));
            if (!item.sendError.isEmpty()) {
                g += line("send_error", item.sendError);
            }
            if (item.reqSeq != 0)      g += numberLine("req_seq", item.reqSeq);
        }
        if (item.downloadState != 0
            || item.downloadProgress != 0
            || item.downloadSpeedBps != 0
            || !item.errorMsg.isEmpty()) {
            g += numberLine("download_state", item.downloadState);
            g += numberLine("download_progress", item.downloadProgress);
            g += numberLine("download_speed_bps", item.downloadSpeedBps);
            if (!item.errorMsg.isEmpty()) {
                g += line("error_msg", item.errorMsg);
            }
        }
        if (item.mediaPlaying)         g += boolLine("media_playing", true);
        if (item.isAudio)              g += boolLine("isAudio", true);
        if (item.peerNumber != -1)     g += line("peerNumber", QString::number(item.peerNumber));
        if (item.transState != 0
            || item.showTranslation
            || !item.translatedText.isEmpty()
            || !item.translateError.isEmpty()
            || item.translateKey != 0) {
            g += numberLine("trans_state", item.transState);
            if (item.showTranslation)  g += boolLine("show_translation", true);
            if (!item.translatedText.isEmpty()) {
                g += line("translated_text", item.translatedText);
            }
            if (!item.translateError.isEmpty()) {
                g += line("translate_error", item.translateError);
            }
            if (item.translateKey != 0) g += numberLine("translate_key", item.translateKey);
        }
        if (item.lpState != 0
            || !item.lpUrl.isEmpty()
            || !item.lpTitle.isEmpty()
            || !item.lpDesc.isEmpty()
            || !item.lpSite.isEmpty()) {
            g += numberLine("lpState", item.lpState);
            if (!item.lpUrl.isEmpty())  g += line("lpUrl", item.lpUrl);
            if (!item.lpTitle.isEmpty())g += line("lpTitle", item.lpTitle);
            if (!item.lpDesc.isEmpty()) g += line("lpDesc", item.lpDesc);
            if (!item.lpSite.isEmpty()) g += line("lpSite", item.lpSite);
        }
        if (!g.isEmpty()) {
            text += QString::fromUtf8("【运行态】\n") + g;
        }
    }

    // DB 行里有而各合并键未单列的字段：etype/category 是方向旗，
    // 语义已被 isSelf 覆盖（同一个方向的两种表达），合并视图不再重复输出。
    return text;
}

} // namespace

QString buildRawDataText(const MessageItem& item)
{
    if (item.dbRowid <= 0) {
        return buildMerged(item, nullptr);
    }

    auto* db = Storage::instance().messageDb();
    if (!db) {
        return buildMerged(item, nullptr);
    }

    std::unique_ptr<MessageRow> row = db->get_message(item.dbRowid);
    if (!row) {
        return buildMerged(item, nullptr);
    }

    return buildMerged(item, row.get());
}
