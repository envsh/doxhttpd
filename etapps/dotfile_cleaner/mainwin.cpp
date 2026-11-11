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

    /* 主窗销毁日志：exe 与 qltox 插件双形态共用（destroyed→LambdaSlot） */
    LambdaSlot* logSlot = new LambdaSlot(0, [] {
        qWarning("[mainwin] destroyed: DotfileCleanerMain");
    });
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(call()));
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(deleteLater()));

    return w;
}
