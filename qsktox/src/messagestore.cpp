#include "messagestore.h"

MessageStore* MessageStore::s_instance = nullptr;

MessageStore::MessageStore(QObject* parent)
    : QObject(parent)
{
}

MessageStore* MessageStore::instance()
{
    if (!s_instance) {
        s_instance = new MessageStore();
    }
    return s_instance;
}

namespace {

QString storeKey(const QString& protocol, const QString& chatId)
{
    return protocol + QLatin1Char('_') + chatId;
}

} // namespace

void MessageStore::addMessage(const QString& protocol, const QString& chatId,
                              const MessageItem& item)
{
    const QString key = storeKey(protocol, chatId);
    auto& msgs = m_channelMessages[key];
    msgs.append(item);
    while (msgs.size() > MAX_MESSAGES_PER_CHANNEL) {
        msgs.removeFirst();
    }
    emit messageAdded(protocol, chatId, item);
}

QList<MessageItem> MessageStore::getMessages(const QString& protocol,
                                             const QString& chatId) const
{
    return m_channelMessages.value(storeKey(protocol, chatId));
}

int MessageStore::messageCount(const QString& protocol,
                               const QString& chatId) const
{
    return m_channelMessages.value(storeKey(protocol, chatId)).size();
}
