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
#ifdef QT3_BUILD
#include <qpopupmenu.h>
#include <qtooltip.h>
#else
#include <qmenu.h>
#endif
#include <qcursor.h>

/* Qt3 没有 QWidget::setToolTip，用 QToolTip::add 兼容 */
static void v2Tip(QWidget* w, const QString& tip) {
    if (!w) { return; }
#ifdef QT3_BUILD
    QToolTip::add(w, tip);
#else
    w->setToolTip(tip);
#endif
}

/* Qt3 insertItem / Qt4 addItem 差异封装 */
static void V2ComboAdd(QComboBox* c, const QString& s) {
#ifdef QT3_BUILD
    c->insertItem(s);
#else
    c->addItem(s);
#endif
}

#ifdef QT3_BUILD
/* 国旗列单元格 tooltip：显示国家信息（国家列已隐藏） */
class V2FlagTip : public QToolTip {
public:
    V2FlagTip(V2Table* t, V2RayLogic* lg, bool fav)
        : QToolTip(t->viewport()), m_t(t), m_logic(lg), m_fav(fav), m_col(2) {}
    void maybeTip(const QPoint& p) {
        if (!m_t || !m_logic) { return; }
        /* QTable 的 viewport 是 4000x4000 裁剪 viewport（基准 -2000,-2000），
           p 需经官方 API 转内容坐标，rect 也须用 contentsToViewport 回写 */
        QPoint cp = m_t->viewportToContents(p);
        int r = m_t->rowAt(cp.y());
        int c = m_t->columnAt(cp.x());
        if (r < 0 || c != m_col) { hide(); return; }
        QString tipText = m_fav ? m_logic->favCountryTip(r)
                                : m_logic->nodeCountryTip(r);
        if (tipText.isEmpty()) { hide(); return; }
        QRect cr = m_t->cellGeometry(r, c);
        QRect vr(m_t->contentsToViewport(cr.topLeft()), cr.size());
        QToolTip::tip(vr, tipText);
    }
private:
    V2Table* m_t;
    V2RayLogic* m_logic;
    bool m_fav;
    int  m_col;
};
#endif

/* 表格右键：按光标落点选中行，供菜单动作使用 */
static void V2PrepareRow(V2Table* t) {
    if (!t) { return; }
    QPoint gp = QCursor::pos();
#ifdef QT3_BUILD
    /* QTable::rowAt 用内容坐标：视口坐标 + 垂直滚动量（含表头偏移修正） */
    int y = t->contentsY() + t->viewport()->mapFromGlobal(gp).y();
    int r = t->rowAt(y);
#else
    /* QTableView::rowAt 用视口坐标 */
    int r = t->rowAt(t->viewport()->mapFromGlobal(gp).y());
#endif
    if (r < 0) { r = t->currentRow(); }   /* 键盘唤起/光标不在表内时回退 */
    if (r >= 0) { t->setCurrentCell(r, 0); }
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
    /* 自动更新开关 + 频率 */
    QHBoxLayout* autoRow = new QHBoxLayout(0, 4, 4);
    QCheckBox* autoChk = new QCheckBox(qFromUtf8("自动更新订阅"), subPage);
    autoChk->setChecked(true);
    QComboBox* autoFreq = new QComboBox(subPage);
    V2ComboAdd(autoFreq, qFromUtf8("30 分钟"));
    V2ComboAdd(autoFreq, qFromUtf8("1 小时"));
    V2ComboAdd(autoFreq, qFromUtf8("2 小时"));
    V2ComboAdd(autoFreq, qFromUtf8("3 小时"));
    V2ComboAdd(autoFreq, qFromUtf8("6 小时"));
    V2ComboAdd(autoFreq, qFromUtf8("12 小时"));
    V2ComboAdd(autoFreq, qFromUtf8("24 小时"));
    autoRow->addWidget(autoChk);
    autoRow->addWidget(new QLabel(qFromUtf8("频率："), subPage));
    autoRow->addWidget(autoFreq);
    autoRow->addStretch(1);
    subLayout->addLayout(autoRow);
#ifdef QT3_BUILD
    V2Table* subList = new QTable(0, 0, subPage);
    subList->setReadOnly(true);
    subList->setSelectionMode(QTable::SingleRow);
#else
    V2Table* subList = new QTableWidget(subPage);
    subList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    subList->setSelectionBehavior(QAbstractItemView::SelectRows);
    subList->setSelectionMode(QAbstractItemView::SingleSelection);
#endif
    subLayout->addWidget(subList);
    QLabel* subHint = new QLabel(qFromUtf8("提示：输入订阅链接点击“添加”；支持“更新选中”与“更新全部”（串行后台执行；可设置自动更新开关/频率，默认每 2 小时）"), subPage);
    subLayout->addWidget(subHint);
    tabs->addTab(subPage, qFromUtf8("订阅"));

    /* ── Tab: 节点 ── */
    QWidget* nodePage = new QWidget(tabs);
    QVBoxLayout* nodeLayout = new QVBoxLayout(nodePage, 6, 4);
    /* 过滤 + 排序行（视图层，不动节点数据） */
    QHBoxLayout* nodeFilterRow = new QHBoxLayout(0, 4, 4);
    nodeFilterRow->addWidget(new QLabel(qFromUtf8("过滤："), nodePage));
    QLineEdit* nodeFilterEdit = new QLineEdit(nodePage);
    nodeFilterRow->addWidget(nodeFilterEdit);
    nodeFilterRow->addWidget(new QLabel(qFromUtf8("排序："), nodePage));
    QComboBox* nodeSortBox = new QComboBox(nodePage);
    V2ComboAdd(nodeSortBox, qFromUtf8("默认顺序"));
    V2ComboAdd(nodeSortBox, qFromUtf8("真连延时 ↑"));
    V2ComboAdd(nodeSortBox, qFromUtf8("TCP 延时 ↑"));
    V2ComboAdd(nodeSortBox, qFromUtf8("名称 A→Z"));
    nodeFilterRow->addWidget(nodeSortBox);
    nodeFilterRow->addStretch(1);
    nodeLayout->addLayout(nodeFilterRow);
#ifdef QT3_BUILD
    V2Table* nodeList = new QTable(0, 0, nodePage);
    nodeList->setReadOnly(true);
    nodeList->setSelectionMode(QTable::SingleRow);
#else
    V2Table* nodeList = new QTableWidget(nodePage);
    nodeList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    nodeList->setSelectionBehavior(QAbstractItemView::SelectRows);
    nodeList->setSelectionMode(QAbstractItemView::SingleSelection);
#endif
    nodeLayout->addWidget(nodeList);
    QHBoxLayout* nodeBtnRow = new QHBoxLayout(0, 4, 4);
    QPushButton* copyBtn = new QPushButton(qFromUtf8("复制节点链接"), nodePage);
    QPushButton* tcpSelBtn = new QPushButton(qFromUtf8("TCP(选中)"), nodePage);
    QPushButton* realSelBtn = new QPushButton(qFromUtf8("真连接(选中)"), nodePage);
    QPushButton* tcpAllBtn = new QPushButton(qFromUtf8("批量TCP"), nodePage);
    QPushButton* realAllBtn = new QPushButton(qFromUtf8("批量真连"), nodePage);
    QPushButton* stopTestBtn = new QPushButton(qFromUtf8("停止测试"), nodePage);
    nodeBtnRow->addWidget(copyBtn);
    nodeBtnRow->addWidget(tcpSelBtn);
    nodeBtnRow->addWidget(realSelBtn);
    nodeBtnRow->addWidget(tcpAllBtn);
    nodeBtnRow->addWidget(realAllBtn);
    nodeBtnRow->addWidget(stopTestBtn);
    nodeBtnRow->addStretch(1);
    nodeLayout->addLayout(nodeBtnRow);
    QLabel* nodeStatus = new QLabel(qFromUtf8("状态：就绪"), nodePage);
    nodeLayout->addWidget(nodeStatus);
    QLabel* nodeHint = new QLabel(qFromUtf8("提示：选中节点后点击“启动核心”即可生效（SOCKS 监听地址/端口见“核心”页）；测速为 curl TCP 实连耗时"), nodePage);
    nodeLayout->addWidget(nodeHint);
    tabs->addTab(nodePage, qFromUtf8("节点"));

    /* ── Tab: 收藏 ── */
    QWidget* favPage = new QWidget(tabs);
    QVBoxLayout* favLayout = new QVBoxLayout(favPage, 6, 4);
#ifdef QT3_BUILD
    V2Table* favList = new QTable(0, 0, favPage);
    favList->setReadOnly(true);
    favList->setSelectionMode(QTable::SingleRow);
#else
    V2Table* favList = new QTableWidget(favPage);
    favList->setEditTriggers(QAbstractItemView::NoEditTriggers);
    favList->setSelectionBehavior(QAbstractItemView::SelectRows);
    favList->setSelectionMode(QAbstractItemView::SingleSelection);
#endif
    favLayout->addWidget(favList);
    QLabel* favHint = new QLabel(qFromUtf8("提示：在“节点”页右键“加入/取消收藏”；此处右键可启动核心/复制链接/取消收藏"), favPage);
    favLayout->addWidget(favHint);
    tabs->addTab(favPage, qFromUtf8("收藏"));

    /* ── Tab: 核心 ── */
    QWidget* corePage = new QWidget(tabs);
    QVBoxLayout* coreLayout = new QVBoxLayout(corePage, 6, 4);
    QLabel* coreState = new QLabel(qFromUtf8("状态：检测中"), corePage);
    coreLayout->addWidget(coreState);
    /* 当前使用的节点（独立一行，不参与测试结果覆盖） */
    QLabel* activeNodeLabel = new QLabel(qFromUtf8("当前节点：—"), corePage);
    coreLayout->addWidget(activeNodeLabel);
    /* 实时流量（速率/累计/请求数），核心未运行时显示占位 */
    QLabel* trafficLabel = new QLabel(qFromUtf8("流量：—（核心未运行）"), corePage);
    coreLayout->addWidget(trafficLabel);
    QHBoxLayout* coreBtnRow = new QHBoxLayout(0, 4, 4);
    QPushButton* dlBtn = new QPushButton(qFromUtf8("下载安装"), corePage);
    QPushButton* startBtn = new QPushButton(qFromUtf8("启动核心"), corePage);
    QPushButton* stopBtn = new QPushButton(qFromUtf8("停止核心"), corePage);
    coreBtnRow->addWidget(dlBtn);
    coreBtnRow->addWidget(startBtn);
    coreBtnRow->addWidget(stopBtn);
    coreBtnRow->addStretch(1);
    coreLayout->addLayout(coreBtnRow);

    /* 内核选择：v2ray / Xray（系统）/ Xray（内置） */
    QHBoxLayout* coreSelRow = new QHBoxLayout(0, 4, 4);
    coreSelRow->addWidget(new QLabel(qFromUtf8("核心："), corePage));
    QComboBox* coreSel = new QComboBox(corePage);
    coreSelRow->addWidget(coreSel);
    QPushButton* redetectBtn = new QPushButton(qFromUtf8("重新检测系统核心"), corePage);
    coreSelRow->addWidget(redetectBtn);
    coreSelRow->addStretch(1);
    coreLayout->addLayout(coreSelRow);

    /* D：SOCKS 监听地址 / 端口 */
    QHBoxLayout* listenRow = new QHBoxLayout(0, 4, 4);
    listenRow->addWidget(new QLabel(qFromUtf8("监听地址："), corePage));
    QComboBox* listenAddr = new QComboBox(corePage);
    listenAddr->setEditable(true);
    V2ComboAdd(listenAddr, "127.0.0.1");   /* 仅本机（默认） */
    V2ComboAdd(listenAddr, "0.0.0.0");     /* 局域网可访问 */
    V2ComboAdd(listenAddr, "::1");         /* IPv6 本机 */
    listenAddr->setFixedWidth(140);
    listenRow->addWidget(listenAddr);
    listenRow->addWidget(new QLabel(qFromUtf8("SOCKS 端口："), corePage));
    QLineEdit* listenSocks = new QLineEdit("10808", corePage);
    listenSocks->setFixedWidth(70);
    listenRow->addWidget(listenSocks);
    listenRow->addWidget(new QLabel(qFromUtf8("HTTP 端口："), corePage));
    QLineEdit* listenHttp = new QLineEdit("10809", corePage);
    listenHttp->setFixedWidth(70);
    listenRow->addWidget(listenHttp);
    QPushButton* testPortsBtn = new QPushButton(qFromUtf8("测试端口"), corePage);
    QPushButton* testGstaticBtn = new QPushButton(qFromUtf8("测试 gstatic"), corePage);
    listenRow->addWidget(testPortsBtn);
    listenRow->addWidget(testGstaticBtn);
    listenRow->addStretch(1);
    coreLayout->addLayout(listenRow);
    coreLayout->addWidget(new QLabel(qFromUtf8("默认 SOCKS 10808 / HTTP 10809（仅本机）；0.0.0.0 会向局域网暴露"), corePage));
    /* 测试按钮结果：图标 + 文字（绿勾/红叉/灰线），独立一行 */
    QHBoxLayout* testRow = new QHBoxLayout(0, 4, 4);
    QLabel* testPrefix = new QLabel(qFromUtf8("测试结果："), corePage);
    QLabel* testSocksIcon = new QLabel(corePage);
    QLabel* testSocksText = new QLabel(corePage);
    QLabel* testHttpIcon = new QLabel(corePage);
    QLabel* testHttpText = new QLabel(corePage);
    QLabel* testMsg = new QLabel(corePage);
    QLabel* testTime = new QLabel(corePage);   /* 行尾唯一时间 */
    testRow->addWidget(testPrefix);
    testRow->addWidget(testSocksIcon);
    testRow->addWidget(testSocksText);
    testRow->addSpacing(12);
    testRow->addWidget(testHttpIcon);
    testRow->addWidget(testHttpText);
    testRow->addWidget(testMsg);
    testRow->addWidget(testTime);
    testRow->addStretch(1);
    coreLayout->addLayout(testRow);

    /* 按钮 Tooltip：说明访问的 URL / 行为 */
    v2Tip(testPortsBtn, qFromUtf8("用 curl 连接 http://<监听地址>:<SOCKS端口>/ 与 http://<监听地址>:<HTTP端口>/，检测端口是否在监听（不访问外网）"));
    v2Tip(testGstaticBtn, qFromUtf8("经 SOCKS5 / HTTP 代理访问 https://www.gstatic.com/generate_204（超时 5s），HTTP 204 表示代理可用"));
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
    logic->setListenWidgets(listenAddr, listenSocks, listenHttp);
    logic->setUpdateWidgets(updBtn, updAllBtn, nodeStatus);
    logic->setAutoUpdateWidgets(autoChk, autoFreq);
    QObject::connect(autoChk, SIGNAL(toggled(bool)), logic, SLOT(onAutoUpdateSettingsChanged()));
    QObject::connect(autoFreq, SIGNAL(activated(int)), logic, SLOT(onAutoUpdateSettingsChanged()));
    logic->setCoreWidgets(dlBtn);
    logic->setCoreTestWidgets(testSocksIcon, testSocksText,
                              testHttpIcon, testHttpText, testMsg);
    logic->setCoreTestTimeWidget(testTime);
    logic->setActiveNodeWidget(activeNodeLabel);
    logic->setTrafficWidget(trafficLabel);
    logic->setNodeViewWidgets(nodeFilterEdit, nodeSortBox);
    logic->setFavWidgets(favList);
    QObject::connect(nodeFilterEdit, SIGNAL(textChanged(const QString&)),
                     logic, SLOT(onNodeFilterChanged()));
    QObject::connect(nodeSortBox, SIGNAL(activated(int)),
                     logic, SLOT(onNodeSortChanged()));
    logic->setCoreSelector(coreSel, redetectBtn);
    logic->setTestWidgets(tcpSelBtn, realSelBtn, tcpAllBtn, realAllBtn, stopTestBtn);
    /* B：控件变化即时落盘（另外 aboutToQuit 再兜底一次） */
    QObject::connect(bypassCn, SIGNAL(toggled(bool)), logic, SLOT(onSaveSettings()));
    QObject::connect(bypassLan, SIGNAL(toggled(bool)), logic, SLOT(onSaveSettings()));
    QObject::connect(listenSocks, SIGNAL(returnPressed()), logic, SLOT(onSaveSettings()));
    QObject::connect(listenHttp, SIGNAL(returnPressed()), logic, SLOT(onSaveSettings()));
    QObject::connect(testPortsBtn, SIGNAL(clicked()), logic, SLOT(onTestPorts()));
    QObject::connect(testGstaticBtn, SIGNAL(clicked()), logic, SLOT(onTestGstatic()));
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
    QObject::connect(tcpSelBtn, SIGNAL(clicked()), logic, SLOT(onTestTcpSelected()));
    QObject::connect(realSelBtn, SIGNAL(clicked()), logic, SLOT(onTestRealSelected()));
    QObject::connect(tcpAllBtn, SIGNAL(clicked()), logic, SLOT(onTestTcpAll()));
    QObject::connect(realAllBtn, SIGNAL(clicked()), logic, SLOT(onTestRealAll()));
    QObject::connect(stopTestBtn, SIGNAL(clicked()), logic, SLOT(onStopTest()));
    QObject::connect(dlBtn, SIGNAL(clicked()), logic, SLOT(onDownloadCore()));
    QObject::connect(startBtn, SIGNAL(clicked()), logic, SLOT(onStartCore()));
    QObject::connect(stopBtn, SIGNAL(clicked()), logic, SLOT(onStopCore()));
    QObject::connect(coreSel, SIGNAL(activated(int)), logic, SLOT(onCoreKindChanged(int)));
    QObject::connect(redetectBtn, SIGNAL(clicked()), logic, SLOT(onRedetectCore()));

    /* ── 订阅表右键菜单 ── */
#ifdef QT3_BUILD
    QPopupMenu* subMenu = new QPopupMenu(w);
    subMenu->insertItem(qFromUtf8("更新选中订阅"), logic, SLOT(onUpdateSubscription()));
    subMenu->insertItem(qFromUtf8("更新全部订阅"), logic, SLOT(onUpdateAllSubscriptions()));
    subMenu->insertItem(qFromUtf8("复制订阅链接"), logic, SLOT(onCopySubLink()));
    subMenu->insertItem(qFromUtf8("删除订阅"), logic, SLOT(onDelSubscription()));
#else
    QMenu* subMenu = new QMenu(w);
    subMenu->addAction(qFromUtf8("更新选中订阅"), logic, SLOT(onUpdateSubscription()));
    subMenu->addAction(qFromUtf8("更新全部订阅"), logic, SLOT(onUpdateAllSubscriptions()));
    subMenu->addAction(qFromUtf8("复制订阅链接"), logic, SLOT(onCopySubLink()));
    subMenu->addAction(qFromUtf8("删除订阅"), logic, SLOT(onDelSubscription()));
#endif
    LambdaSlot* subCtx = new LambdaSlot(0, [=] {
        V2PrepareRow(subList);
        subMenu->popup(QCursor::pos());
    });
#ifdef QT3_BUILD
    QObject::connect(subList, SIGNAL(contextMenuRequested(int,int,const QPoint&)),
                     subCtx, SLOT(call()));
#else
    subList->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(subList, SIGNAL(customContextMenuRequested(const QPoint&)),
                     subCtx, SLOT(call()));
#endif

    /* ── 节点表右键菜单 ── */
#ifdef QT3_BUILD
    QPopupMenu* nodeMenu = new QPopupMenu(w);
    nodeMenu->insertItem(qFromUtf8("用此节点启动核心"), logic, SLOT(onStartCore()));
    nodeMenu->insertItem(qFromUtf8("复制节点链接"), logic, SLOT(onCopyNodeLink()));
    nodeMenu->insertItem(qFromUtf8("加入/取消收藏"), logic, SLOT(onToggleFavorite()));
    nodeMenu->insertSeparator();
    nodeMenu->insertItem(qFromUtf8("查询国家（选中）"), logic, SLOT(onLookupCountrySelected()));
    nodeMenu->insertItem(qFromUtf8("查询国家（全部未确定）"), logic, SLOT(onLookupCountryAll()));
    nodeMenu->insertItem(qFromUtf8("清空国家缓存"), logic, SLOT(onClearGeoCache()));
    nodeMenu->insertSeparator();
    nodeMenu->insertItem(qFromUtf8("测试 TCP（选中）"), logic, SLOT(onTestTcpSelected()));
    nodeMenu->insertItem(qFromUtf8("测试真连接（选中）"), logic, SLOT(onTestRealSelected()));
    nodeMenu->insertItem(qFromUtf8("批量 TCP（全部）"), logic, SLOT(onTestTcpAll()));
    nodeMenu->insertItem(qFromUtf8("批量真连接（全部）"), logic, SLOT(onTestRealAll()));
    nodeMenu->insertItem(qFromUtf8("停止测试"), logic, SLOT(onStopTest()));
    nodeMenu->insertSeparator();
    nodeMenu->insertItem(qFromUtf8("按延迟排序"), logic, SLOT(onSortByLatency()));
    nodeMenu->insertItem(qFromUtf8("清理超时节点"), logic, SLOT(onCleanFailedNodes()));
#else
    QMenu* nodeMenu = new QMenu(w);
    nodeMenu->addAction(qFromUtf8("用此节点启动核心"), logic, SLOT(onStartCore()));
    nodeMenu->addAction(qFromUtf8("复制节点链接"), logic, SLOT(onCopyNodeLink()));
    nodeMenu->addAction(qFromUtf8("加入/取消收藏"), logic, SLOT(onToggleFavorite()));
    nodeMenu->addSeparator();
    nodeMenu->addAction(qFromUtf8("查询国家（选中）"), logic, SLOT(onLookupCountrySelected()));
    nodeMenu->addAction(qFromUtf8("查询国家（全部未确定）"), logic, SLOT(onLookupCountryAll()));
    nodeMenu->addAction(qFromUtf8("清空国家缓存"), logic, SLOT(onClearGeoCache()));
    nodeMenu->addSeparator();
    nodeMenu->addAction(qFromUtf8("测试 TCP（选中）"), logic, SLOT(onTestTcpSelected()));
    nodeMenu->addAction(qFromUtf8("测试真连接（选中）"), logic, SLOT(onTestRealSelected()));
    nodeMenu->addAction(qFromUtf8("批量 TCP（全部）"), logic, SLOT(onTestTcpAll()));
    nodeMenu->addAction(qFromUtf8("批量真连接（全部）"), logic, SLOT(onTestRealAll()));
    nodeMenu->addAction(qFromUtf8("停止测试"), logic, SLOT(onStopTest()));
    nodeMenu->addSeparator();
    nodeMenu->addAction(qFromUtf8("按延迟排序"), logic, SLOT(onSortByLatency()));
    nodeMenu->addAction(qFromUtf8("清理超时节点"), logic, SLOT(onCleanFailedNodes()));
#endif
    LambdaSlot* nodeCtx = new LambdaSlot(0, [=] {
        V2PrepareRow(nodeList);
        nodeMenu->popup(QCursor::pos());
    });
#ifdef QT3_BUILD
    QObject::connect(nodeList, SIGNAL(contextMenuRequested(int,int,const QPoint&)),
                     nodeCtx, SLOT(call()));
#else
    nodeList->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(nodeList, SIGNAL(customContextMenuRequested(const QPoint&)),
                     nodeCtx, SLOT(call()));
#endif

#ifdef QT3_BUILD
    /* 国旗列 tooltip（Qt3；Qt4 走 QTableWidgetItem::setToolTip） */
    (new V2FlagTip(nodeList, logic, false));
    (new V2FlagTip(favList, logic, true));
#endif

    /* ── 收藏表右键菜单（logic 已创建，可挂槽） ── */
#ifdef QT3_BUILD
    QPopupMenu* favMenu = new QPopupMenu(w);
    favMenu->insertItem(qFromUtf8("用此节点启动核心"), logic, SLOT(onStartFavCore()));
    favMenu->insertItem(qFromUtf8("复制节点链接"), logic, SLOT(onCopyFavLink()));
    favMenu->insertItem(qFromUtf8("取消收藏"), logic, SLOT(onUnfavFav()));
#else
    QMenu* favMenu = new QMenu(w);
    favMenu->addAction(qFromUtf8("用此节点启动核心"), logic, SLOT(onStartFavCore()));
    favMenu->addAction(qFromUtf8("复制节点链接"), logic, SLOT(onCopyFavLink()));
    favMenu->addAction(qFromUtf8("取消收藏"), logic, SLOT(onUnfavFav()));
#endif
    LambdaSlot* favCtx = new LambdaSlot(0, [=] {
        V2PrepareRow(favList);
        favMenu->popup(QCursor::pos());
    });
#ifdef QT3_BUILD
    QObject::connect(favList, SIGNAL(contextMenuRequested(int,int,const QPoint&)),
                     favCtx, SLOT(call()));
#else
    favList->setContextMenuPolicy(Qt::CustomContextMenu);
    QObject::connect(favList, SIGNAL(customContextMenuRequested(const QPoint&)),
                     favCtx, SLOT(call()));
#endif

    /* 主窗销毁日志：exe 与 qltox 插件双形态共用（destroyed→LambdaSlot） */
    LambdaSlot* logSlot = new LambdaSlot(0, [] {
        qWarning("[mainwin] destroyed: V2raySuberMain");
    });
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(call()));
    QObject::connect(w, SIGNAL(destroyed()), logSlot, SLOT(deleteLater()));

    /* 其它按钮 Tooltip（说明访问的 URL / 行为） */
    v2Tip(startBtn, qFromUtf8("用当前内核 + 选中节点，在本机同时启动 SOCKS 与 HTTP 代理"));
    v2Tip(stopBtn, qFromUtf8("向当前核心进程发送终止信号"));
    v2Tip(dlBtn, qFromUtf8("从 gh-proxy.com 下载核心（失败自动回退 GitHub）"));
    v2Tip(redetectBtn, qFromUtf8("重新探测系统中的 v2ray / xray"));
    v2Tip(tcpSelBtn, qFromUtf8("curl telnet://<节点地址>:<端口>，测 TCP 建连耗时"));
    v2Tip(realSelBtn, qFromUtf8("经该节点访问 https://www.gstatic.com/generate_204，测真连接耗时"));
    v2Tip(tcpAllBtn, qFromUtf8("对所有节点批量执行 TCP 建连测试"));
    v2Tip(realAllBtn, qFromUtf8("对所有节点批量访问 https://www.gstatic.com/generate_204 测真连接"));
    v2Tip(stopTestBtn, qFromUtf8("停止当前测速队列"));
    v2Tip(autoChk, qFromUtf8("开启后按设定频率自动执行“更新全部订阅”"));
    v2Tip(autoFreq, qFromUtf8("自动更新间隔（默认 2 小时）"));
    v2Tip(nodeFilterEdit, qFromUtf8("按名称/国家/地址/协议实时过滤节点列表"));
    v2Tip(nodeSortBox, qFromUtf8("按延时/名称排序（仅调整显示顺序，不改动数据）"));
    v2Tip(updBtn, qFromUtf8("用 curl 拉取选中订阅地址并解析节点"));
    v2Tip(updAllBtn, qFromUtf8("用 curl 拉取全部订阅地址并解析节点"));

    /* 节点选择/当前行变化 → 更新“当前节点”显示 */
#ifdef QT3_BUILD
    QObject::connect(nodeList, SIGNAL(selectionChanged()), logic, SLOT(onNodeSelectionChanged()));
    QObject::connect(nodeList, SIGNAL(currentChanged(int,int)), logic, SLOT(onNodeSelectionChanged()));
#else
    QObject::connect(nodeList, SIGNAL(itemSelectionChanged()), logic, SLOT(onNodeSelectionChanged()));
    QObject::connect(nodeList, SIGNAL(currentCellChanged(int,int,int,int)), logic, SLOT(onNodeSelectionChanged()));
#endif

    /* 启动默认页：核心 */
#ifdef QT3_BUILD
    tabs->setCurrentPage(tabs->indexOf(corePage));
#else
    tabs->setCurrentIndex(tabs->indexOf(corePage));
#endif

    /* 重启恢复：上次运行中的核心自动拉起 */
    logic->restoreCoreOnStartup();

    return w;
}
