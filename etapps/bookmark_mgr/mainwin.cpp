#include "compat34.h"
#include "plugin_api.h"
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qmessagebox.h>

// 真实 UI 创建代码（build-qt3/table_compat.o 中的上游实现）：
// dummy 组织中 UI 创建代码集中于 mainwin.cpp，此处直调上游工厂。
QWidget* createTableWidget(QWidget* parent);

extern "C"
QWidget* createBookmarkMgrMainWindow(void* parent) {
    return createTableWidget((QWidget*)parent);
}
