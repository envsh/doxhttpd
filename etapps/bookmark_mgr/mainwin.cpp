#include "compat34.h"
#include "lambdaslot.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include "qlistwidget34.h"
#include <qgroupbox.h>
#include <qlayout.h>

extern "C"
QWidget* createBookmarkMgrMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("书签管理"));
#else
    w->setWindowTitle(qFromUtf8("书签管理"));
#endif
    w->resize(560, 420);

    QVBoxLayout* mainLayout = (QVBoxLayout*)qNewBoxLayout(w, QBoxLayout::TopToBottom, 8, 6);

    QGroupBox* listBox = new QGroupBox(qFromUtf8("书签列表"), w);
    QVBoxLayout* listLayout = (QVBoxLayout*)qNewBoxLayout(listBox, QBoxLayout::TopToBottom, 6, 4);
    QListWidget34* listBoxCtrl = new QListWidget34(listBox);
    listLayout->addWidget(listBoxCtrl);
    mainLayout->addWidget(listBox);

    QHBoxLayout* btnLayout = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 4, 4);
    QPushButton* addBtn = new QPushButton(qFromUtf8("添加"), w);
    QPushButton* delBtn = new QPushButton(qFromUtf8("删除"), w);
    QPushButton* openBtn = new QPushButton(qFromUtf8("打开"), w);
    QPushButton* refreshBtn = new QPushButton(qFromUtf8("刷新"), w);
    btnLayout->addWidget(addBtn);
    btnLayout->addWidget(delBtn);
    btnLayout->addWidget(openBtn);
    btnLayout->addWidget(refreshBtn);
    btnLayout->addStretch(1);
    mainLayout->addLayout(btnLayout);

        /* 主窗销毁日志：exe 与 qltox 插件双形态共用（destroyed→LambdaSlot）。
     * ⚠ 坑：logSlot 必须以 w 为 parent、且绝不能用 destroyed()→deleteLater()：
     *   loader 在 destroyUiApp 里 destroy 窗口后立刻 dlclose(.so)，而 lambda 闭包的
     *   manager 函数指针编译在插件 .so 内；parentless logSlot 靠 destroyed() 里
     *   post 的 DeferredDelete 事件释放，该事件在 dlclose 之后才被事件循环处理，
     *   析构时间接调用已卸载代码 → SIGSEGV（栈顶 ?? ()）。
     *   Qt3 的 destroyed() 在 ~QObject 中先于子对象删除发出，故 parent=w 的 logSlot
     *   日志照常打印，且恰好随 w 析构一次，无跨 dlclose 的 DeferredDelete。 */
    LambdaSlot* logSlot = new LambdaSlot(w, [] {
        qWarning("[mainwin] destroyed: BookmarkMgrMain");
    });
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(call()));

    return w;
}
