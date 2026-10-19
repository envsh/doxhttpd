#ifndef MOZPUSHSENDER_H
#define MOZPUSHSENDER_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <QMap>
#include <QTimer>

class QNetworkAccessManager;

class MozpushSender : public QObject
{
    Q_OBJECT

public:
    static MozpushSender* instance();
    static void start();

    // 返回本次发送的 reqseq（未配置 uaid/endpoint 或构造失败时返回 0）。
    // 失败原因（若非空）写入 *errorOut。
    quint64 sendMessage(const QString& type, const QString& id, const QString& message,
                        const QMap<QString,QString>& attributes = QMap<QString,QString>(),
                        QString* errorOut = nullptr);

    void resume();
    void pause();

Q_SIGNALS:
    void messageSent(quint64 reqseq, bool success, const QString& error);

public slots:
    void sendNow();

private:
    explicit MozpushSender(QObject* parent = nullptr);
    void reloadSettings();
    void buildSend();
    void postRequest(const QByteArray& body, const QString& authz,
                     const QString& endpoint, quint64 reqseq, bool notify);

    QNetworkAccessManager* m_nam = nullptr;
    QTimer m_timer;
    QString m_uaid;
    QString m_endpoint;
    quint64 m_seq = 0;     // register(register action) 的 seq
    quint64 m_reqSeq = 100;   // 下一个 reqseq，从 100 开始，跨联系人全局唯一
};

#endif
