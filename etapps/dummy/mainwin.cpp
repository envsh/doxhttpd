#include "compat34.h"
#include "plugin_api.h"
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qmessagebox.h>

extern "C"
QWidget* createDummyMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);
    QVBoxLayout* layout = new QVBoxLayout(w);

    QLabel* lbl = new QLabel("Hello from Dummy Plugin!", w);
    layout->addWidget(lbl);

    QPushButton* btn = new QPushButton("Click Me", w);
    layout->addWidget(btn);

#ifdef QT3_BUILD
    w->setCaption("Dummy Plugin");
#else
    w->setWindowTitle("Dummy Plugin");
#endif
    w->resize(300, 200);
    return w;
    
}
