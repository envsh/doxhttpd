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

        /* 主窗销毁日志：exe 与 qltox 插件双形态共用（destroyed→LambdaSlot）。
     * ⚠ 坑：logSlot 必须以 w 为 parent、且绝不能用 destroyed()→deleteLater()：
     *   loader 在 destroyUiApp 里 destroy 窗口后立刻 dlclose(.so)，而 lambda 闭包的
     *   manager 函数指针编译在插件 .so 内；parentless logSlot 靠 destroyed() 里
     *   post 的 DeferredDelete 事件释放，该事件在 dlclose 之后才被事件循环处理，
     *   析构时间接调用已卸载代码 → SIGSEGV（栈顶 ?? ()）。
     *   Qt3 的 destroyed() 在 ~QObject 中先于子对象删除发出，故 parent=w 的 logSlot
     *   日志照常打印，且恰好随 w 析构一次，无跨 dlclose 的 DeferredDelete。 */
    LambdaSlot* logSlot = new LambdaSlot(w, [] {
        qWarning("[mainwin] destroyed: DummyMain");
    });
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(call()));

    return w;
    
}
