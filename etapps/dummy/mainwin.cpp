#include "compat34.h"
#include "lambdaslot.h"
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

    /* 主窗销毁日志：exe 与 qltox 插件双形态共用（destroyed→LambdaSlot） */
    LambdaSlot* logSlot = new LambdaSlot(0, [] {
        qWarning("[mainwin] destroyed: DummyMain");
    });
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(call()));
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(deleteLater()));

    return w;
    
}
