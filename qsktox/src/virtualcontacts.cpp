#include "virtualcontacts.h"
#include "pureconsts.hpp"

const QVector<VirtualContact>& virtualContacts()
{
    static QVector<VirtualContact> s_virtual;
    if (s_virtual.isEmpty()) {
        const int n = (int)(sizeof(kVirtualContactDefs) / sizeof(kVirtualContactDefs[0]));
        for (int i = 0; i < n; ++i) {
            s_virtual.append({ kVirtualContactDefs[i].id,
                               kVirtualContactDefs[i].type,
                               kVirtualContactDefs[i].name,
                               kVirtualContactDefs[i].emoji });
        }
    }
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
