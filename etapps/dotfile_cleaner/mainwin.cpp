#include "compat34.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlistbox.h>
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

    QVBoxLayout* mainLayout = new QVBoxLayout(w, 8, 6);

    QGroupBox* fileBox = new QGroupBox(qFromUtf8("待清理文件"), w);
    QVBoxLayout* fileLayout = new QVBoxLayout(fileBox, 6, 4);
    QListBox* fileList = new QListBox(fileBox);
    fileList->insertItem(qFromUtf8(".bash_history"));
    fileList->insertItem(qFromUtf8(".zsh_history"));
    fileList->insertItem(qFromUtf8(".cache/"));
    fileList->insertItem(qFromUtf8(".local/share/Trash/"));
    fileLayout->addWidget(fileList);
    mainLayout->addWidget(fileBox);

    QHBoxLayout* btnLayout = new QHBoxLayout(0, 4, 4);
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

    return w;
}
