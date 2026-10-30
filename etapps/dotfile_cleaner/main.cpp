#include "compat34.h"
#include "plugin_api.h"
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qmessagebox.h>

extern "C" const char* plugin_name(void) {
    return "Dotfile Cleaner";
}

extern "C" const char* plugin_version(void) {
    return "1.0.0";
}

extern "C" const char* plugin_description(void) {
    return "dot 文件清理";
}

extern "C" void* plugin_create(void* parent) {
	extern QWidget* createDotfileCleanerMainWindow();
    QWidget* w = createDotfileCleanerMainWindow();
    return w;
}

extern "C" void plugin_destroy(void* widget) {
    delete (QWidget*)widget;
}
