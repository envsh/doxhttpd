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
    // push 编辑（m.replace）命中：内存桶按 msgId 原地改 content。
    // 命中返回 true 并发 messageContentUpdated；未命中（进程内未见原稿）
    // 返回 false 由调用方退化普通 addMessage 新行路径（Matrix 语义）。
    // 现无持久改写链（schema 零改约束：msgId↔event_id 无映射）。
    bool updateContent(const QString& protocol, const QString& chatId,
                       const QString& msgId, const QString& newContent);
    QList<MessageItem> getMessages(const QString& protocol,
                                   const QString& chatId) const;
    int messageCount(const QString& protocol, const QString& chatId) const;

Q_SIGNALS:
    void messageAdded(const QString& protocol, const QString& chatId,
                      const MessageItem& item);
    void messageContentUpdated(const QString& protocol, const QString& chatId,
                               const QString& msgId, const QString& newContent);

private:
    MessageStore(QObject* parent = nullptr);
    static MessageStore* s_instance;
    // key = "<protocol>_<chatId>"
    QMap<QString, QList<MessageItem>> m_channelMessages;
};

#endif
