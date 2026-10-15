#ifndef MOZPUSHSENDER_H
#define MOZPUSHSENDER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QTimer>

class QNetworkAccessManager;

class MozpushSender : public QObject
{
    Q_OBJECT

public:
    static MozpushSender* instance();
    static void start();

    void resume();
    void pause();

public slots:
    void sendNow();

private:
    explicit MozpushSender(QObject* parent = nullptr);
    void reloadSettings();
    void buildSend();
    void postRequest(const QByteArray& body, const QString& authz,
                     const QString& endpoint);

    QNetworkAccessManager* m_nam = nullptr;
    QTimer m_timer;
    QString m_uaid;
    QString m_endpoint;
    quint64 m_seq = 0;
};

#endif
