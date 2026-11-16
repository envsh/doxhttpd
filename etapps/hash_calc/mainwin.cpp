#include "compat34.h"
#include "lambdaslot.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qtextedit.h>
#include <qpushbutton.h>
#include <qcombobox.h>
#include <qgroupbox.h>
#include <qlayout.h>

/* Qt3 insertItem / Qt4 addItem 差异封装 */
static void comboAdd(QComboBox* c, const QString& s) {
#ifdef QT3_BUILD
    c->insertItem(s);
#else
    c->addItem(s);
#endif
}

extern "C"
QWidget* createHashCalcMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("哈希计算器"));
#else
    w->setWindowTitle(qFromUtf8("哈希计算器"));
#endif
    w->resize(520, 380);

    QVBoxLayout* mainLayout = (QVBoxLayout*)qNewBoxLayout(w, QBoxLayout::TopToBottom, 8, 6);

    QGroupBox* inputBox = new QGroupBox(qFromUtf8("输入"), w);
    QVBoxLayout* inputLayout = (QVBoxLayout*)qNewBoxLayout(inputBox, QBoxLayout::TopToBottom, 6, 4);
    QTextEdit* inputEdit = new QTextEdit(inputBox);
#ifdef QT3_BUILD
    inputEdit->setTextFormat(Qt::PlainText);
#else
    inputEdit->setAcceptRichText(false);
#endif
    inputLayout->addWidget(inputEdit);
    mainLayout->addWidget(inputBox);

    QHBoxLayout* optLayout = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 4, 4);
    QLabel* algoLabel = new QLabel(qFromUtf8("算法："), w);
    QComboBox* algoCombo = new QComboBox(w);
    comboAdd(algoCombo, qFromUtf8("MD5"));
    comboAdd(algoCombo, qFromUtf8("SHA1"));
    comboAdd(algoCombo, qFromUtf8("SHA256"));
    comboAdd(algoCombo, qFromUtf8("SHA512"));
    comboAdd(algoCombo, qFromUtf8("Base64"));
    optLayout->addWidget(algoLabel);
    optLayout->addWidget(algoCombo);
    optLayout->addStretch(1);
    QPushButton* calcBtn = new QPushButton(qFromUtf8("计算"), w);
    QPushButton* clearBtn = new QPushButton(qFromUtf8("清空"), w);
    optLayout->addWidget(calcBtn);
    optLayout->addWidget(clearBtn);
    mainLayout->addLayout(optLayout);

    QGroupBox* resultBox = new QGroupBox(qFromUtf8("结果"), w);
    QVBoxLayout* resultLayout = (QVBoxLayout*)qNewBoxLayout(resultBox, QBoxLayout::TopToBottom, 6, 4);
    QLineEdit* resultEdit = new QLineEdit(resultBox);
    resultEdit->setReadOnly(true);
    resultLayout->addWidget(resultEdit);
    mainLayout->addWidget(resultBox);

        /* 主窗销毁日志：exe 与 qltox 插件双形态共用（destroyed→LambdaSlot）。
     * ⚠ 坑：logSlot 必须以 w 为 parent、且绝不能用 destroyed()→deleteLater()：
     *   loader 在 destroyUiApp 里 destroy 窗口后立刻 dlclose(.so)，而 lambda 闭包的
     *   manager 函数指针编译在插件 .so 内；parentless logSlot 靠 destroyed() 里
     *   post 的 DeferredDelete 事件释放，该事件在 dlclose 之后才被事件循环处理，
     *   析构时间接调用已卸载代码 → SIGSEGV（栈顶 ?? ()）。
     *   Qt3 的 destroyed() 在 ~QObject 中先于子对象删除发出，故 parent=w 的 logSlot
     *   日志照常打印，且恰好随 w 析构一次，无跨 dlclose 的 DeferredDelete。 */
    LambdaSlot* logSlot = new LambdaSlot(w, [] {
        qWarning("[mainwin] destroyed: HashCalcMain");
    });
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(call()));

    return w;
}
