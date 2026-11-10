#include "compat34.h"
#include "logic.h"
#include <qtabwidget.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qgroupbox.h>
#include <qprogressbar.h>
#include <qfontmetrics.h>

/* ═══════════ 本地小工具（cpuz CpzSub/CpzRow/CpzValue 同款，改前缀 Istm） ═══════════ */

/* 子布局：parent-less（每容器仅一个顶层布局），由 into->addLayout 收纳 */
static QBoxLayout* IstmSub(QBoxLayout::Direction d) {
    QBoxLayout* l = new QBoxLayout(d);
    qSetLayoutMargin(l, 0);
    l->setSpacing(6);
    return l;
}

static QLabel* IstmNote(QWidget* parent, const char* text) {
    QLabel* l = new QLabel(qFromUtf8(text), parent);
    QFont f = l->font();
    f.setPointSize(9);
    l->setFont(f);
    return l;
}

static QLabel* IstmBig(QWidget* page, const char* text, int pt, bool bold) {
    QLabel* l = new QLabel(qFromUtf8(text), page);
    QFont f = l->font();
    f.setPointSize(pt);
    if (bold) { f.setBold(true); }
    l->setFont(f);
    return l;
}

static void IstmTableInit(IstmTable*& t, QWidget* parent, int cols,
                          const char* const* headers, int rows) {
#ifdef QT3_BUILD
    t = new QTable(rows, cols, parent);
    for (int i = 0; i < cols; i++) {
        t->horizontalHeader()->setLabel(i, qFromUtf8(headers[i]));
        t->setColumnReadOnly(i, true);
    }
#else
    t = new QTableWidget(rows, cols, parent);
    QStringList h;
    for (int i = 0; i < cols; i++) {
        h << qFromUtf8(headers[i]);
    }
    t->setHorizontalHeaderLabels(h);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
#endif
    t->setMinimumHeight(rows >= 5 ? 120 : 96);
}

/* ═══════════ 顶条 menu-bar 快照 ═══════════ */

static QWidget* buildTopBar(QWidget* parent, IstatUi* ui) {
    QWidget* bar = new QWidget(parent);
    QBoxLayout* h = qNewBoxLayout(bar, QBoxLayout::LeftToRight, 6);

    QLabel* c = new QLabel(qFromUtf8("CPU —"), bar);
    h->addWidget(c, 0);
    ui->topCpu = c;

    QLabel* m = new QLabel(qFromUtf8("MEM —"), bar);
    h->addWidget(m, 0);
    ui->topMem = m;

    QLabel* n = new QLabel(qFromUtf8("NET —"), bar);
    h->addWidget(n, 0);
    ui->topNet = n;

    QLabel* t = new QLabel(qFromUtf8("—"), bar);
    h->addWidget(t, 0);
    ui->topTemp = t;

    QLabel* b = new QLabel(qFromUtf8("🔋 —"), bar);
    h->addWidget(b, 0);
    ui->topBat = b;

    QLabel* ck = new QLabel(qFromUtf8("--:--"), bar);
    h->addWidget(ck, 0);
    ui->topClock = ck;

    h->addStretch(1);
    return bar;
}

/* ═══════════ CPU 页（/proc/stat 差分 + 每核 MHz/温度 + Top 进程） ═══════════ */

static QWidget* buildCpuPage(QWidget* tabs, IstatUi* ui) {
    QWidget* page = new QWidget(tabs);
    QBoxLayout* v = qNewBoxLayout(page, QBoxLayout::TopToBottom, 8);

    QLabel* big = IstmBig(page, "—", 16, true);
    v->addWidget(big, 0);
    ui->lblCpuBig = big;

    IstatGraph* g = new IstatGraph(page);
    v->addWidget(g, 0);
    ui->graphCpu = g;

    QLabel* load = new QLabel(qFromUtf8("load — · uptime —"), page);
    v->addWidget(load, 0);
    ui->lblLoad = load;

    QLabel* gpu = new QLabel(qFromUtf8("GPU：—"), page);
    v->addWidget(gpu, 0);
    ui->lblGpu = gpu;

    static const char* const kCoreCols[] =
        { "Core", "MHz", "%", "°C" };
    IstmTable* cores = 0;
    IstmTableInit(cores, page, 4, kCoreCols, 4);
    v->addWidget(cores, 0);
    ui->tblCores = cores;

    v->addSpacing(4);
    QLabel* th = new QLabel(qFromUtf8("Top 进程（每 10s 刷新）"), page);
    QFont thf = th->font();
    thf.setBold(true);
    th->setFont(thf);
    v->addWidget(th, 0);
    static const char* const kTopCols[] = { "进程", "CPU %", "MEM %" };
    IstmTable* top = 0;
    IstmTableInit(top, page, 3, kTopCols, 5);
    v->addWidget(top, 0);
    ui->tblTopCpu = top;

    v->addStretch(1);
    return page;
}

/* ═══════════ Memory 页（meminfo + PSI + swap 分设备 + Top 进程） ═══════════ */

static QWidget* buildMemPage(QWidget* tabs, IstatUi* ui) {
    QWidget* page = new QWidget(tabs);
    QBoxLayout* v = qNewBoxLayout(page, QBoxLayout::TopToBottom, 8);

    QLabel* big = IstmBig(page, "—", 16, true);
    v->addWidget(big, 0);
    ui->lblMemBig = big;

    QProgressBar* bar = new QProgressBar(page);
#ifdef QT3_BUILD
    bar->setTotalSteps(100);
#endif
    v->addWidget(bar, 0);
    ui->barMem = bar;

    IstatGraph* g = new IstatGraph(page);
    v->addWidget(g, 0);
    ui->graphMem = g;

    QLabel* kv = new QLabel(qFromUtf8("—"), page);
    QLabel* swapL = new QLabel(qFromUtf8("swap —"), page);
    QLabel* psi = new QLabel(qFromUtf8("PSI —"), page);
    v->addWidget(kv, 0);
    v->addWidget(swapL, 0);
    v->addWidget(psi, 0);
    ui->lblMemKv = kv;
    ui->lblSwap = swapL;
    ui->lblPsi = psi;

    v->addSpacing(4);
    QLabel* th = new QLabel(qFromUtf8("Top 进程（每 10s 刷新）"), page);
    QFont thf = th->font();
    thf.setBold(true);
    th->setFont(thf);
    v->addWidget(th, 0);
    static const char* const kTopCols[] = { "进程", "CPU %", "MEM %" };
    IstmTable* top = 0;
    IstmTableInit(top, page, 3, kTopCols, 5);
    v->addWidget(top, 0);
    ui->tblTopMem = top;

    v->addStretch(1);
    return page;
}

/* ═══════════ Network 页（/proc/net/dev 差分 + 本机/公网 IP + ss 统计） ═══════════ */

static QWidget* buildNetPage(QWidget* tabs, IstatUi* ui) {
    QWidget* page = new QWidget(tabs);
    QBoxLayout* v = qNewBoxLayout(page, QBoxLayout::TopToBottom, 8);

    QLabel* big = IstmBig(page, "—", 16, true);
    v->addWidget(big, 0);
    ui->lblNetBig = big;

    QLabel* rx = new QLabel(qFromUtf8("下行 RX"), page);
    QFont rxf = rx->font();
    rxf.setPointSize(8);
    rx->setFont(rxf);
    v->addWidget(rx, 0);
    IstatGraph* gx = new IstatGraph(page);
    gx->setColor(0, 150, 199);                   /* teal */
    v->addWidget(gx, 0);
    ui->graphNetRx = gx;

    QLabel* tx = new QLabel(qFromUtf8("上行 TX"), page);
    QFont txf = tx->font();
    txf.setPointSize(8);
    tx->setFont(txf);
    v->addWidget(tx, 0);
    IstatGraph* gt = new IstatGraph(page);
    gt->setColor(245, 124, 0);                   /* orange */
    v->addWidget(gt, 0);
    ui->graphNetTx = gt;

    static const char* const kNetCols[] =
        { "Iface", "State·IP", "RX", "TX", "累计 RX", "累计 TX" };
    IstmTable* t = 0;
    IstmTableInit(t, page, 6, kNetCols, 1);      /* 行数运行时 IstmSetRows */
    v->addWidget(t, 0);
    ui->tblNet = t;

    QLabel* pub = new QLabel(qFromUtf8("公网 IP：—"), page);
    v->addWidget(pub, 0);
    ui->lblNetPub = pub;

    QLabel* note = IstmNote(page,
        "速率=1s 差分；累计为开机以来；公网 IP 每 2min best-effort（超时 5s，失败 —）");
    v->addWidget(note, 0);

    v->addStretch(1);
    return page;
}

/* ═══════════ Disk 页（df 分区 + sda 活动速率差分 + SMART root 注记） ═══════════ */

static QWidget* buildDiskPage(QWidget* tabs, IstatUi* ui) {
    QWidget* page = new QWidget(tabs);
    QBoxLayout* v = qNewBoxLayout(page, QBoxLayout::TopToBottom, 8);

    QLabel* big = IstmBig(page, "—", 16, true);
    v->addWidget(big, 0);
    ui->lblDiskBig = big;

    IstatGraph* g = new IstatGraph(page);
    g->setColor(255, 152, 0);                    /* orange */
    v->addWidget(g, 0);
    ui->graphDisk = g;

    QLabel* iops = new QLabel(qFromUtf8("IOPS — · 队列 —"), page);
    v->addWidget(iops, 0);
    ui->lblDiskIops = iops;

    static const char* const kDiskCols[] =
        { "Device", "Type", "Size", "Used %", "Mount" };
    IstmTable* t = 0;
    IstmTableInit(t, page, 5, kDiskCols, 1);     /* 行数运行时 IstmSetRows */
    v->addWidget(t, 0);
    ui->tblDisk = t;

    QLabel* note = IstmNote(page,
        "S.M.A.R.T. 需 root（smartctl）不可用；速率=1s 差分；每 10s 刷分区表");
    v->addWidget(note, 0);
    ui->lblDiskNote = note;

    v->addStretch(1);
    return page;
}

/* ═══════════ Sensors 页（hwmon：温度/电压/电流/风扇全表） ═══════════ */

static QWidget* buildSensorsPage(QWidget* tabs, IstatUi* ui) {
    QWidget* page = new QWidget(tabs);
    QBoxLayout* v = qNewBoxLayout(page, QBoxLayout::TopToBottom, 8);

    static const char* const kSensCols[] =
        { "Chip", "Label", "Value" };
    IstmTable* t = 0;
    IstmTableInit(t, page, 3, kSensCols, 1);     /* 行数运行时 IstmSetRows */
    t->setMinimumHeight(220);
    v->addWidget(t, 0);
    ui->tblSens = t;

    QLabel* note = new QLabel(qFromUtf8("—"), page);
    v->addWidget(note, 0);
    ui->lblSensNote = note;

    v->addStretch(1);
    return page;
}

/* ═══════════ Battery 页（BAT1 + ACAD + 健康率推导） ═══════════ */

static QWidget* buildBatPage(QWidget* tabs, IstatUi* ui) {
    QWidget* page = new QWidget(tabs);
    QBoxLayout* v = qNewBoxLayout(page, QBoxLayout::TopToBottom, 8);

    QLabel* big = IstmBig(page, "—", 16, true);
    v->addWidget(big, 0);
    ui->lblBatBig = big;

    IstatGraph* g = new IstatGraph(page);
    g->setColor(33, 150, 243);                   /* blue */
    v->addWidget(g, 0);
    ui->graphBat = g;

    QLabel* hp = new QLabel(qFromUtf8("健康 —"), page);
    v->addWidget(hp, 0);
    ui->lblBatHealth = hp;

    QLabel* kv = new QLabel(qFromUtf8("—"), page);
    v->addWidget(kv, 0);
    ui->lblBatKv = kv;

    QLabel* ac = new QLabel(qFromUtf8("AC —"), page);
    v->addWidget(ac, 0);
    ui->lblBatAc = ac;

    QLabel* note = IstmNote(page,
        "健康%=charge_full÷charge_full_design（<80% 建议更换）；每 10s 刷新");
    v->addWidget(note, 0);

    v->addStretch(1);
    return page;
}

/* ═══════════ 主窗口 ═══════════ */

QWidget* createIstatMainWindow(void* parent) {
    IstatUi* ui = new IstatUi;

    QWidget* root = new QWidget(0);
#ifdef QT3_BUILD
    root->setName("istatRoot");
#else
    Q_UNUSED(parent);
#endif
    QBoxLayout* v = qNewBoxLayout(root, QBoxLayout::TopToBottom, 8);
    qSetLayoutMargin(v, 8);

    /* 顶条 */
    QWidget* bar = buildTopBar(root, ui);
    v->addWidget(bar, 0);

    /* 7 页 Tabs */
    QTabWidget* tabs = new QTabWidget(root);
    v->addWidget(tabs, 1);

    tabs->addTab(buildCpuPage(tabs, ui), qFromUtf8("CPU"));
    tabs->addTab(buildMemPage(tabs, ui), qFromUtf8("Memory"));
    tabs->addTab(buildNetPage(tabs, ui), qFromUtf8("Network"));
    tabs->addTab(buildDiskPage(tabs, ui), qFromUtf8("Disk"));
    tabs->addTab(buildSensorsPage(tabs, ui), qFromUtf8("Sensors"));
    tabs->addTab(buildBatPage(tabs, ui), qFromUtf8("Battery"));
    /* About 页（Step6：版本说明 + 通知历史由 logic.fillAbout 刷新） */
    QWidget* aboutPage = new QWidget(tabs);
    QBoxLayout* av = qNewBoxLayout(aboutPage, QBoxLayout::TopToBottom, 8);
    QLabel* about = new QLabel(qFromUtf8("—"), aboutPage);
    av->addWidget(about);
    ui->lblAbout = about;

    QLabel* world = new QLabel(qFromUtf8("世界钟 —"), aboutPage);
    av->addWidget(world, 0);
    ui->lblWorld = world;

    av->addStretch(1);
    tabs->addTab(aboutPage, qFromUtf8("About"));

    /* Logic 接线 */
    IstatLogic* logic = new IstatLogic(root);
    logic->setWidgets(ui);

    return root;
}