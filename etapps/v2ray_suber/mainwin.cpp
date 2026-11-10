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
#include <qcheckbox.h>
#include <qcombobox.h>

/* Qt3 insertItem / Qt4 addItem 差异封装 */
static void V2ComboAdd(QComboBox* c, const QString& s) {
#ifdef QT3_BUILD
    c->insertItem(s);
#else
    c->addItem(s);
#endif
}

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
    QLabel* nodeHint = new QLabel(qFromUtf8("提示：选中节点后点击“启动核心”即可生效（SOCKS 监听地址/端口见“核心”页）；测速为 curl TCP 实连耗时"), nodePage);
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

    /* D：SOCKS 监听地址 / 端口 */
    QHBoxLayout* listenRow = new QHBoxLayout(0, 4, 4);
    listenRow->addWidget(new QLabel(qFromUtf8("监听地址/端口："), corePage));
    QComboBox* listenAddr = new QComboBox(corePage);
    listenAddr->setEditable(true);
    V2ComboAdd(listenAddr, "127.0.0.1");   /* 仅本机（默认） */
    V2ComboAdd(listenAddr, "0.0.0.0");     /* 局域网可访问 */
    V2ComboAdd(listenAddr, "::1");         /* IPv6 本机 */
    listenAddr->setFixedWidth(140);
    listenRow->addWidget(listenAddr);
    QLineEdit* listenPort = new QLineEdit("10808", corePage);
    listenPort->setFixedWidth(70);
    listenRow->addWidget(listenPort);
    listenRow->addStretch(1);
    coreLayout->addLayout(listenRow);
    coreLayout->addWidget(new QLabel(qFromUtf8("默认 127.0.0.1:10808 仅本机；0.0.0.0 会向局域网暴露 SOCKS（含 UDP），请谨慎"), corePage));
    QCheckBox* bypassCn = new QCheckBox(qFromUtf8("绕过中国大陆（geosite:cn / geoip:cn 直连，使用核心包自带 geo 文件）"), corePage);
    bypassCn->setChecked(true);            /* 默认勾选 */
    coreLayout->addWidget(bypassCn);
    QCheckBox* bypassLan = new QCheckBox(qFromUtf8("绕过局域网（私有地址直连，不依赖 geo 文件）"), corePage);
    bypassLan->setChecked(true);           /* 默认勾选 */
    coreLayout->addWidget(bypassLan);
    QLabel* coreHint = new QLabel(qFromUtf8("说明：核心版本固定 v5.35.0，下载优先 gh-proxy.com 加速（失败自动直连 GitHub）；监听地址/端口可在上方修改"), corePage);
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
    logic->setRouteWidgets(bypassCn, bypassLan);
    logic->setListenWidgets(listenAddr, listenPort);
    /* B：控件变化即时落盘（另外 aboutToQuit 再兜底一次） */
    QObject::connect(bypassCn, SIGNAL(toggled(bool)), logic, SLOT(onSaveSettings()));
    QObject::connect(bypassLan, SIGNAL(toggled(bool)), logic, SLOT(onSaveSettings()));
    QObject::connect(listenPort, SIGNAL(returnPressed()), logic, SLOT(onSaveSettings()));
    QObject::connect(listenAddr, SIGNAL(activated(int)), logic, SLOT(onSaveSettings()));
#ifdef QT3_BUILD
    QObject::connect(listenAddr, SIGNAL(textChanged(const QString&)), logic, SLOT(onSaveSettings()));
#else
    QObject::connect(listenAddr, SIGNAL(editTextChanged(const QString&)), logic, SLOT(onSaveSettings()));
#endif
    QObject::connect(addBtn, SIGNAL(clicked()), logic, SLOT(onAddSubscription()));
    QObject::connect(delBtn, SIGNAL(clicked()), logic, SLOT(onDelSubscription()));
    QObject::connect(updBtn, SIGNAL(clicked()), logic, SLOT(onUpdateSubscription()));
    QObject::connect(updAllBtn, SIGNAL(clicked()), logic, SLOT(onUpdateAllSubscriptions()));
    QObject::connect(copyBtn, SIGNAL(clicked()), logic, SLOT(onCopyNodeLink()));
    QObject::connect(latBtn, SIGNAL(clicked()), logic, SLOT(onTestLatency()));
    QObject::connect(dlBtn, SIGNAL(clicked()), logic, SLOT(onDownloadCore()));
    QObject::connect(startBtn, SIGNAL(clicked()), logic, SLOT(onStartCore()));
    QObject::connect(stopBtn, SIGNAL(clicked()), logic, SLOT(onStopCore()));

    /* 主窗销毁日志：exe 与 qltox 插件双形态共用（destroyed→LambdaSlot） */
    LambdaSlot* logSlot = new LambdaSlot(0, [] {
        qWarning("[mainwin] destroyed: V2raySuberMain");
    });
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(call()));
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(deleteLater()));

    return w;
}
