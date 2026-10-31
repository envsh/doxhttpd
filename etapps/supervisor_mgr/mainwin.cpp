#include "compat34.h"
#include "logic.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qgroupbox.h>

extern "C"
QWidget* createSupervisorMgrMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("Supervisor 管理"));
#else
    w->setWindowTitle(qFromUtf8("Supervisor 管理"));
#endif
    w->resize(820, 580);

    QVBoxLayout* mainLayout = new QVBoxLayout(w, 6, 4);

    /* 连接区 */
    QGroupBox* connGroup = new QGroupBox(qFromUtf8("连接"), w);
    QHBoxLayout* connLayout = new QHBoxLayout(connGroup, 6, 4);
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
    QHBoxLayout* midRow = new QHBoxLayout(0, 4, 4);
    mainLayout->addLayout(midRow, 1);

    /* 左：进程列表 */
    QVBoxLayout* leftCol = new QVBoxLayout(0, 0, 2);
    midRow->addLayout(leftCol, 2);
    QLabel* procLbl = new QLabel(qFromUtf8("进程列表"), w);
    leftCol->addWidget(procLbl);
    RmListBox* procList = new RmListBox(w);
    leftCol->addWidget(procList, 1);

    /* 右：操作 + 日志 */
    QVBoxLayout* rightCol = new QVBoxLayout(0, 0, 2);
    midRow->addLayout(rightCol, 3);
    QHBoxLayout* opRow = new QHBoxLayout(0, 4, 4);
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
    logEdit->setTextFormat(Qt::PlainText);
    logEdit->setReadOnly(true);
    rightCol->addWidget(logEdit, 1);

    /* 逻辑 */
    SupervisorMgrLogic* logic = new SupervisorMgrLogic(w);
    logic->setWidgets(hostEdit, portEdit, userEdit, passEdit,
                      procList, statusLabel,
                      refreshBtn, startBtn, stopBtn, restartBtn, clearLogBtn,
                      logEdit);
    QObject::connect(refreshBtn, SIGNAL(clicked()), logic, SLOT(onConnect()));
    QObject::connect(startBtn, SIGNAL(clicked()), logic, SLOT(onStart()));
    QObject::connect(stopBtn, SIGNAL(clicked()), logic, SLOT(onStop()));
    QObject::connect(restartBtn, SIGNAL(clicked()), logic, SLOT(onRestart()));
    QObject::connect(clearLogBtn, SIGNAL(clicked()), logic, SLOT(onClearLog()));
    QObject::connect(procList, SIGNAL(selected(int)), logic, SLOT(onProcSelected()));

    return w;
}
