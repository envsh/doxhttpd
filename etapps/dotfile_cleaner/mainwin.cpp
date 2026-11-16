#include "compat34.h"
#include "lambdaslot.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include "qlistwidget34.h"
#include <qcheckbox.h>
#include <qgroupbox.h>
#include <qlayout.h>

extern "C"
QWidget* createDotfileCleanerMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("Dotfile 清理"));
#else
    w->setWindowTitle(qFromUtf8("Dotfile 清理"));
#endif
    w->resize(500, 420);

    QVBoxLayout* mainLayout = (QVBoxLayout*)qNewBoxLayout(w, QBoxLayout::TopToBottom, 8, 6);

    QGroupBox* fileBox = new QGroupBox(qFromUtf8("待清理文件"), w);
    QVBoxLayout* fileLayout = (QVBoxLayout*)qNewBoxLayout(fileBox, QBoxLayout::TopToBottom, 6, 4);
    QListWidget34* fileList = new QListWidget34(fileBox);
    fileList->addItem(qFromUtf8(".bash_history"));
    fileList->addItem(qFromUtf8(".zsh_history"));
    fileList->addItem(qFromUtf8(".cache/"));
    fileList->addItem(qFromUtf8(".local/share/Trash/"));
    fileLayout->addWidget(fileList);
    mainLayout->addWidget(fileBox);

    QHBoxLayout* btnLayout = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 4, 4);
    QPushButton* scanBtn = new QPushButton(qFromUtf8("扫描"), w);
    QPushButton* cleanBtn = new QPushButton(qFromUtf8("清理选中"), w);
    QPushButton* cleanAllBtn = new QPushButton(qFromUtf8("全部清理"), w);
    btnLayout->addWidget(scanBtn);
    btnLayout->addWidget(cleanBtn);
    btnLayout->addWidget(cleanAllBtn);
    btnLayout->addStretch(1);
    mainLayout->addLayout(btnLayout);

    QLabel* statusLabel = new QLabel(qFromUtf8("状态：就绪"), w);
    mainLayout->addWidget(statusLabel);

        /* 主窗销毁日志：exe 与 qltox 插件双形态共用（destroyed→LambdaSlot）。
     * ⚠ 坑：logSlot 必须以 w 为 parent、且绝不能用 destroyed()→deleteLater()：
     *   loader 在 destroyUiApp 里 destroy 窗口后立刻 dlclose(.so)，而 lambda 闭包的
     *   manager 函数指针编译在插件 .so 内；parentless logSlot 靠 destroyed() 里
     *   post 的 DeferredDelete 事件释放，该事件在 dlclose 之后才被事件循环处理，
     *   析构时间接调用已卸载代码 → SIGSEGV（栈顶 ?? ()）。
     *   Qt3 的 destroyed() 在 ~QObject 中先于子对象删除发出，故 parent=w 的 logSlot
     *   日志照常打印，且恰好随 w 析构一次，无跨 dlclose 的 DeferredDelete。 */
    LambdaSlot* logSlot = new LambdaSlot(w, [] {
        qWarning("[mainwin] destroyed: DotfileCleanerMain");
    });
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(call()));

    return w;
}
