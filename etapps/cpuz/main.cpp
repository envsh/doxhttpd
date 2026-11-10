#include "compat34.h"
#include "plugin_api.h"
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qmessagebox.h>

extern QWidget* createCpuMainWindow(void*);

extern "C" const char* plugin_name(void) {
    return "CPU-Z";
}

extern "C" const char* plugin_version(void) {
    return "1.0.0";
}

extern "C" const char* plugin_description(void) {
    return "CPU 硬件信息查看器";
}

extern "C" void* plugin_create(void* parent) {
    QWidget* w = createCpuMainWindow(0);
    return w;
}

extern "C" void plugin_destroy(void* widget) {
    delete (QWidget*)widget;
}
