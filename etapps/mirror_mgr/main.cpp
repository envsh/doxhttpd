#include "compat34.h"
#include "plugin_api.h"
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qmessagebox.h>

extern "C" const char* plugin_name(void) {
    return "Mirror Manager";
}

extern "C" const char* plugin_version(void) {
    return "1.0.0";
}

extern "C" const char* plugin_description(void) {
    return "镜像管理";
}

extern "C" void* plugin_create(void* parent) {
	extern QWidget* createMirrorMgrMainWindow();
    QWidget* w = createMirrorMgrMainWindow();
    return w;
}

extern "C" void plugin_destroy(void* widget) {
    delete (QWidget*)widget;
}
