#ifndef NETWORK_MONITOR_H
#define NETWORK_MONITOR_H

#include <QObject>
#include <QString>

class NetworkMonitor : public QObject
{
Q_OBJECT
public:
    static void start();
    static void stop();
    static void checkNetwork();
    // 三平台网络变化统一转发入口（Android JNI 线程 / 桌面 lambda 汇至此）：
    // lastSent(state/nettype) 去重（首次必广播）→ 编组回主线程 →
    // MozpushSender::sendNetState(state, netType)。桌面 Unknown 不广播。
    static void onNetworkChanged(bool connected, const QString& netType);
};

#endif // NETWORK_MONITOR_H
