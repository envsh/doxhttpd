#include "netproxy.h"

#include <qmutex.h>

#include <string>

namespace {
QMutex g_mutex;
bool g_enabled = false;
std::string g_url;
}

void netProxySet(bool enabled, const std::string& url) {
    QMutexLocker lock(&g_mutex);
    g_enabled = enabled;
    g_url = url;
}

std::string netProxyFor(bool needsProxy) {
    QMutexLocker lock(&g_mutex);
    if (!needsProxy || !g_enabled || g_url.empty()) { return std::string(); }
    return g_url;
}
