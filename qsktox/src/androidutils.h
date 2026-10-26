#ifndef ANDROID_UTILS_H
#define ANDROID_UTILS_H

#include <QString>

void showAndroidToast(const QString& message);
void installLogHandler();
QString androidDeviceName();
void androidRequestBluetoothNamePermission();
void shareText(const QString& text);

#endif
