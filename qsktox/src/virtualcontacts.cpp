#include "virtualcontacts.h"

const QVector<VirtualContact>& virtualContacts()
{
    static const QVector<VirtualContact> s_virtual = {
        { -100, "unknown",   "Unknown",         "❓" },
        { -101, "sysevent",  "Sysevent",        "🔔" },
        { -102, "reddit",    "Reddit",          "👽" },
        { -103, "bookmark",  "Bookmark·收藏夹",  "🔖" },
        { -104, "aichat",    "AI·GPT Chat",     "🤖" },
        { -105, "pastebin",  "Pastebin",        "📋" },
        { -106, "translate", "Translate",       "🌐" },
        { -121, "mobpush",   "MobPush",         "📨" },
        { -122, "snap",      "Snap",            "📸" },
        { -123, "websearch", "Search·网页搜索",  "🔍" },
        { -124, "tts",       "TTS",             "🔊" },
    };
    return s_virtual;
}

VirtualSendKind virtualSendKind(const QString& protocol)
{
    if (protocol == QLatin1String("unknown")
        || protocol == QLatin1String("sysevent")
        || protocol == QLatin1String("reddit")
        || protocol == QLatin1String("bookmark")
        || protocol == QLatin1String("mobpush")
        || protocol == QLatin1String("snap")) {
        return kVirtualLocalNoop;
    }
    if (protocol == QLatin1String("aichat")) {
        return kVirtualBotAigpt;
    }
    if (protocol == QLatin1String("websearch")) {
        return kVirtualBotWebsearch;
    }
    if (protocol == QLatin1String("translate")) {
        return kVirtualBotTranslate;
    }
    if (protocol == QLatin1String("pastebin")) {
        return kVirtualBotPaste;
    }
    if (protocol == QLatin1String("tts")) {
        return kVirtualBotTts;
    }
    return kVirtualNone;
}

bool isVirtualProtocol(const QString& protocol)
{
    return virtualSendKind(protocol) != kVirtualNone;
}
