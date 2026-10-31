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
#include <qcheckbox.h>

extern "C"
QWidget* createV2raySuberMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("V2Ray 订阅管理器"));
#else
    w->setWindowTitle(qFromUtf8("V2Ray 订阅管理器"));
#endif
    w->resize(640, 480);

    QVBoxLayout* mainLayout = new QVBoxLayout(w, 8, 6);

    QTabWidget* tabs = new QTabWidget(w);
    mainLayout->addWidget(tabs);

    /* ── Tab: 订阅 ── */
    QWidget* subPage = new QWidget(tabs);
    QVBoxLayout* subLayout = new QVBoxLayout(subPage, 6, 4);
    QHBoxLayout* subTop = new QHBoxLayout(0, 4, 4);
    QLineEdit* subEdit = new QLineEdit(subPage);
    subTop->addWidget(subEdit);
    QPushButton* addBtn = new QPushButton(qFromUtf8("添加"), subPage);
    QPushButton* delBtn = new QPushButton(qFromUtf8("删除"), subPage);
    QPushButton* updBtn = new QPushButton(qFromUtf8("更新选中订阅"), subPage);
    QPushButton* updAllBtn = new QPushButton(qFromUtf8("更新全部订阅"), subPage);
    subTop->addWidget(addBtn);
    subTop->addWidget(delBtn);
    subTop->addWidget(updBtn);
    subTop->addWidget(updAllBtn);
    subLayout->addLayout(subTop);
    V2ListBox* subList = new V2ListBox(subPage);
    subLayout->addWidget(subList);
    QLabel* subHint = new QLabel(qFromUtf8("提示：输入订阅链接点击“添加”；支持“更新选中”与“更新全部”（串行后台执行，每小时自动更新）"), subPage);
    subLayout->addWidget(subHint);
    tabs->addTab(subPage, qFromUtf8("订阅"));

    /* ── Tab: 节点 ── */
    QWidget* nodePage = new QWidget(tabs);
    QVBoxLayout* nodeLayout = new QVBoxLayout(nodePage, 6, 4);
    V2ListBox* nodeList = new V2ListBox(nodePage);
    nodeLayout->addWidget(nodeList);
    QHBoxLayout* nodeBtnRow = new QHBoxLayout(0, 4, 4);
    QPushButton* copyBtn = new QPushButton(qFromUtf8("复制节点链接"), nodePage);
    QPushButton* latBtn = new QPushButton(qFromUtf8("测试延迟（选中）"), nodePage);
    nodeBtnRow->addWidget(copyBtn);
    nodeBtnRow->addWidget(latBtn);
    nodeBtnRow->addStretch(1);
    nodeLayout->addLayout(nodeBtnRow);
    QLabel* nodeHint = new QLabel(qFromUtf8("提示：选中节点后点击“启动核心”即可生效（SOCKS 127.0.0.1:10808）；测速为 curl TCP 实连耗时"), nodePage);
    nodeLayout->addWidget(nodeHint);
    tabs->addTab(nodePage, qFromUtf8("节点"));

    /* ── Tab: 核心 ── */
    QWidget* corePage = new QWidget(tabs);
    QVBoxLayout* coreLayout = new QVBoxLayout(corePage, 6, 4);
    QLabel* coreState = new QLabel(qFromUtf8("状态：检测中"), corePage);
    coreLayout->addWidget(coreState);
    QHBoxLayout* coreBtnRow = new QHBoxLayout(0, 4, 4);
    QPushButton* dlBtn = new QPushButton(qFromUtf8("下载安装"), corePage);
    QPushButton* startBtn = new QPushButton(qFromUtf8("启动核心"), corePage);
    QPushButton* stopBtn = new QPushButton(qFromUtf8("停止核心"), corePage);
    coreBtnRow->addWidget(dlBtn);
    coreBtnRow->addWidget(startBtn);
    coreBtnRow->addWidget(stopBtn);
    coreBtnRow->addStretch(1);
    coreLayout->addLayout(coreBtnRow);
    QCheckBox* bypassCn = new QCheckBox(qFromUtf8("绕过中国大陆（geosite:cn / geoip:cn 直连，使用核心包自带 geo 文件）"), corePage);
    coreLayout->addWidget(bypassCn);
    QLabel* coreHint = new QLabel(qFromUtf8("说明：核心版本固定 v5.35.0，下载使用 gh-proxy.com 加速前缀；geoip/geosite 随核心包解压，无需单独下载"), corePage);
    coreLayout->addWidget(coreHint);
    coreLayout->addStretch(1);
    tabs->addTab(corePage, qFromUtf8("核心"));

    /* ── Tab: 日志 ── */
    QWidget* logPage = new QWidget(tabs);
    QVBoxLayout* logLayout = new QVBoxLayout(logPage, 6, 4);
    QTextEdit* logEdit = new QTextEdit(logPage);
    logEdit->setTextFormat(Qt::PlainText);
    logEdit->setReadOnly(true);
    logLayout->addWidget(logEdit);
    tabs->addTab(logPage, qFromUtf8("日志"));

    /* ── 逻辑关联 ── */
    V2RayLogic* logic = new V2RayLogic(w);
    logic->setWidgets(subEdit, subList, nodeList, coreState, logEdit, startBtn, stopBtn);
    logic->setRouteWidgets(bypassCn);
    QObject::connect(addBtn, SIGNAL(clicked()), logic, SLOT(onAddSubscription()));
    QObject::connect(delBtn, SIGNAL(clicked()), logic, SLOT(onDelSubscription()));
    QObject::connect(updBtn, SIGNAL(clicked()), logic, SLOT(onUpdateSubscription()));
    QObject::connect(updAllBtn, SIGNAL(clicked()), logic, SLOT(onUpdateAllSubscriptions()));
    QObject::connect(copyBtn, SIGNAL(clicked()), logic, SLOT(onCopyNodeLink()));
    QObject::connect(latBtn, SIGNAL(clicked()), logic, SLOT(onTestLatency()));
    QObject::connect(dlBtn, SIGNAL(clicked()), logic, SLOT(onDownloadCore()));
    QObject::connect(startBtn, SIGNAL(clicked()), logic, SLOT(onStartCore()));
    QObject::connect(stopBtn, SIGNAL(clicked()), logic, SLOT(onStopCore()));

    return w;
}
