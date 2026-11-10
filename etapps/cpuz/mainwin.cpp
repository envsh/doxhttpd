#include "compat34.h"
#include "logic.h"
#include <qtabwidget.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qgroupbox.h>
#include <qprogressbar.h>
#include <qpushbutton.h>

#include <qfontmetrics.h>

/* ═══════════ 本地小工具 ═══════════ */

/* 子布局：不能挂 page（每容器仅一个顶层布局），parent-less + 由 into->addLayout 收纳 */
static QBoxLayout* CpzSub(QBoxLayout::Direction d) {
    QBoxLayout* l = new QBoxLayout(d);
    qSetLayoutMargin(l, 0);
    l->setSpacing(6);
    return l;
}

static QBoxLayout* CpzRow(QBoxLayout* into, QWidget* page,
                          const char* title, int titleW) {
    QBoxLayout* row = CpzSub(QBoxLayout::LeftToRight);
    row->setSpacing(8);
    QLabel* ln = new QLabel(qFromUtf8(title), page);
    ln->setMinimumWidth(titleW);
    row->addWidget(ln, 0);
    into->addLayout(row);
    return row;
}

static QLabel* CpzValue(QBoxLayout* row, QWidget* page) {
    QLabel* lv = new QLabel(qFromUtf8("—"), page);
    row->addWidget(lv, 1);
    return lv;
}

static void CpzTableInit(CpuTable*& t, QWidget* parent, int cols,
                         const char* const* headers) {
#ifdef QT3_BUILD
    t = new QTable(0, cols, parent);
    for (int i = 0; i < cols; i++) {
        t->horizontalHeader()->setLabel(i, qFromUtf8(headers[i]));
        t->setColumnReadOnly(i, true);
    }
#else
    t = new QTableWidget(0, cols, parent);
    QStringList h;
    for (int i = 0; i < cols; i++) {
        h << qFromUtf8(headers[i]);
    }
    t->setHorizontalHeaderLabels(h);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
#endif
    t->setMinimumHeight(120);
}

static QLabel* CpzBig(QGroupBox* page, const char* text, int pt, bool bold) {
    QLabel* l = new QLabel(qFromUtf8(text), page);
    QFont f = l->font();
    f.setPointSize(pt);
    if (bold) { f.setBold(true); }
    l->setFont(f);
    return l;
}

static void CpzWordWrap(QLabel* l) {
#ifdef QT3_BUILD
    /* Qt3 QLabel 无 wrap，Step3 数据侧按需换行填充 */
#else
    l->setWordWrap(true);
#endif
}

/* ═══════════ CPU 页 ═══════════ */

static QWidget* buildCpuPage(QWidget* tabs, CpuUi* ui) {
    QWidget* page = new QWidget(tabs);
#ifdef QT3_BUILD
    page->setName("cpuPage");
#endif
    QBoxLayout* v = qNewBoxLayout(page, QBoxLayout::TopToBottom, 8);

    /* Name 大字 */
    QLabel* name = new QLabel(qFromUtf8("—"), page);
    QFont nf = name->font();
    nf.setPointSize(16);
    nf.setBold(true);
    name->setFont(nf);
    v->addWidget(name);
    ui->lblName = name;

    QLabel* spec = new QLabel(qFromUtf8("—"), page);
    v->addWidget(spec);
    ui->lblSpec = spec;

    QLabel* ident = new QLabel(qFromUtf8("—"), page);
    v->addWidget(ident);
    ui->lblIdent = ident;

    /* Clocks + 每核表 */
    QBoxLayout* mid = CpzSub(QBoxLayout::LeftToRight);
    mid->setSpacing(6);
    v->addLayout(mid, 1);

    QGroupBox* gb = new QGroupBox(qFromUtf8("Clocks（Core #0 实时）"), page);
    QBoxLayout* cv = qNewBoxLayout(gb, QBoxLayout::TopToBottom, 6);
    QLabel* clk = CpzBig(gb, "— MHz", 15, true);
    cv->addWidget(clk);
    ui->lblClocks = clk;
    QProgressBar* bar = new QProgressBar(gb);
    cv->addWidget(bar);
    ui->barClock = bar;
    QLabel* mult = new QLabel(qFromUtf8("×— @ —"), gb);
    cv->addWidget(mult);
    ui->lblMult = mult;
    QLabel* temp = new QLabel(qFromUtf8("温度：—"), gb);
    QFont tf2 = temp->font();
    tf2.setPointSize(9);
    temp->setFont(tf2);
    cv->addWidget(temp);
    ui->lblTemp = temp;
    cv->addStretch(1);
    mid->addWidget(gb, 2);

    static const char* kCoreHeaders[4] = {
        "core", "当前 MHz", "Min MHz", "Max MHz"
    };
    CpuTable* ct = 0;
    CpzTableInit(ct, page, 4, kCoreHeaders);
    ct->setMinimumHeight(150);
    mid->addWidget(ct, 5);
    ui->coresTable = ct;

    /* Instructions */
    QLabel* instr = new QLabel(qFromUtf8("—"), page);
    QFont inf = instr->font();
    inf.setPointSize(9);
    instr->setFont(inf);
    CpzWordWrap(instr);
    instr->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    instr->setMinimumHeight(44);
    v->addWidget(instr);
    ui->lblInstr = instr;

    /* 指令集小字注记（占位，Step3 填真实绑定说明） */
    QLabel* ifoot = new QLabel(qFromUtf8("Instructions（逐核 flags 映射）"), page);
    QFont ftf = ifoot->font();
    ftf.setPointSize(7);
    ifoot->setFont(ftf);
    v->addWidget(ifoot);

    return page;
}

/* ═══════════ Caches 页 ═══════════ */

static QWidget* buildCachesPage(QWidget* tabs, CpuUi* ui) {
    QWidget* page = new QWidget(tabs);
#ifdef QT3_BUILD
    page->setName("cachePage");
#endif
    QBoxLayout* v = qNewBoxLayout(page, QBoxLayout::TopToBottom, 6);

    /* 示例行：L1 Data —— Step3 起按 sysfs/cache indexK 聚合填充 */
    QBoxLayout* r1 = CpzRow(v, page, "L1 Data", 110);
    ui->lblL1D = CpzValue(r1, page);
    QBoxLayout* r2 = CpzRow(v, page, "L1 Inst.", 110);
    ui->lblL1I = CpzValue(r2, page);
    QBoxLayout* r3 = CpzRow(v, page, "L2", 110);
    ui->lblL2 = CpzValue(r3, page);
    QBoxLayout* r4 = CpzRow(v, page, "L3", 110);
    ui->lblL3 = CpzValue(r4, page);

    v->addStretch(1);
    return page;
}

/* ═══════════ Mainboard 页 ═══════════ */

static QWidget* buildMainboardPage(QWidget* tabs, CpuUi* ui) {
    QWidget* page = new QWidget(tabs);
#ifdef QT3_BUILD
    page->setName("boardPage");
#endif
    QBoxLayout* v = qNewBoxLayout(page, QBoxLayout::TopToBottom, 6);

    QBoxLayout* r0 = CpzRow(v, page, "Manufacturer", 150);
    ui->lblBoardMfr = CpzValue(r0, page);
    QBoxLayout* r1 = CpzRow(v, page, "Model", 150);
    ui->lblBoardModel = CpzValue(r1, page);
    QBoxLayout* r2 = CpzRow(v, page, "板卡版本", 150);
    ui->lblBoardVer = CpzValue(r2, page);
    QBoxLayout* r3 = CpzRow(v, page, "Chipset", 150);
    ui->lblChipset = CpzValue(r3, page);
    QBoxLayout* r4 = CpzRow(v, page, "BIOS Brand", 150);
    ui->lblBiosBrand = CpzValue(r4, page);
    QBoxLayout* r5 = CpzRow(v, page, "BIOS Version/Date", 150);
    ui->lblBiosVer = CpzValue(r5, page);
    QBoxLayout* r6 = CpzRow(v, page, "Chassis", 150);
    ui->lblChassis = CpzValue(r6, page);

    v->addStretch(1);
    return page;
}

/* ═══════════ Graphics 页 ═══════════ */

static QWidget* buildGraphicsPage(QWidget* tabs, CpuUi* ui) {
    QWidget* page = new QWidget(tabs);
#ifdef QT3_BUILD
    page->setName("gpuPage");
#endif
    QBoxLayout* v = qNewBoxLayout(page, QBoxLayout::TopToBottom, 6);

    static const char* kGpuHeaders[5] = {
        "Bus 位置", "类型", "型号", "驱动", "内存"
    };
    CpuTable* gt = 0;
    CpzTableInit(gt, page, 5, kGpuHeaders);
    gt->setMinimumHeight(140);
    v->addWidget(gt, 1);
    ui->gpusTable = gt;
    return page;
}

/* ═══════════ Memory 页 ═══════════ */

static QWidget* buildMemoryPage(QWidget* tabs, CpuUi* ui) {
    QWidget* page = new QWidget(tabs);
#ifdef QT3_BUILD
    page->setName("memPage");
#endif
    QBoxLayout* v = qNewBoxLayout(page, QBoxLayout::TopToBottom, 6);

    QLabel* mi = new QLabel(qFromUtf8("—"), page);
    QFont mf = mi->font();
    mf.setPointSize(10);
    mi->setFont(mf);
    CpzWordWrap(mi);
    v->addWidget(mi);
    ui->lblMemInfo = mi;

    QLabel* note = new QLabel(
        qFromUtf8("频率/时序/SPD 需 root 权限 dmidecode，不可用时显示 —"),
        page);
    QFont nf = note->font();
    nf.setPointSize(8);
    nf.setItalic(true);
    note->setFont(nf);
    CpzWordWrap(note);
    v->addWidget(note);
    ui->lblMemNote = note;

    v->addStretch(1);
    return page;
}

/* ═══════════ Bench 页 ═══════════ */

static QWidget* buildBenchPage(QWidget* tabs, CpuUi* ui) {
    QWidget* page = new QWidget(tabs);
#ifdef QT3_BUILD
    page->setName("benchPage");
#endif
    QBoxLayout* v = qNewBoxLayout(page, QBoxLayout::TopToBottom, 8);

    QPushButton* btn = new QPushButton(qFromUtf8("开始基准"), page);
    btn->setMinimumHeight(32);
    v->addWidget(btn, 0);
    ui->btnBench = btn;

    QPushButton* btnR = new QPushButton(qFromUtf8("导出报告"), page);
    btnR->setMinimumHeight(32);
    v->addWidget(btnR, 0);
    ui->btnReport = btnR;

    QProgressBar* bar = new QProgressBar(page);
    v->addWidget(bar);
    ui->barBench = bar;

    QLabel* score = new QLabel(qFromUtf8("—"), page);
    QFont sf = score->font();
    sf.setPointSize(11);
    score->setFont(sf);
    score->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    CpzWordWrap(score);
    v->addWidget(score, 1);
    ui->lblBenchScore = score;

    QLabel* st = new QLabel(
        qFromUtf8("状态：空闲"), page);
    QFont tf = st->font();
    tf.setPointSize(9);
    st->setFont(tf);
    v->addWidget(st);
    ui->lblBenchState = st;

    QLabel* note = new QLabel(
        qFromUtf8("自研口径，与 CPU-Z Bench 分不可比"), page);
    QFont bf = note->font();
    bf.setPointSize(8);
    bf.setItalic(true);
    note->setFont(bf);
    CpzWordWrap(note);
    v->addWidget(note);
    ui->lblBenchNote = note;

    return page;
}

/* ═══════════ 顶层 ═══════════ */

QWidget* createCpuMainWindow(void* parent) {
    QWidget* w;
    QWidget* pw = (QWidget*)parent;
#ifdef QT3_BUILD
    if (pw) {
        w = new QWidget(pw, "cpuzMain");
    } else {
        w = new QWidget(0, "cpuzMain");
    }
#else
    if (pw) {
        w = new QWidget(pw);
    } else {
        w = new QWidget(0);
    }
#endif
    QBoxLayout* outer = qNewBoxLayout(w, QBoxLayout::TopToBottom, 6);

    QTabWidget* tabs = new QTabWidget(w);
    static const char* kSubTitles[6] = {
        "CPU", "Caches", "Mainboard", "Graphics", "Memory", "Bench"
    };
    CpuUi* ui = new CpuUi();
    ui->cpuzTabs = tabs;

    QWidget* pages[6];
    pages[0] = buildCpuPage(tabs, ui);
    pages[1] = buildCachesPage(tabs, ui);
    pages[2] = buildMainboardPage(tabs, ui);
    pages[3] = buildGraphicsPage(tabs, ui);
    pages[4] = buildMemoryPage(tabs, ui);
    pages[5] = buildBenchPage(tabs, ui);
    for (int i = 0; i < 6; i++) {
        tabs->addTab(pages[i], qFromUtf8(kSubTitles[i]));
    }
    outer->addWidget(tabs, 1);

    CpuLogic* logic = new CpuLogic(w);
    logic->setWidgets(ui);

    return w;
}
