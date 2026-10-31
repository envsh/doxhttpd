#ifndef ANDROID_UTILS_H
#define ANDROID_UTILS_H

#include <QString>

void showAndroidToast(const QString& message);
void installLogHandler();
QString androidDeviceName();
void androidRequestBluetoothNamePermission();
void shareText(const QString& text);
// 打开外部文件（Android 系统播放器/阅读器跳转）——FileProvider content uri
//（manifest :90-97 已有 androidx FileProvider，authority=<pkg>.qtprovider）
void openFile(const QString& path, const QString& mime);
// 应用 cache 目录绝对路径（Android=JNI getCacheDir；桌面返回 "" 由调用方兜底）
QString androidCacheDir();
// Root 检测（MobUtil.isRooted）；非 Android 恒 false
bool androidIsRooted();
// 充电状态（BatteryManager.isCharging）；非 Android 恒 false
bool androidIsCharging();

#endif
