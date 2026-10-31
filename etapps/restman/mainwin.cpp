#include "compat34.h"
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

extern "C"
QWidget* createRestmanMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("REST 请求管理器"));
#else
    w->setWindowTitle(qFromUtf8("REST 请求管理器"));
#endif
    w->resize(880, 620);

    QVBoxLayout* mainLayout = new QVBoxLayout(w, 8, 6);

    /* ── 顶部请求栏 ── */
    QHBoxLayout* reqRow = new QHBoxLayout(0, 4, 4);
    QComboBox* methodBox = new QComboBox(w);
    methodBox->insertItem("GET");
    methodBox->insertItem("POST");
    methodBox->insertItem("PUT");
    methodBox->insertItem("PATCH");
    methodBox->insertItem("DELETE");
    methodBox->insertItem("HEAD");
    methodBox->insertItem("OPTIONS");
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
    QHBoxLayout* midRow = new QHBoxLayout(0, 4, 4);
    mainLayout->addLayout(midRow, 1);

    /* 左：历史 + 收藏 */
    QVBoxLayout* leftCol = new QVBoxLayout(0, 0, 4);
    midRow->addLayout(leftCol, 2);

    QVBoxLayout* histCol = new QVBoxLayout(0, 0, 2);
    QLabel* histLbl = new QLabel(qFromUtf8("历史（最近 30 条）"), w);
    histCol->addWidget(histLbl);
    RmListBox* histList = new RmListBox(w);
    histCol->addWidget(histList, 1);
    QHBoxLayout* histBtnRow = new QHBoxLayout(0, 2, 2);
    QPushButton* hLoadBtn = new QPushButton(qFromUtf8("加载"), w);
    QPushButton* hDelBtn = new QPushButton(qFromUtf8("删除"), w);
    QPushButton* hClrBtn = new QPushButton(qFromUtf8("清空"), w);
    histBtnRow->addWidget(hLoadBtn);
    histBtnRow->addWidget(hDelBtn);
    histBtnRow->addWidget(hClrBtn);
    histCol->addLayout(histBtnRow);
    leftCol->addLayout(histCol, 1);

    QVBoxLayout* bmCol = new QVBoxLayout(0, 0, 2);
    QLabel* bmLbl = new QLabel(qFromUtf8("收藏（含完整参数）"), w);
    bmCol->addWidget(bmLbl);
    RmListBox* bmList = new RmListBox(w);
    bmCol->addWidget(bmList, 1);
    QHBoxLayout* bmBtnRow = new QHBoxLayout(0, 2, 2);
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
    QVBoxLayout* queryLayout = new QVBoxLayout(queryPage, 4, 2);
    QLabel* queryHint = new QLabel(qFromUtf8("每行一项：key=value（自动拼接到 URL）"), queryPage);
    queryLayout->addWidget(queryHint);
    QTextEdit* queryEdit = new QTextEdit(queryPage);
    queryEdit->setTextFormat(Qt::PlainText);
    queryLayout->addWidget(queryEdit);
    reqTabs->addTab(queryPage, qFromUtf8("Query"));

    QWidget* headerPage = new QWidget(reqTabs);
    QVBoxLayout* headerLayout = new QVBoxLayout(headerPage, 4, 2);
    QLabel* headerHint = new QLabel(qFromUtf8("每行一条：Key: Value"), headerPage);
    headerLayout->addWidget(headerHint);
    QTextEdit* headerEdit = new QTextEdit(headerPage);
    headerEdit->setTextFormat(Qt::PlainText);
    headerLayout->addWidget(headerEdit);
    reqTabs->addTab(headerPage, qFromUtf8("Headers"));

    QWidget* bodyPage = new QWidget(reqTabs);
    QVBoxLayout* bodyLayout = new QVBoxLayout(bodyPage, 4, 2);
    QHBoxLayout* bodyTypeRow = new QHBoxLayout(0, 2, 2);
    QComboBox* bodyTypeBox = new QComboBox(bodyPage);
    bodyTypeBox->insertItem(qFromUtf8("无"));
    bodyTypeBox->insertItem(qFromUtf8("原始(raw)"));
    bodyTypeBox->insertItem(qFromUtf8("表单(urlencoded)"));
    bodyTypeRow->addWidget(bodyTypeBox);
    bodyTypeRow->addStretch(1);
    bodyLayout->addLayout(bodyTypeRow);
    QTextEdit* bodyEdit = new QTextEdit(bodyPage);
    bodyEdit->setTextFormat(Qt::PlainText);
    bodyLayout->addWidget(bodyEdit, 1);
    reqTabs->addTab(bodyPage, qFromUtf8("Body"));

    QWidget* authPage = new QWidget(reqTabs);
    QVBoxLayout* authLayout = new QVBoxLayout(authPage, 4, 2);
    QHBoxLayout* authTypeRow = new QHBoxLayout(0, 2, 2);
    QComboBox* authTypeBox = new QComboBox(authPage);
    authTypeBox->insertItem(qFromUtf8("无"));
    authTypeBox->insertItem(qFromUtf8("Basic"));
    authTypeRow->addWidget(authTypeBox);
    authTypeRow->addStretch(1);
    authLayout->addLayout(authTypeRow);
    QHBoxLayout* authUserRow = new QHBoxLayout(0, 2, 2);
    QLabel* authUserLbl = new QLabel("User:", authPage);
    QLineEdit* authUserEdit = new QLineEdit(authPage);
    authUserRow->addWidget(authUserLbl);
    authUserRow->addWidget(authUserEdit);
    authLayout->addLayout(authUserRow);
    QHBoxLayout* authPassRow = new QHBoxLayout(0, 2, 2);
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
    QVBoxLayout* respCol = new QVBoxLayout(0, 0, 2);
    mainLayout->addLayout(respCol, 1);
    QLabel* respState = new QLabel(qFromUtf8("状态：待发送"), w);
    respCol->addWidget(respState);
    QTabWidget* respTabs = new QTabWidget(w);
    respCol->addWidget(respTabs, 1);

    QWidget* respBodyPage = new QWidget(respTabs);
    QVBoxLayout* respBodyLayout = new QVBoxLayout(respBodyPage, 4, 2);
    QTextEdit* respBody = new QTextEdit(respBodyPage);
    respBody->setTextFormat(Qt::PlainText);
    respBody->setReadOnly(true);
    respBodyLayout->addWidget(respBody);
    respTabs->addTab(respBodyPage, qFromUtf8("响应 Body"));

    QWidget* respHeadersPage = new QWidget(respTabs);
    QVBoxLayout* respHeadersLayout = new QVBoxLayout(respHeadersPage, 4, 2);
    QTextEdit* respHeaders = new QTextEdit(respHeadersPage);
    respHeaders->setTextFormat(Qt::PlainText);
    respHeaders->setReadOnly(true);
    respHeadersLayout->addWidget(respHeaders);
    respTabs->addTab(respHeadersPage, qFromUtf8("响应 Headers"));

    QWidget* respRawPage = new QWidget(respTabs);
    QVBoxLayout* respRawLayout = new QVBoxLayout(respRawPage, 4, 2);
    QTextEdit* respRaw = new QTextEdit(respRawPage);
    respRaw->setTextFormat(Qt::PlainText);
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

    return w;
}
