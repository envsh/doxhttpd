#include "compat34.h"
#include "plugin_api.h"
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qmessagebox.h>

extern "C"
QWidget* createFmtCodecMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);
    QVBoxLayout* layout = new QVBoxLayout(w);

    QLabel* lbl = new QLabel("JSON Formatter", w);
    layout->addWidget(lbl);

#ifdef QT3_BUILD
    w->setCaption("JSON Formatter");
#else
    w->setWindowTitle("JSON Formatter");
#endif
    w->resize(300, 200);
    return w;

}
