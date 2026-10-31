#include "compat34.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlistbox.h>
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

    QVBoxLayout* mainLayout = new QVBoxLayout(w, 8, 6);

    QGroupBox* listBox = new QGroupBox(qFromUtf8("书签列表"), w);
    QVBoxLayout* listLayout = new QVBoxLayout(listBox, 6, 4);
    QListBox* listBoxCtrl = new QListBox(listBox);
    listLayout->addWidget(listBoxCtrl);
    mainLayout->addWidget(listBox);

    QHBoxLayout* btnLayout = new QHBoxLayout(0, 4, 4);
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

    return w;
}
