#include "compat34.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qtextedit.h>
#include <qpushbutton.h>
#include <qcombobox.h>
#include <qgroupbox.h>
#include <qlayout.h>

extern "C"
QWidget* createLinkSwiftMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("网盘直链下载助手"));
#else
    w->setWindowTitle(qFromUtf8("网盘直链下载助手"));
#endif
    w->resize(640, 500);

    QVBoxLayout* mainLayout = new QVBoxLayout(w, 8, 6);

    // ── 平台选择 ──
    QGroupBox* platBox = new QGroupBox(qFromUtf8("网盘平台"), w);
    QHBoxLayout* platLayout = new QHBoxLayout(platBox, 6, 4);
    QLabel* platLabel = new QLabel(qFromUtf8("平台："), platBox);
    QComboBox* platCombo = new QComboBox(platBox);
    platCombo->insertItem(qFromUtf8("百度网盘"));
    platCombo->insertItem(qFromUtf8("阿里云盘"));
    platCombo->insertItem(qFromUtf8("夸克网盘"));
    platCombo->insertItem(qFromUtf8("UC网盘"));
    platCombo->insertItem(qFromUtf8("迅雷云盘"));
    platCombo->insertItem(qFromUtf8("123云盘"));
    platCombo->insertItem(qFromUtf8("天翼云盘"));
    platCombo->insertItem(qFromUtf8("移动云盘"));
    platLayout->addWidget(platLabel);
    platLayout->addWidget(platCombo);
    platLayout->addStretch(1);
    mainLayout->addWidget(platBox);

    // ── 分享链接 ──
    QGroupBox* linkBox = new QGroupBox(qFromUtf8("分享链接"), w);
    QGridLayout* linkLayout = new QGridLayout(linkBox, 2, 2, 6, 4);
    QLabel* urlLabel = new QLabel(qFromUtf8("链接："), linkBox);
    QLineEdit* urlEdit = new QLineEdit(linkBox);
    urlEdit->setText(qFromUtf8("请输入网盘分享链接"));
    QLabel* pwdLabel = new QLabel(qFromUtf8("提取码："), linkBox);
    QLineEdit* pwdEdit = new QLineEdit(linkBox);
    linkLayout->addWidget(urlLabel, 0, 0);
    linkLayout->addWidget(urlEdit, 0, 1);
    linkLayout->addWidget(pwdLabel, 1, 0);
    linkLayout->addWidget(pwdEdit, 1, 1);
    mainLayout->addWidget(linkBox);

    // ── 认证信息 ──
    QGroupBox* authBox = new QGroupBox(qFromUtf8("认证信息（Cookie/Token）"), w);
    QVBoxLayout* authLayout = new QVBoxLayout(authBox, 6, 4);
    QTextEdit* authEdit = new QTextEdit(authBox);
    authEdit->setTextFormat(Qt::PlainText);
    authLayout->addWidget(authEdit);
    mainLayout->addWidget(authBox);

    // ── 下载器设置 ──
    QGroupBox* dlBox = new QGroupBox(qFromUtf8("下载器"), w);
    QHBoxLayout* dlLayout = new QHBoxLayout(dlBox, 6, 4);
    QLabel* dlLabel = new QLabel(qFromUtf8("类型："), dlBox);
    QComboBox* dlCombo = new QComboBox(dlBox);
    dlCombo->insertItem(qFromUtf8("Aria2 (RPC)"));
    dlCombo->insertItem(qFromUtf8("Motrix"));
    dlCombo->insertItem(qFromUtf8("IDM"));
    dlCombo->insertItem(qFromUtf8("AB Download Manager"));
    dlCombo->insertItem(qFromUtf8("BitComet"));
    dlCombo->insertItem(qFromUtf8("cURL"));
    QLabel* rpcLabel = new QLabel(qFromUtf8("RPC 地址："), dlBox);
    QLineEdit* rpcEdit = new QLineEdit(dlBox);
    rpcEdit->setText("http://127.0.0.1:6800/jsonrpc");
    dlLayout->addWidget(dlLabel);
    dlLayout->addWidget(dlCombo);
    dlLayout->addWidget(rpcLabel);
    dlLayout->addWidget(rpcEdit);
    mainLayout->addWidget(dlBox);

    // ── 操作按钮 ──
    QHBoxLayout* btnLayout = new QHBoxLayout(0, 4, 4);
    QPushButton* fetchBtn = new QPushButton(qFromUtf8("获取直链"), w);
    QPushButton* copyBtn = new QPushButton(qFromUtf8("复制全部"), w);
    QPushButton* pushBtn = new QPushButton(qFromUtf8("推送到下载器"), w);
    QPushButton* clearBtn = new QPushButton(qFromUtf8("清空"), w);
    btnLayout->addWidget(fetchBtn);
    btnLayout->addWidget(copyBtn);
    btnLayout->addWidget(pushBtn);
    btnLayout->addWidget(clearBtn);
    btnLayout->addStretch(1);
    mainLayout->addLayout(btnLayout);

    // ── 结果/日志 ──
    QGroupBox* logBox = new QGroupBox(qFromUtf8("直链结果"), w);
    QVBoxLayout* logLayout = new QVBoxLayout(logBox, 6, 4);
    QTextEdit* logEdit = new QTextEdit(logBox);
    logEdit->setTextFormat(Qt::PlainText);
    logEdit->setReadOnly(true);
    logLayout->addWidget(logEdit);
    mainLayout->addWidget(logBox);

    return w;
}
