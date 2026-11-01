#ifndef DEVSTAT_MONITOR_H
#define DEVSTAT_MONITOR_H

// 定时（345s）上报设备充电/电量/温度：action=devstat，仅 Android 启用；
// 桌面空操作。envelope 由 MozpushSender::sendMobEvent 统一（静默 notify=false）。
class DevStatMonitor
{
public:
    static void start();
};

#endif // DEVSTAT_MONITOR_H
