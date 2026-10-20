#ifndef MESSAGE_PAGE_H
#define MESSAGE_PAGE_H

#include "page.h"

class MessageListWidget;
class MessageAttribBar;
class QskTextField;
class QskTextLabel;

class MessagePage : public Page
{
    Q_OBJECT
public:
    MessagePage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;
    void onNewIntent(const QVariantMap& launchArgs) override;

private:
    void sendMessage();
    void sendToVirtualBot(int kind, qint64 localId, const QString& text);
    void onBotDone(int chatId, const QString& protocol, qint64 localId,
                   bool success, const QString& text,
                   const QString& providerUsed, const QString& errorMsg);
    void onTtsDone(int chatId, qint64 localId, bool success,
                   const QString& caption, const QString& text,
                   const QString& errorMsg,
                   const QString& remoteUrl, const QString& mime,
                   int durationSec, const QByteArray& audioData);
    static void onRowidBackfilled(void* ctx, const QString& protocol,
                                  const QString& chatId, qint64 localId,
                                  int64_t rowid);

    MessageListWidget* m_messageList = nullptr;
    MessageAttribBar* m_attrBar = nullptr;
    QskTextField* m_input = nullptr;
    QskTextLabel* m_titleLabel = nullptr;
    QString m_chatId;
    QString m_chatType;
    QString m_protocol;
};

#endif
