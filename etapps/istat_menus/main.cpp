#include "compat34.h"
#include "plugin_api.h"
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qmessagebox.h>

extern QWidget* createIstatMainWindow(void*);

extern "C" const char* plugin_name(void) {
    return "iStat Menus";
}

extern "C" const char* plugin_version(void) {
    return "1.0.0";
}

extern "C" const char* plugin_description(void) {
    return "系统监控面板";
}

extern "C" void* plugin_create(void* parent) {
    QWidget* w = createIstatMainWindow(0);
    return w;
}

extern "C" void plugin_destroy(void* widget) {
    delete (QWidget*)widget;
}