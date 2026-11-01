#include "devstatmonitor.h"
#include "androidutils.h"
#include "mozpushsender.h"
#include <QCoreApplication>
#include <QDebug>
#include <QObject>
#include <QTimer>
#include <QMap>
#include <QString>

#if defined(Q_OS_ANDROID)

static const int kDevStatIntervalMs = 345000;   // 5min45s
static QTimer* s_devStatTimer = nullptr;

static void devStatTick()
{
    if (!MozpushSender::instance())
        return;
    QMap<QString,QString> nf;
    nf.insert(QStringLiteral("charging"),
        androidIsCharging() ? QStringLiteral("1") : QStringLiteral("0"));
    const QString level = androidBatteryLevel();
    nf.insert(QStringLiteral("level"),
        level.isEmpty() ? QStringLiteral("-1") : level);
    const QString temp = androidBatteryTemperature();
    nf.insert(QStringLiteral("temp"),
        temp.isEmpty() ? QStringLiteral("-1") : temp);
    MozpushSender::instance()->sendMobEvent(QStringLiteral("devstat"), nf);
}

#endif

void DevStatMonitor::start()
{
#if defined(Q_OS_ANDROID)
    if (s_devStatTimer)
        return;
    s_devStatTimer = new QTimer(QCoreApplication::instance());
    s_devStatTimer->setInterval(kDevStatIntervalMs);
    QObject::connect(s_devStatTimer, &QTimer::timeout, &devStatTick);
    s_devStatTimer->start();
    qDebug() << "[DevStatMonitor] started, interval" << kDevStatIntervalMs << "ms";
#else
    qDebug() << "[DevStatMonitor] not Android, skip";
#endif
}
