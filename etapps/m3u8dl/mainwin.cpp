#include "compat34.h"
#include "lambdaslot.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qtextedit.h>
#include <qgroupbox.h>
#include <qlayout.h>

extern "C"
QWidget* createM3u8dlMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("M3U8 下载器"));
#else
    w->setWindowTitle(qFromUtf8("M3U8 下载器"));
#endif
    w->resize(560, 420);

    QVBoxLayout* mainLayout = (QVBoxLayout*)qNewBoxLayout(w, QBoxLayout::TopToBottom, 8, 6);

    QGroupBox* urlBox = new QGroupBox(qFromUtf8("下载地址"), w);
    QVBoxLayout* urlLayout = (QVBoxLayout*)qNewBoxLayout(urlBox, QBoxLayout::TopToBottom, 6, 4);
    QLineEdit* urlEdit = new QLineEdit(urlBox);
    urlEdit->setText(qFromUtf8("请输入 m3u8 地址"));
    urlLayout->addWidget(urlEdit);
    mainLayout->addWidget(urlBox);

    QGroupBox* dirBox = new QGroupBox(qFromUtf8("保存目录"), w);
    QHBoxLayout* dirLayout = (QHBoxLayout*)qNewBoxLayout(dirBox, QBoxLayout::LeftToRight, 6, 4);
    QLineEdit* dirEdit = new QLineEdit(dirBox);
    dirEdit->setText(qFromUtf8("./download"));
    QPushButton* browseBtn = new QPushButton(qFromUtf8("浏览..."), dirBox);
    dirLayout->addWidget(dirEdit);
    dirLayout->addWidget(browseBtn);
    mainLayout->addWidget(dirBox);

    QHBoxLayout* btnLayout = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 4, 4);
    QPushButton* startBtn = new QPushButton(qFromUtf8("开始下载"), w);
    QPushButton* stopBtn = new QPushButton(qFromUtf8("停止"), w);
    btnLayout->addWidget(startBtn);
    btnLayout->addWidget(stopBtn);
    btnLayout->addStretch(1);
    mainLayout->addLayout(btnLayout);

    QGroupBox* logBox = new QGroupBox(qFromUtf8("日志"), w);
    QVBoxLayout* logLayout = (QVBoxLayout*)qNewBoxLayout(logBox, QBoxLayout::TopToBottom, 6, 4);
    QTextEdit* logEdit = new QTextEdit(logBox);
#ifdef QT3_BUILD
    logEdit->setTextFormat(Qt::PlainText);
#else
    logEdit->setAcceptRichText(false);
#endif
    logEdit->setReadOnly(true);
    logLayout->addWidget(logEdit);
    mainLayout->addWidget(logBox);

        /* 主窗销毁日志：exe 与 qltox 插件双形态共用（destroyed→LambdaSlot）。
     * ⚠ 坑：logSlot 必须以 w 为 parent、且绝不能用 destroyed()→deleteLater()：
     *   loader 在 destroyUiApp 里 destroy 窗口后立刻 dlclose(.so)，而 lambda 闭包的
     *   manager 函数指针编译在插件 .so 内；parentless logSlot 靠 destroyed() 里
     *   post 的 DeferredDelete 事件释放，该事件在 dlclose 之后才被事件循环处理，
     *   析构时间接调用已卸载代码 → SIGSEGV（栈顶 ?? ()）。
     *   Qt3 的 destroyed() 在 ~QObject 中先于子对象删除发出，故 parent=w 的 logSlot
     *   日志照常打印，且恰好随 w 析构一次，无跨 dlclose 的 DeferredDelete。 */
    LambdaSlot* logSlot = new LambdaSlot(w, [] {
        qWarning("[mainwin] destroyed: m3u8dlMain");
    });
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(call()));

    return w;
}
