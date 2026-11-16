#include "compat34.h"
#include "lambdaslot.h"
#include <qwidget.h>
#include <qlabel.h>
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
QWidget* createFmtCodecMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("格式化编解码"));
#else
    w->setWindowTitle(qFromUtf8("格式化编解码"));
#endif
    w->resize(640, 420);

    QVBoxLayout* mainLayout = (QVBoxLayout*)qNewBoxLayout(w, QBoxLayout::TopToBottom, 8, 6);

    QHBoxLayout* topLayout = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 4, 4);
    QLabel* typeLabel = new QLabel(qFromUtf8("类型："), w);
    QComboBox* typeCombo = new QComboBox(w);
    comboAdd(typeCombo, qFromUtf8("JSON"));
    comboAdd(typeCombo, qFromUtf8("XML"));
    comboAdd(typeCombo, qFromUtf8("YAML"));
    comboAdd(typeCombo, qFromUtf8("TOML"));
    topLayout->addWidget(typeLabel);
    topLayout->addWidget(typeCombo);
    topLayout->addStretch(1);
    QPushButton* fmtBtn = new QPushButton(qFromUtf8("格式化"), w);
    QPushButton* minBtn = new QPushButton(qFromUtf8("压缩"), w);
    QPushButton* clearBtn = new QPushButton(qFromUtf8("清空"), w);
    topLayout->addWidget(fmtBtn);
    topLayout->addWidget(minBtn);
    topLayout->addWidget(clearBtn);
    mainLayout->addLayout(topLayout);

    QHBoxLayout* editLayout = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 6, 6);

    QGroupBox* inBox = new QGroupBox(qFromUtf8("输入"), w);
    QVBoxLayout* inLayout = (QVBoxLayout*)qNewBoxLayout(inBox, QBoxLayout::TopToBottom, 4, 2);
    QTextEdit* inEdit = new QTextEdit(inBox);
#ifdef QT3_BUILD
    inEdit->setTextFormat(Qt::PlainText);
#else
    inEdit->setAcceptRichText(false);
#endif
    inLayout->addWidget(inEdit);
    editLayout->addWidget(inBox);

    QGroupBox* outBox = new QGroupBox(qFromUtf8("输出"), w);
    QVBoxLayout* outLayout = (QVBoxLayout*)qNewBoxLayout(outBox, QBoxLayout::TopToBottom, 4, 2);
    QTextEdit* outEdit = new QTextEdit(outBox);
#ifdef QT3_BUILD
    outEdit->setTextFormat(Qt::PlainText);
#else
    outEdit->setAcceptRichText(false);
#endif
    outEdit->setReadOnly(true);
    outLayout->addWidget(outEdit);
    editLayout->addWidget(outBox);

    mainLayout->addLayout(editLayout);

        /* 主窗销毁日志：exe 与 qltox 插件双形态共用（destroyed→LambdaSlot）。
     * ⚠ 坑：logSlot 必须以 w 为 parent、且绝不能用 destroyed()→deleteLater()：
     *   loader 在 destroyUiApp 里 destroy 窗口后立刻 dlclose(.so)，而 lambda 闭包的
     *   manager 函数指针编译在插件 .so 内；parentless logSlot 靠 destroyed() 里
     *   post 的 DeferredDelete 事件释放，该事件在 dlclose 之后才被事件循环处理，
     *   析构时间接调用已卸载代码 → SIGSEGV（栈顶 ?? ()）。
     *   Qt3 的 destroyed() 在 ~QObject 中先于子对象删除发出，故 parent=w 的 logSlot
     *   日志照常打印，且恰好随 w 析构一次，无跨 dlclose 的 DeferredDelete。 */
    LambdaSlot* logSlot = new LambdaSlot(w, [] {
        qWarning("[mainwin] destroyed: FmtCodecMain");
    });
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(call()));

    return w;
}
