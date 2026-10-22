#ifndef MESSAGE_STORE_H
#define MESSAGE_STORE_H

#include "messagelist.h"
#include <QObject>
#include <QMap>
#include <QList>

class MessageStore : public QObject
{
    Q_OBJECT
public:
    static MessageStore* instance();

    static constexpr int MAX_MESSAGES_PER_CHANNEL = 80;
    static constexpr int MAX_CHANNELS = 200;

    // 按 protocol + chatId 分桶：不同 protocol 的会话 ID 可能相同（纯数字），
    // 只按 chatId 存会把不同会话的消息混在一起。
    void addMessage(const QString& protocol, const QString& chatId,
                    const MessageItem& item);
    QList<MessageItem> getMessages(const QString& protocol,
                                  const QString& chatId) const;
    int messageCount(const QString& protocol, const QString& chatId) const;

Q_SIGNALS:
    void messageAdded(const QString& protocol, const QString& chatId,
                      const MessageItem& item);

private:
    MessageStore(QObject* parent = nullptr);
    static MessageStore* s_instance;
    // key = "<protocol>_<chatId>"
    QMap<QString, QList<MessageItem>> m_channelMessages;
};

#endif
