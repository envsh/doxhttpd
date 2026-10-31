#include "compat34.h"
#include "logic.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qgroupbox.h>
#ifdef QT3_BUILD
#include <qtable.h>
#include <qheader.h>
#else
#include <qtablewidget.h>
#endif

extern "C"
QWidget* createSupervisorMgrMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("Supervisor 管理"));
#else
    w->setWindowTitle(qFromUtf8("Supervisor 管理"));
#endif
    w->resize(820, 580);

    QBoxLayout* mainLayout = qNewBoxLayout(w, QBoxLayout::TopToBottom, 6, 4);

    /* 连接区 */
    QGroupBox* connGroup = new QGroupBox(qFromUtf8("连接"), w);
    QBoxLayout* connLayout = qNewBoxLayout(connGroup, QBoxLayout::LeftToRight, 6, 4);
    QLabel* hostLbl = new QLabel(qFromUtf8("主机:"), connGroup);
    QLineEdit* hostEdit = new QLineEdit(connGroup);
    hostEdit->setText("127.0.0.1");
    QLabel* portLbl = new QLabel(qFromUtf8("端口:"), connGroup);
    QLineEdit* portEdit = new QLineEdit(connGroup);
    portEdit->setText("9001");
    portEdit->setMaximumWidth(60);
    QLabel* userLbl = new QLabel(qFromUtf8("用户:"), connGroup);
    QLineEdit* userEdit = new QLineEdit(connGroup);
    userEdit->setMaximumWidth(90);
    QLabel* passLbl = new QLabel(qFromUtf8("密码:"), connGroup);
    QLineEdit* passEdit = new QLineEdit(connGroup);
    passEdit->setMaximumWidth(90);
    passEdit->setEchoMode(QLineEdit::Password);
    QPushButton* refreshBtn = new QPushButton(qFromUtf8("连接/刷新"), connGroup);
    QLabel* statusLabel = new QLabel(qFromUtf8("未连接"), connGroup);
    connLayout->addWidget(hostLbl);
    connLayout->addWidget(hostEdit);
    connLayout->addWidget(portLbl);
    connLayout->addWidget(portEdit);
    connLayout->addWidget(userLbl);
    connLayout->addWidget(userEdit);
    connLayout->addWidget(passLbl);
    connLayout->addWidget(passEdit);
    connLayout->addWidget(refreshBtn);
    connLayout->addWidget(statusLabel);
    connLayout->addStretch(1);
    mainLayout->addWidget(connGroup);

    /* 中部 */
    QBoxLayout* midRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    mainLayout->addLayout(midRow, 1);

    /* 左：进程列表（多列 QTable） */
    QBoxLayout* leftCol = qNewBoxLayout(0, QBoxLayout::TopToBottom, 0, 2);
    midRow->addLayout(leftCol, 2);
    QLabel* procLbl = new QLabel(qFromUtf8("进程列表"), w);
    leftCol->addWidget(procLbl);
#ifdef QT3_BUILD
    QTable* procTable = new QTable(0, 4, w);
    procTable->horizontalHeader()->setLabel(0, qFromUtf8("进程名"));
    procTable->horizontalHeader()->setLabel(1, qFromUtf8("状态"));
    procTable->horizontalHeader()->setLabel(2, qFromUtf8("PID"));
    procTable->horizontalHeader()->setLabel(3, qFromUtf8("描述"));
    procTable->setColumnWidth(0, 160);
    procTable->setColumnWidth(1, 100);
    procTable->setColumnWidth(2, 70);
    procTable->setColumnWidth(3, 280);
    procTable->setColumnReadOnly(0, true);
    procTable->setColumnReadOnly(1, true);
    procTable->setColumnReadOnly(2, true);
    procTable->setColumnReadOnly(3, true);
    procTable->setSorting(false);
#else
    QTableWidget* procTable = new QTableWidget(0, 4, w);
    QStringList tblLabels;
    tblLabels << qFromUtf8("进程名") << qFromUtf8("状态") << qFromUtf8("PID") << qFromUtf8("描述");
    procTable->setHorizontalHeaderLabels(tblLabels);
    procTable->setColumnWidth(0, 160);
    procTable->setColumnWidth(1, 100);
    procTable->setColumnWidth(2, 70);
    procTable->setColumnWidth(3, 280);
    procTable->setSortingEnabled(false);
    procTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
#endif
    leftCol->addWidget(procTable, 1);

    /* 右：操作 + 日志 */
    QBoxLayout* rightCol = qNewBoxLayout(0, QBoxLayout::TopToBottom, 0, 2);
    midRow->addLayout(rightCol, 3);
    QBoxLayout* opRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 0, 4);
    QPushButton* startBtn = new QPushButton(qFromUtf8("启动"), w);
    QPushButton* stopBtn = new QPushButton(qFromUtf8("停止"), w);
    QPushButton* restartBtn = new QPushButton(qFromUtf8("重启"), w);
    QPushButton* clearLogBtn = new QPushButton(qFromUtf8("清空日志"), w);
    opRow->addWidget(startBtn);
    opRow->addWidget(stopBtn);
    opRow->addWidget(restartBtn);
    opRow->addStretch(1);
    opRow->addWidget(clearLogBtn);
    rightCol->addLayout(opRow);
    QLabel* logLbl = new QLabel(qFromUtf8("日志"), w);
    rightCol->addWidget(logLbl);
    QTextEdit* logEdit = new QTextEdit(w);
#ifdef QT3_BUILD
    logEdit->setTextFormat(Qt::PlainText);
#endif
    logEdit->setReadOnly(true);
    rightCol->addWidget(logEdit, 1);

    /* 逻辑 */
    SupervisorMgrLogic* logic = new SupervisorMgrLogic(w);
    logic->setWidgets(hostEdit, portEdit, userEdit, passEdit,
                      procTable, statusLabel,
                      refreshBtn, startBtn, stopBtn, restartBtn, clearLogBtn,
                      logEdit);
    QObject::connect(refreshBtn, SIGNAL(clicked()), logic, SLOT(onConnect()));
    QObject::connect(startBtn, SIGNAL(clicked()), logic, SLOT(onStart()));
    QObject::connect(stopBtn, SIGNAL(clicked()), logic, SLOT(onStop()));
    QObject::connect(restartBtn, SIGNAL(clicked()), logic, SLOT(onRestart()));
    QObject::connect(clearLogBtn, SIGNAL(clicked()), logic, SLOT(onClearLog()));
#ifdef QT3_BUILD
    QObject::connect(procTable, SIGNAL(selectionChanged()), logic, SLOT(onProcSelected()));
#else
    QObject::connect(procTable, SIGNAL(itemSelectionChanged()), logic, SLOT(onProcSelected()));
    QObject::connect(procTable, SIGNAL(cellClicked(int,int)), logic, SLOT(onProcSelected()));
#endif

    return w;
}
