#include "virtualbothost.h"

#include "eventpoller.h"

VirtualBotHost::VirtualBotHost()
    : QObject(nullptr)
{
}

VirtualBotHost* VirtualBotHost::instance()
{
    static VirtualBotHost* s_instance = nullptr;
    if (!s_instance) {
        s_instance = new VirtualBotHost();
    }
    return s_instance;
}

qint64 VirtualBotHost::nextLocalId()
{
    static qint64 s_next = 1;
    return s_next++;
}

static QString joinWebsearchItems(const WebsearchDoneEvent* evt)
{
    auto oneLine = [](const QString& s) {
        QString t = s;
        t.replace(QChar('\n'), QChar(' '));
        t.replace(QChar('\r'), QChar(' '));
        return t;
    };

    QString out;
    const int n = (int)evt->items.size();
    for (int i = 0; i < n && i < 3; ++i) {
        if (i) {
            out += QString::fromUtf8("\n\n");
        }
        out += QString::number(i + 1) + QString::fromUtf8(". ")
             + oneLine(QString::fromUtf8(evt->items[i].title.c_str()))
             + QString::fromUtf8("\n")
             + oneLine(QString::fromUtf8(evt->items[i].url.c_str()));
        if (!evt->items[i].snippet.empty()) {
            out += QString::fromUtf8("\n")
                 + oneLine(QString::fromUtf8(evt->items[i].snippet.c_str()));
        }
    }
    if (!evt->backendUsed.empty()) {
        out += QString::fromUtf8("\n\n-- ")
             + QString::fromUtf8(evt->backendUsed.c_str());
    }
    return out;
}

void VirtualBotHost::customEvent(QEvent* event)
{
    if (event->type() == AigptbotDoneType) {
        AigptbotDoneEvent* evt = static_cast<AigptbotDoneEvent*>(event);
        QString text = QString::fromUtf8(evt->reply.c_str());
        if (!evt->providerUsed.empty()) {
            text += QString::fromUtf8(" -- ")
                  + QString::fromUtf8(evt->providerUsed.c_str());
        }
        emit botDone(evt->chatId, QStringLiteral("aichat"), (qint64)evt->localId,
                     evt->success, evt->success ? text : QString(),
                     QString::fromUtf8(evt->providerUsed.c_str()),
                     QString::fromUtf8(evt->errorMsg.c_str()));
        return;
    }
    if (event->type() == FanyibotDoneType) {
        FanyibotDoneEvent* evt = static_cast<FanyibotDoneEvent*>(event);
        QString text = QString::fromUtf8(evt->translatedText.c_str());
        if (!evt->engineUsed.empty()) {
            text += QString::fromUtf8(" -- ")
                  + QString::fromUtf8(evt->engineUsed.c_str());
        }
        emit botDone(evt->chatId, QStringLiteral("translate"), (qint64)evt->localId,
                     evt->success, evt->success ? text : QString(),
                     QString::fromUtf8(evt->engineUsed.c_str()),
                     QString::fromUtf8(evt->errorMsg.c_str()));
        return;
    }
    if (event->type() == WebsearchDoneType) {
        WebsearchDoneEvent* evt = static_cast<WebsearchDoneEvent*>(event);
        emit botDone(evt->chatId, QStringLiteral("websearch"), (qint64)evt->localId,
                     evt->success,
                     evt->success ? joinWebsearchItems(evt) : QString(),
                     QString::fromUtf8(evt->backendUsed.c_str()),
                     QString::fromUtf8(evt->errorMsg.c_str()));
        return;
    }
    if (event->type() == PastebinResultType) {
        PasteResultEvent* evt = static_cast<PasteResultEvent*>(event);
        emit botDone(evt->chatId, QStringLiteral("pastebin"), (qint64)evt->localId,
                     evt->success,
                     evt->success ? QString::fromUtf8(evt->url.c_str()) : QString(),
                      QString::fromUtf8(evt->providerUsed.c_str()),
                     QString::fromUtf8(evt->errorMsg.c_str()));
        return;
    }
    if (event->type() == TtsbotDoneType) {
        TtsbotDoneEvent* evt = static_cast<TtsbotDoneEvent*>(event);
        QString caption = QString::fromUtf8("🔊 ");
        if (!evt->providerUsed.empty()) {
            caption += QString::fromUtf8(evt->providerUsed.c_str());
        }
        if (!evt->voice.empty()) {
            caption += QString::fromUtf8(" · ")
                     + QString::fromUtf8(evt->voice.c_str());
        }
        emit ttsDone(evt->chatId, (qint64)evt->localId, evt->success,
                     caption, QString::fromUtf8(evt->text.c_str()),
                     QString::fromUtf8(evt->errorMsg.c_str()),
                     QString::fromUtf8(evt->audioUrl.c_str()),
                     QString::fromUtf8(evt->mime.c_str()),
                     evt->durationSec,
                     QByteArray(reinterpret_cast<const char*>(evt->audioData.data()),
                                (int)evt->audioData.size()));
        return;
    }
    QObject::customEvent(event);
}
