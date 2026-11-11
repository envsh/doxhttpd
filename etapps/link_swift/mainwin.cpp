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
QWidget* createLinkSwiftMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("网盘直链下载助手"));
#else
    w->setWindowTitle(qFromUtf8("网盘直链下载助手"));
#endif
    w->resize(640, 500);

    QVBoxLayout* mainLayout = (QVBoxLayout*)qNewBoxLayout(w, QBoxLayout::TopToBottom, 8, 6);

    // ── 平台选择 ──
    QGroupBox* platBox = new QGroupBox(qFromUtf8("网盘平台"), w);
    QHBoxLayout* platLayout = (QHBoxLayout*)qNewBoxLayout(platBox, QBoxLayout::LeftToRight, 6, 4);
    QLabel* platLabel = new QLabel(qFromUtf8("平台："), platBox);
    QComboBox* platCombo = new QComboBox(platBox);
    comboAdd(platCombo, qFromUtf8("百度网盘"));
    comboAdd(platCombo, qFromUtf8("阿里云盘"));
    comboAdd(platCombo, qFromUtf8("夸克网盘"));
    comboAdd(platCombo, qFromUtf8("UC网盘"));
    comboAdd(platCombo, qFromUtf8("迅雷云盘"));
    comboAdd(platCombo, qFromUtf8("123云盘"));
    comboAdd(platCombo, qFromUtf8("天翼云盘"));
    comboAdd(platCombo, qFromUtf8("移动云盘"));
    platLayout->addWidget(platLabel);
    platLayout->addWidget(platCombo);
    platLayout->addStretch(1);
    mainLayout->addWidget(platBox);

    // ── 分享链接 ──
    QGroupBox* linkBox = new QGroupBox(qFromUtf8("分享链接"), w);
#ifdef QT3_BUILD
    QGridLayout* linkLayout = new QGridLayout(linkBox, 2, 2, 6, 4);
#else
    QGridLayout* linkLayout = new QGridLayout(linkBox);
    linkLayout->setContentsMargins(6, 6, 6, 6);
    linkLayout->setSpacing(4);
#endif
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
    QVBoxLayout* authLayout = (QVBoxLayout*)qNewBoxLayout(authBox, QBoxLayout::TopToBottom, 6, 4);
    QTextEdit* authEdit = new QTextEdit(authBox);
#ifdef QT3_BUILD
    authEdit->setTextFormat(Qt::PlainText);
#else
    authEdit->setAcceptRichText(false);
#endif
    authLayout->addWidget(authEdit);
    mainLayout->addWidget(authBox);

    // ── 下载器设置 ──
    QGroupBox* dlBox = new QGroupBox(qFromUtf8("下载器"), w);
    QHBoxLayout* dlLayout = (QHBoxLayout*)qNewBoxLayout(dlBox, QBoxLayout::LeftToRight, 6, 4);
    QLabel* dlLabel = new QLabel(qFromUtf8("类型："), dlBox);
    QComboBox* dlCombo = new QComboBox(dlBox);
    comboAdd(dlCombo, qFromUtf8("Aria2 (RPC)"));
    comboAdd(dlCombo, qFromUtf8("Motrix"));
    comboAdd(dlCombo, qFromUtf8("IDM"));
    comboAdd(dlCombo, qFromUtf8("AB Download Manager"));
    comboAdd(dlCombo, qFromUtf8("BitComet"));
    comboAdd(dlCombo, qFromUtf8("cURL"));
    QLabel* rpcLabel = new QLabel(qFromUtf8("RPC 地址："), dlBox);
    QLineEdit* rpcEdit = new QLineEdit(dlBox);
    rpcEdit->setText("http://127.0.0.1:6800/jsonrpc");
    dlLayout->addWidget(dlLabel);
    dlLayout->addWidget(dlCombo);
    dlLayout->addWidget(rpcLabel);
    dlLayout->addWidget(rpcEdit);
    mainLayout->addWidget(dlBox);

    // ── 操作按钮 ──
    QHBoxLayout* btnLayout = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 4, 4);
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

    /* 主窗销毁日志：exe 与 qltox 插件双形态共用（destroyed→LambdaSlot） */
    LambdaSlot* logSlot = new LambdaSlot(0, [] {
        qWarning("[mainwin] destroyed: LinkSwiftMain");
    });
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(call()));
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(deleteLater()));

    return w;
}
