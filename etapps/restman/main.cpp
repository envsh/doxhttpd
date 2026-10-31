#include "compat34.h"
#include "plugin_api.h"
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qmessagebox.h>

extern "C" const char* plugin_name(void) {
    return "Restman";
}

extern "C" const char* plugin_version(void) {
    return "1.0.0";
}

extern "C" const char* plugin_description(void) {
    return "REST 请求管理器（历史30条/收藏完整参数）";
}

extern "C" void* plugin_create(void* parent) {
	extern QWidget* createRestmanMainWindow(void* parent);
    QWidget* w = createRestmanMainWindow(parent);
    return w;
}

extern "C" void plugin_destroy(void* widget) {
    delete (QWidget*)widget;
}
