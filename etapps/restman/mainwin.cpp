#include "compat34.h"
#include "lambdaslot.h"
#include "logic.h"
#include <qwidget.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qtextedit.h>
#include <qtabwidget.h>
#include <qgroupbox.h>
#include <qlayout.h>
#include <qcombobox.h>

/* Qt3 insertItem / Qt4 addItem 差异封装 */
static void rmComboAdd(QComboBox* c, const QString& s) {
#ifdef QT3_BUILD
    c->insertItem(s);
#else
    c->addItem(s);
#endif
}

extern "C"
QWidget* createRestmanMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("REST 请求管理器"));
#else
    w->setWindowTitle(qFromUtf8("REST 请求管理器"));
#endif
    w->resize(880, 620);

    QVBoxLayout* mainLayout = (QVBoxLayout*)qNewBoxLayout(w, QBoxLayout::TopToBottom, 8, 6);

    /* ── 顶部请求栏 ── */
    QHBoxLayout* reqRow = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 4, 4);
    QComboBox* methodBox = new QComboBox(w);
    rmComboAdd(methodBox, "GET");
    rmComboAdd(methodBox, "POST");
    rmComboAdd(methodBox, "PUT");
    rmComboAdd(methodBox, "PATCH");
    rmComboAdd(methodBox, "DELETE");
    rmComboAdd(methodBox, "HEAD");
    rmComboAdd(methodBox, "OPTIONS");
    reqRow->addWidget(methodBox);
    QLineEdit* urlEdit = new QLineEdit(w);
    urlEdit->setMinimumWidth(320);
    urlEdit->setText("http://");
    reqRow->addWidget(urlEdit);
    QPushButton* sendBtn = new QPushButton(qFromUtf8("发送"), w);
    QPushButton* bmBtn = new QPushButton(qFromUtf8("保存收藏"), w);
    reqRow->addWidget(sendBtn);
    reqRow->addWidget(bmBtn);
    mainLayout->addLayout(reqRow);

    /* ── 中部：左历史/收藏，右编辑区 ── */
    QHBoxLayout* midRow = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 4, 4);
    mainLayout->addLayout(midRow, 1);

    /* 左：历史 + 收藏 */
    QVBoxLayout* leftCol = (QVBoxLayout*)qNewBoxLayout(0, QBoxLayout::TopToBottom, 0, 4);
    midRow->addLayout(leftCol, 2);

    QVBoxLayout* histCol = (QVBoxLayout*)qNewBoxLayout(0, QBoxLayout::TopToBottom, 0, 2);
    QLabel* histLbl = new QLabel(qFromUtf8("历史（最近 30 条）"), w);
    histCol->addWidget(histLbl);
    RmListBox* histList = new RmListBox(w);
    histCol->addWidget(histList, 1);
    QHBoxLayout* histBtnRow = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 2);
    QPushButton* hLoadBtn = new QPushButton(qFromUtf8("加载"), w);
    QPushButton* hDelBtn = new QPushButton(qFromUtf8("删除"), w);
    QPushButton* hClrBtn = new QPushButton(qFromUtf8("清空"), w);
    histBtnRow->addWidget(hLoadBtn);
    histBtnRow->addWidget(hDelBtn);
    histBtnRow->addWidget(hClrBtn);
    histCol->addLayout(histBtnRow);
    leftCol->addLayout(histCol, 1);

    QVBoxLayout* bmCol = (QVBoxLayout*)qNewBoxLayout(0, QBoxLayout::TopToBottom, 0, 2);
    QLabel* bmLbl = new QLabel(qFromUtf8("收藏（含完整参数）"), w);
    bmCol->addWidget(bmLbl);
    RmListBox* bmList = new RmListBox(w);
    bmCol->addWidget(bmList, 1);
    QHBoxLayout* bmBtnRow = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 2);
    QPushButton* bLoadBtn = new QPushButton(qFromUtf8("加载"), w);
    QPushButton* bDelBtn = new QPushButton(qFromUtf8("删除"), w);
    bmBtnRow->addWidget(bLoadBtn);
    bmBtnRow->addWidget(bDelBtn);
    bmCol->addLayout(bmBtnRow);
    leftCol->addLayout(bmCol, 1);

    /* 右：请求编辑区 */
    QTabWidget* reqTabs = new QTabWidget(w);
    midRow->addWidget(reqTabs, 3);

    QWidget* queryPage = new QWidget(reqTabs);
    QVBoxLayout* queryLayout = (QVBoxLayout*)qNewBoxLayout(queryPage, QBoxLayout::TopToBottom, 4, 2);
    QLabel* queryHint = new QLabel(qFromUtf8("每行一项：key=value（自动拼接到 URL）"), queryPage);
    queryLayout->addWidget(queryHint);
    QTextEdit* queryEdit = new QTextEdit(queryPage);
#ifdef QT3_BUILD
    queryEdit->setTextFormat(Qt::PlainText);
#else
    queryEdit->setAcceptRichText(false);
#endif
    queryLayout->addWidget(queryEdit);
    reqTabs->addTab(queryPage, qFromUtf8("Query"));

    QWidget* headerPage = new QWidget(reqTabs);
    QVBoxLayout* headerLayout = (QVBoxLayout*)qNewBoxLayout(headerPage, QBoxLayout::TopToBottom, 4, 2);
    QLabel* headerHint = new QLabel(qFromUtf8("每行一条：Key: Value"), headerPage);
    headerLayout->addWidget(headerHint);
    QTextEdit* headerEdit = new QTextEdit(headerPage);
#ifdef QT3_BUILD
    headerEdit->setTextFormat(Qt::PlainText);
#else
    headerEdit->setAcceptRichText(false);
#endif
    headerLayout->addWidget(headerEdit);
    reqTabs->addTab(headerPage, qFromUtf8("Headers"));

    QWidget* bodyPage = new QWidget(reqTabs);
    QVBoxLayout* bodyLayout = (QVBoxLayout*)qNewBoxLayout(bodyPage, QBoxLayout::TopToBottom, 4, 2);
    QHBoxLayout* bodyTypeRow = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 2);
    QComboBox* bodyTypeBox = new QComboBox(bodyPage);
    rmComboAdd(bodyTypeBox, qFromUtf8("无"));
    rmComboAdd(bodyTypeBox, qFromUtf8("原始(raw)"));
    rmComboAdd(bodyTypeBox, qFromUtf8("表单(urlencoded)"));
    bodyTypeRow->addWidget(bodyTypeBox);
    bodyTypeRow->addStretch(1);
    bodyLayout->addLayout(bodyTypeRow);
    QTextEdit* bodyEdit = new QTextEdit(bodyPage);
#ifdef QT3_BUILD
    bodyEdit->setTextFormat(Qt::PlainText);
#else
    bodyEdit->setAcceptRichText(false);
#endif
    bodyLayout->addWidget(bodyEdit, 1);
    reqTabs->addTab(bodyPage, qFromUtf8("Body"));

    QWidget* authPage = new QWidget(reqTabs);
    QVBoxLayout* authLayout = (QVBoxLayout*)qNewBoxLayout(authPage, QBoxLayout::TopToBottom, 4, 2);
    QHBoxLayout* authTypeRow = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 2);
    QComboBox* authTypeBox = new QComboBox(authPage);
    rmComboAdd(authTypeBox, qFromUtf8("无"));
    rmComboAdd(authTypeBox, qFromUtf8("Basic"));
    authTypeRow->addWidget(authTypeBox);
    authTypeRow->addStretch(1);
    authLayout->addLayout(authTypeRow);
    QHBoxLayout* authUserRow = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 2);
    QLabel* authUserLbl = new QLabel("User:", authPage);
    QLineEdit* authUserEdit = new QLineEdit(authPage);
    authUserRow->addWidget(authUserLbl);
    authUserRow->addWidget(authUserEdit);
    authLayout->addLayout(authUserRow);
    QHBoxLayout* authPassRow = (QHBoxLayout*)qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 2);
    QLabel* authPassLbl = new QLabel("Pass:", authPage);
    QLineEdit* authPassEdit = new QLineEdit(authPage);
    authPassRow->addWidget(authPassLbl);
    authPassRow->addWidget(authPassEdit);
    authLayout->addLayout(authPassRow);
    QLabel* authHint = new QLabel(qFromUtf8("Basic 认证将以 curl --user 传递"), authPage);
    authLayout->addWidget(authHint);
    authLayout->addStretch(1);
    reqTabs->addTab(authPage, qFromUtf8("Auth"));

    /* ── 底部：响应区 ── */
    QVBoxLayout* respCol = (QVBoxLayout*)qNewBoxLayout(0, QBoxLayout::TopToBottom, 0, 2);
    mainLayout->addLayout(respCol, 1);
    QLabel* respState = new QLabel(qFromUtf8("状态：待发送"), w);
    respCol->addWidget(respState);
    QTabWidget* respTabs = new QTabWidget(w);
    respCol->addWidget(respTabs, 1);

    QWidget* respBodyPage = new QWidget(respTabs);
    QVBoxLayout* respBodyLayout = (QVBoxLayout*)qNewBoxLayout(respBodyPage, QBoxLayout::TopToBottom, 4, 2);
    QTextEdit* respBody = new QTextEdit(respBodyPage);
#ifdef QT3_BUILD
    respBody->setTextFormat(Qt::PlainText);
#else
    respBody->setAcceptRichText(false);
#endif
    respBody->setReadOnly(true);
    respBodyLayout->addWidget(respBody);
    respTabs->addTab(respBodyPage, qFromUtf8("响应 Body"));

    QWidget* respHeadersPage = new QWidget(respTabs);
    QVBoxLayout* respHeadersLayout = (QVBoxLayout*)qNewBoxLayout(respHeadersPage, QBoxLayout::TopToBottom, 4, 2);
    QTextEdit* respHeaders = new QTextEdit(respHeadersPage);
#ifdef QT3_BUILD
    respHeaders->setTextFormat(Qt::PlainText);
#else
    respHeaders->setAcceptRichText(false);
#endif
    respHeaders->setReadOnly(true);
    respHeadersLayout->addWidget(respHeaders);
    respTabs->addTab(respHeadersPage, qFromUtf8("响应 Headers"));

    QWidget* respRawPage = new QWidget(respTabs);
    QVBoxLayout* respRawLayout = (QVBoxLayout*)qNewBoxLayout(respRawPage, QBoxLayout::TopToBottom, 4, 2);
    QTextEdit* respRaw = new QTextEdit(respRawPage);
#ifdef QT3_BUILD
    respRaw->setTextFormat(Qt::PlainText);
#else
    respRaw->setAcceptRichText(false);
#endif
    respRaw->setReadOnly(true);
    respRawLayout->addWidget(respRaw);
    respTabs->addTab(respRawPage, qFromUtf8("日志/原始"));

    /* ── 逻辑关联 ── */
    RestmanLogic* logic = new RestmanLogic(w);
    logic->setWidgets(methodBox, urlEdit, queryEdit, headerEdit,
                      bodyTypeBox, bodyEdit,
                      authTypeBox, authUserEdit, authPassEdit,
                      histList, bmList,
                      respState, respBody, respHeaders, respRaw,
                      sendBtn);
    QObject::connect(sendBtn, SIGNAL(clicked()), logic, SLOT(onSend()));
    QObject::connect(bmBtn, SIGNAL(clicked()), logic, SLOT(onSaveBookmark()));
    QObject::connect(bLoadBtn, SIGNAL(clicked()), logic, SLOT(onLoadBookmark()));
    QObject::connect(bDelBtn, SIGNAL(clicked()), logic, SLOT(onDelBookmark()));
    QObject::connect(hLoadBtn, SIGNAL(clicked()), logic, SLOT(onLoadHistory()));
    QObject::connect(hDelBtn, SIGNAL(clicked()), logic, SLOT(onDelHistory()));
    QObject::connect(hClrBtn, SIGNAL(clicked()), logic, SLOT(onClearHistory()));

    /* 主窗销毁日志：exe 与 qltox 插件双形态共用（destroyed→LambdaSlot） */
    LambdaSlot* logSlot = new LambdaSlot(0, [] {
        qWarning("[mainwin] destroyed: RestmanMain");
    });
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(call()));
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(deleteLater()));

    return w;
}
