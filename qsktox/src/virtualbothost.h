#ifndef VIRTUAL_BOT_HOST_H
#define VIRTUAL_BOT_HOST_H

#include <QObject>
#include <QString>

class VirtualBotHost : public QObject
{
    Q_OBJECT
public:
    static VirtualBotHost* instance();
    static qint64 nextLocalId();

Q_SIGNALS:
    void botDone(int chatId, const QString& protocol, qint64 localId,
                 bool success, const QString& text,
                 const QString& providerUsed, const QString& errorMsg);
    void ttsDone(int chatId, qint64 localId, bool success,
                 const QString& caption, const QString& text,
                 const QString& errorMsg);

protected:
    void customEvent(QEvent* event) override;

private:
    VirtualBotHost();
};

#endif // VIRTUAL_BOT_HOST_H
