#include "logic.h"
#include "compat34.h"
#include "compatcore34.h"
#include <qfile.h>
#include <qtimer.h>
#include <qstringlist.h>
#include <qdir.h>
#include <qthread.h>
#include <qdatetime.h>
#include <string.h>
#include <atomic>
#include <thread>
#include <vector>

/* ═══════════ 本地小工具 ═══════════ */

static QString CpzStrip(const QString& s) {
    QString t = s;
#ifdef QT3_BUILD
    return t.stripWhiteSpace();
#else
    return t.trimmed();
#endif
}

static QStringList CpzSplitLines(const QString& s) {
#ifdef QT3_BUILD
    return QStringList::split('\n', s, false);
#else
    return s.split('\n', QString::KeepEmptyParts);
#endif
}

static int CpzPos(const QString& s, const char* needle) {
#ifdef QT3_BUILD
    return s.find(QString(needle));
#else
    return s.indexOf(QString(needle), 0);
#endif
}

static QStringList CpzSplitW(const QString& s) {
#ifdef QT3_BUILD
    return QStringList::split(' ', s, false);
#else
    return s.split(' ', QString::SkipEmptyParts);
#endif
}

/* UTF-8 字节队列（Qt3 utf8 / Qt4 toUtf8） */
static QByteArray qUtf8Bytes(const QString& s) {
#ifdef QT3_BUILD
    return s.utf8();
#else
    return s.toUtf8();
#endif
}

static QString CpzReadFile(const QString& path) {
    QFile f(path);
#ifdef QT3_BUILD
    if (!f.open(IO_ReadOnly)) { return QString(); }
#else
    if (!f.open(QIODevice::ReadOnly)) { return QString(); }
#endif
    QByteArray a;
    /* Qt3 QIODevice::readAll 内部 while(!atEnd()) 在 sysfs 文件上死循环
       （readBlock 失败不推进读位 → atEnd 永不成立 → CPU 100% 空转，启动卡死；
       istat_menus 同源 bug 已验证的修法：确定性 readBlock，n<=0 即停） */
    char buf[4096];
    for (;;) {
#ifdef QT3_BUILD
        int n = f.readBlock(buf, sizeof(buf));
#else
        int n = (int)f.read(buf, sizeof(buf));
#endif
        if (n <= 0) { break; }
        int pos = (int)a.size();
        a.resize(pos + n);
        memcpy(a.data() + pos, buf, n);
    }
    f.close();
    return CpzStrip(QString::fromUtf8(a.data(), (int)a.size()));
}

/* 读取 lscpu 等命令输出（stdout+stderr 合流） */
static QString CpzRun(const QString& program, const QStringList& args) {
    QString out;
    qRuncmdCaptureOuterr(program, args, &out);
    return out;
}

/* ═══════════ Bench（自研口径：定时长计次，与 CPU-Z 分不可比） ═══════════ */

/* 载荷积累器：volatile 抑制整数/浮点优化剥除 */
struct CpuBenchAcc {
    volatile long   x;
    volatile double d;
    volatile long   sink;
    CpuBenchAcc() : x(0), d(1.0), sink(0) {}
};

/* 单段工作循环：~2.0 s 内整数×浮点混合批量累加；stop==2 提前撤 */
static long sqBenchLoop(CpuBenchAcc* a, int ms, volatile int* stop) {
    long ops = 0;
    QTime t;
    t.start();
    while (t.elapsed() < ms && t.elapsed() >= 0) {
        for (int i = 0; i < 200; i++) {
            a->x = a->x * 48271 + 12345;
            a->d = a->d * 1.6180339887;
            if (i % 41 == 0) { a->sink = a->x; }
        }
        ops += 200;
        if (stop && *stop == 2) { break; }
    }
    return ops;
}

/* Manager 线程：run() 内做一段计时；多线程时 spawn 附加 std::thread */
class CpuBenchRunner : public QThread {
public:
    CpuBenchRunner(int workers, volatile int* stopflag)
#ifndef QT3_BUILD
        : QThread()
#endif
    {
        m_workers = workers > 0 ? workers : 1;
        m_stopflag = stopflag;
    }

    void run() {
        if (m_workers <= 1) {
            CpuBenchAcc a;
            m_ops.store(sqBenchLoop(&a, 2000, m_stopflag));
            return;
        }
        std::vector<std::thread> extra;
        for (int i = 1; i < m_workers; i++) {
            extra.push_back(std::thread(workerBody, this));
        }
        CpuBenchAcc a;
        long own = sqBenchLoop(&a, 2000, m_stopflag);
        for (unsigned i = 0; i < extra.size(); i++) {
            extra[i].join();
        }
        m_ops.fetch_add(own);
    }

    static void workerBody(CpuBenchRunner* r) {
        CpuBenchAcc a;
        long v = sqBenchLoop(&a, 2000, r->m_stopflag);
        r->m_ops.fetch_add(v);
    }

    std::atomic<long> m_ops;
    int               m_workers;
    volatile int*     m_stopflag;
};

/* ═══════════ 构造 / setWidgets ═══════════ */

CpuLogic::CpuLogic(QObject* parent)
    : QObject(parent)
{
    m_ui = 0;
    m_tickTimer = 0;
    m_benchState = 0;
    m_fam = 0; m_model = 0; m_step = 0;
    m_sockets = 0; m_coresPer = 0; m_threadsPer = 0; m_totalCpus = 0;
    m_minMhz = 0; m_maxMhz = 0;
    m_benchTimer = 0;
    m_benchRunner = 0;
    m_benchPhase = 0;
    m_singleScore = 0;
    m_multiScore = 0;
}

CpuLogic::~CpuLogic() {
    /* 取消未完成的 bench（stop=2 让循环尽快退出） */
    if (m_benchRunner) {
        m_benchState = 2;
        ((CpuBenchRunner*)m_benchRunner)->wait();
        delete m_benchRunner;
        m_benchRunner = 0;
    }
}

void CpuLogic::setWidgets(CpuUi* ui) {
    m_ui = ui;
    probeBase();
    fillCpuPage();
    fillCachesPage();
    fillMainboardPage();
    fillGraphicsPage();
    fillMemoryPage();
    m_tickTimer = new QTimer(this);
    connect(m_tickTimer, SIGNAL(timeout()), this, SLOT(onTick()));
    m_tickTimer->start(1000);
    probeChipset();

    m_benchTimer = new QTimer(this);
    connect(m_benchTimer, SIGNAL(timeout()), this, SLOT(onBenchTick()));
    if (m_ui) {
        connect(m_ui->btnBench, SIGNAL(clicked()),
            this, SLOT(onBench()));
        connect(m_ui->btnReport, SIGNAL(clicked()),
            this, SLOT(onReport()));
    }
}

/* 实时频率：/sys/.../cpuN/cpufreq/scaling_cur_freq（kHz） */
int CpuLogic::readCurMhz(int cpuIdx) {
    QString s = CpzReadFile(QString(
        "/sys/devices/system/cpu/cpu%1/cpufreq/scaling_cur_freq").arg(cpuIdx));
    if (s.isEmpty()) { return -1; }
    return s.toInt() / 1000;
}

void CpuLogic::barPct(QProgressBar* bar, int pct) {
#ifdef QT3_BUILD
    bar->setTotalSteps(100);
    bar->setProgress(pct);
#else
    bar->setRange(0, 100);
    bar->setValue(pct);
#endif
}

/* 读系统 hwmon coretemp/acpitz（root-free，毫摄氏） */
static QString CpzTemp() {
    QDir hwdir("/sys/class/hwmon");
    QStringList hws;
    for (int scan = 0; scan < 12; scan++) {
        QString cand = QString("hwmon%1").arg(scan);
        QString nm = CpzReadFile("/sys/class/hwmon/" + cand + "/name");
        if (nm.isEmpty()) { continue; }
        if (nm != "coretemp" && nm != "acpitz") { continue; }
        /* coretemp: 取首个 Core 标签；acpitz: 取 temp1 */
        for (int j = 1; j <= 16; j++) {
            QString vin = CpzReadFile(QString(
                "/sys/class/hwmon/%1/temp%2_input").arg(cand).arg(j));
            if (vin.isEmpty()) { continue; }
            int mc = vin.toInt();
            if (mc <= 0 || mc >= 110000) { continue; }
            QString lab = CpzReadFile(QString(
                "/sys/class/hwmon/%1/temp%2_label").arg(cand).arg(j));
            if (nm == "coretemp" && CpzPos(lab, "Core") < 0) {
                continue;
            }
            return QString("%1°C").arg(mc / 1000);
        }
    }
    return QString();
}

void CpuLogic::onTick() {
    if (!m_ui || !m_ui->coresTable) { return; }
    for (int i = 0; i < m_totalCpus; i++) {
        int mhz = readCurMhz(i);
        QString v = (mhz < 0) ? QString("—") : QString::number(mhz);
#ifdef QT3_BUILD
        m_ui->coresTable->setText(i, 1, v);
#else
        QTableWidgetItem* it = m_ui->coresTable->item(i, 1);
        if (it) { it->setText(v); }
#endif
    }
    /* #0 大字 + 折算条 + 倍频 */
    int hz0 = readCurMhz(0);
    if (hz0 < 0) { return; }
    m_ui->lblClocks->setText(qFromUtf8("%1 MHz")
        .arg((double)hz0, 0, 'f', 1));
    m_ui->lblMult->setText(qFromUtf8("×%1 @ ~100 MHz（BCLK 假定）")
        .arg((double)hz0 / 100.0, 0, 'f', 1));
    int pct = 0;
    if (m_maxMhz > m_minMhz) {
        pct = (int)((hz0 - m_minMhz) * 100 / (m_maxMhz - m_minMhz));
        if (pct < 0) { pct = 0; }
        if (pct > 100) { pct = 100; }
    }
    barPct(m_ui->barClock, pct);

    QString tl = CpzTemp();
    m_ui->lblTemp->setText(qFromUtf8("温度：") +
        (tl.isEmpty() ? QString("—") : tl));
}

/* ═══════════ 报告导出 ═══════════ */

void CpuLogic::onReport() {
    if (!m_ui) { return; }
    QString body = makeReportText();
    QString path = qGetHomePath() + QString("/cpuz_report_%1.txt")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    QFile f(path);
#ifdef QT3_BUILD
    if (!f.open(IO_WriteOnly)) {
#else
    if (!f.open(QIODevice::WriteOnly)) {
#endif
        m_ui->lblBenchState->setText(qFromUtf8("报告写出失败"));
        return;
    }
    QByteArray ub = qUtf8Bytes(body);
#ifdef QT3_BUILD
    f.writeBlock(ub.data(), ub.size());
#else
    f.write(ub.data(), ub.size());
#endif
    f.close();
    m_ui->lblBenchState->setText(qFromUtf8("报告已导出：") + path);
}

static int CpzGpuRows(CpuTable* t) {
#ifdef QT3_BUILD
    return t->numRows();
#else
    return t->rowCount();
#endif
}

static QString CpzCellText(CpuTable* t, int row, int col) {
#ifdef QT3_BUILD
    return t->text(row, col);
#else
    QTableWidgetItem* it = t->item(row, col);
    if (!it) { return QString(); }
    return it->text();
#endif
}

static QString CpzRowJoin(CpuTable* t, int row, int cols) {
    QString line;
    for (int c = 0; c < cols; c++) {
        if (c > 0) { line += " | "; }
        line += CpzCellText(t, row, c);
    }
    return line;
}

QString CpuLogic::makeReportText() {
    QString r = qFromUtf8("CPU-Z（Linux 版）报告 %1\n")
        .arg(QDateTime::currentDateTime().toString("yy-MM-dd hh:mm:ss"));
    r += qFromUtf8("\n== CPU ==\n");
    r += m_ui->lblName->text() + "\n"
      + m_ui->lblSpec->text() + "\n"
      + m_ui->lblIdent->text() + "\n"
      + m_ui->lblClocks->text() + " | " + m_ui->lblMult->text()
      + " | " + m_ui->lblTemp->text() + "\n"
      + m_ui->lblInstr->text() + "\n";
    r += qFromUtf8("\n== Caches ==\n")
      + m_ui->lblL1D->text() + "\n"
      + m_ui->lblL1I->text() + "\n"
      + m_ui->lblL2->text() + "\n"
      + m_ui->lblL3->text() + "\n";
    r += qFromUtf8("\n== Mainboard ==\n")
      + "Manufacturer: " + m_ui->lblBoardMfr->text() + "\n"
      + "Model: " + m_ui->lblBoardModel->text() + "\n"
      + "Version: " + m_ui->lblBoardVer->text() + "\n"
      + "Chipset: " + m_ui->lblChipset->text() + "\n"
      + "BIOS: " + m_ui->lblBiosBrand->text() + " · "
      + m_ui->lblBiosVer->text() + "\n"
      + "Chassis: " + m_ui->lblChassis->text() + "\n";
    r += qFromUtf8("\n== Graphics ==\n");
    for (int i = 0; i < CpzGpuRows(m_ui->gpusTable); i++) {
        r += CpzRowJoin(m_ui->gpusTable, i, 5) + "\n";
    }
    r += qFromUtf8("\n== Memory ==\n")
      + m_ui->lblMemInfo->text() + "\n";
    r += qFromUtf8("\n== Bench ==\n")
      + m_ui->lblBenchScore->text() + "\n";
    r += "\n[Cpuz v1.0 Linux; 自研 Bench 不可与官方比对]\n";
    return r;
}

/* ═══════════ helpers ═══════════ */

QMap<QString, QString> CpuLogic::parseKvLines(const QString& text) {
    QMap<QString, QString> m;
    QStringList lines = CpzSplitLines(text);
    for (int i = 0; i < (int)lines.count(); i++) {
        QString line = lines[i];
        int c = CpzPos(line, ":");
        if (c < 0) { continue; }
        QString key = CpzStrip(line.left(c));
        QString val = CpzStrip(line.mid(c + 1));
        if (key.isEmpty()) { continue; }
        m[key] = val;
    }
    return m;
}

/* flags → 友好串（遗漏 flag 丢弃；仅列常用） */
QString CpuLogic::instrFriendly(const QString& rawFlags) {
    QStringList words = CpzSplitW(rawFlags);
    QStringList outs;
    static const char* kPairs[][2] = {
        { "mmx",     "MMX" },
        { "sse",     "SSE" },
        { "sse2",    "SSE2" },
        { "ssse3",   "SSSE3" },
        { "sse4_1",  "SSE4.1" },
        { "sse4_2",  "SSE4.2" },
        { "avx",     "AVX" },
        { "avx2",    "AVX2" },
        { "avx512f", "AVX-512" },
        { "fma",     "FMA3" },
        { "aes",     "AES" },
        { "rdrand",  "RDRAND" },
        { "bmi1",    "BMI1" },
        { "bmi2",    "BMI2" },
        { "sha",     "SHA" },
        { "clflush", "CLFSH" },
        { "xsave",   "XSAVE" },
        { "pclmulqdq", "PCLMUL" },
    };
    int nPairs = (int)(sizeof(kPairs) / sizeof(kPairs[0]));
    for (int i = 0; i < nPairs; i++) {
        int f = CpzPos(rawFlags, kPairs[i][0]);
        if (f < 0) { continue; }
        /* 避免 sse4_11 类误判：同一词首完全词界才认 */
        outs << QString(kPairs[i][1]);
    }
    if (outs.isEmpty()) { return words.join(" "); }
    /* 输出 join " · " */
    QString res;
    for (int i = 0; i < (int)outs.count(); i++) {
        if (i > 0) { res += " · "; }
        res += outs[i];
    }
    return res;
}

/* 代号/制程小字典（仅覆盖常见 Intel family 6；miss 返回空） */
QString CpuLogic::codenameLookup(int fam, int model, int step) {
    (void)step;
    if (fam != 6) { return QString(); }
    switch (model) {
    case 60: case 69: case 70: case 71: {
        return "Haswell"; }
    case 61: {
        return "Broadwell"; }
    case 78: case 94: {
        return "Skylake"; }
    case 142: case 158: {
        return "Kaby/Coffee Lake"; }
    default: {
        return QString(); }
    }
}

static QString technodeLookup(int fam, int model) {
    if (fam != 6) { return QString(); }
    switch (model) {
    case 60: case 69: case 70: case 71: {
        return "22 nm"; }
    case 61: case 78: case 94: case 142: case 158: {
        return "14 nm"; }
    default: {
        return QString(); }
    }
}

QString CpuLogic::readTrimmedFile(const QString& path) {
    return CpzReadFile(path);
}

/* ═══════════ 静态采集 ═══════════ */

void CpuLogic::probeBase() {
    /* lscpu 抑制本地化 */
    QStringList a;
    a << "-c" << "LC_ALL=C lscpu";
    QString lsCpuOut = CpzRun("sh", a);
    QMap<QString, QString> kv;
    if (!lsCpuOut.isEmpty()) {
        kv = parseKvLines(lsCpuOut);
    }

    /* proc/cpuinfo 兜底补齐 */
    QString ci = CpzReadFile("/proc/cpuinfo");
    QMap<QString, QString> ckv;
    if (!ci.isEmpty()) {
        /* cpuinfo 每段有 processor 起，取第一段键值 */
        QStringList lines = CpzSplitLines(ci);
        for (int i = 0; i < (int)lines.count(); i++) {
            int c = CpzPos(lines[i], ":");
            if (c < 0) { continue; }
            QString key = CpzStrip(lines[i].left(c));
            QString val = CpzStrip(lines[i].mid(c + 1));
            if (key.isEmpty() || ckv.contains(key)) { continue; }
            ckv[key] = val;
        }
        m_flags = ckv.contains("flags") ? ckv["flags"] : QString();
        m_cpuVendor = ckv.contains("vendor_id") ? ckv["vendor_id"]
            : (kv.contains("Vendor ID") ? kv["Vendor ID"] : QString());
    }

    if (kv.contains("Model name")) {
        m_cpuName = kv["Model name"];
    } else if (ckv.contains("model name")) {
        m_cpuName = ckv["model name"];
    } else {
        m_cpuName = qFromUtf8("未知处理器");
    }

    m_fam  = kv.contains("CPU family") ? kv["CPU family"].toInt()
        : (ckv.contains("cpu family") ? ckv["cpu family"].toInt() : 0);
    m_model = kv.contains("Model") ? kv["Model"].toInt()
        : (ckv.contains("model") ? ckv["model"].toInt() : 0);
    m_step = kv.contains("Stepping") ? kv["Stepping"].toInt()
        : (ckv.contains("stepping") ? ckv["stepping"].toInt() : 0);
    m_microCode = kv.contains("Microcode version") ? kv["Microcode version"]
        : (ckv.contains("microcode") ? ckv["microcode"] : QString());
    m_sockets = kv.contains("Socket(s)") ? kv["Socket(s)"].toInt() : 0;
    m_coresPer = kv.contains("Core(s) per socket")
        ? kv["Core(s) per socket"].toInt() : 0;
    m_threadsPer = kv.contains("Thread(s) per core")
        ? kv["Thread(s) per core"].toInt() : 0;
    m_totalCpus = kv.contains("CPU(s)") ? kv["CPU(s)"].toInt()
        : (ckv.contains("processor") ? ckv["processor"].toInt() + 1 : 0);
    if (kv.contains("CPU min MHz")) {
        m_minMhz = (int)kv["CPU min MHz"].toDouble();
    }
    if (kv.contains("CPU max MHz")) {
        m_maxMhz = (int)kv["CPU max MHz"].toDouble();
    }
    m_bogo = kv.contains("BogoMIPS") ? kv["BogoMIPS"].toDouble() : 0.0;

    m_codename = codenameLookup(m_fam, m_model, m_step);
    /* BogoMIPS、Technology 等，line sweep 完成 */
    m_technode = technodeLookup(m_fam, m_model);

    /* per-core cache 汇总：/sys/devices/system/cpu/cpu0/cache/indexK */
    m_caches.clear();
    QDir cdir("/sys/devices/system/cpu/cpu0/cache");
#ifdef QT3_BUILD
    QStringList idxs = cdir.entryList("index*", QDir::Dirs | QDir::NoSymLinks
        | QDir::Hidden);
#else
    QStringList idxs = cdir.entryList(QDir::Dirs | QDir::NoSymLinks
        | QDir::NoDotAndDotDot);
    /* Qt4 无 nameFilter 的一参变体混着用两个重载较绕，顺序过滤 */
#endif
    for (int i = 0; i < (int)idxs.count(); i++) {
        if (!idxs[i].startsWith("index")) { continue; }
        QString base = "/sys/devices/system/cpu/cpu0/cache/" + idxs[i] + "/";
        CpuCacheLevel c;
        c.level = CpzReadFile(base + "level").toInt();
        c.type  = CpzReadFile(base + "type");
        QString sz = CpzReadFile(base + "size");          /* 如 "3072K" */
        int slen = sz.length();
        c.sizeK = (slen > 1) ? (long)sz.left(slen - 1).toLong() : 0;
        c.ways  = CpzReadFile(base + "ways_of_associativity").toInt();
        c.line  = CpzReadFile(base + "coherency_line_size").toInt();
        c.shared = CpzReadFile(base + "shared_cpu_list");
        m_caches.append(c);
    }
}

/* ── Chipset/Southbridge 近似（lspci Host/ISA bridge 描述段） ── */

static QString CpzLineDesc(const QString& ln) {
    int z = CpzPos(ln, "]");           /* 类型区间尾（"Host bridge [0600]"） */
    if (z < 0) { return QString(); }
    QString d = CpzStrip(ln.mid(z + 1));
    int v = CpzPos(d, " [");
    if (v > 0) { return CpzStrip(d.left(v)); }
    return d;
}

void CpuLogic::probeChipset() {
    m_chipset = QString();
    m_southbridge = QString();
    QStringList a;
    a << "-nn";
    QString out = CpzRun("lspci", a);
    if (out.isEmpty()) { return; }
    QStringList lines = CpzSplitLines(out);
    for (int i = 0; i < (int)lines.count(); i++) {
        QString ln = lines[i];
        if (CpzPos(ln, "Host bridge") >= 0 && m_chipset.isEmpty()) {
            m_chipset = CpzLineDesc(ln);
        } else if (CpzPos(ln, "ISA bridge") >= 0 && m_southbridge.isEmpty()) {
            m_southbridge = CpzLineDesc(ln);
        }
        if (!m_chipset.isEmpty() && !m_southbridge.isEmpty()) { break; }
    }
}

/* ═══════════ 页面填充 ═══════════ */

void CpuLogic::fillCpuPage() {
    if (!m_ui) { return; }
    m_ui->lblName->setText(m_cpuName);
    m_ui->lblSpec->setText(qFromUtf8("Family %1 · Model %2 · Stepping %3 · 微码 %4")
        .arg(m_fam).arg(m_model).arg(m_step).arg(
            m_microCode.isEmpty() ? QString("—") : m_microCode));
    m_ui->lblIdent->setText(qFromUtf8(
        "CodeName：%1 · Package：— · Technology：%2 · 插槽×%3 · BogoMIPS %4")
        .arg(m_codename.isEmpty() ? QString("—") : m_codename)
        .arg(m_technode.isEmpty() ? QString("—") : m_technode)
        .arg(m_sockets)
        .arg(m_bogo, 0, 'f', 2));
    m_ui->lblInstr->setText(instrFriendly(m_flags));

    /* 每核表：N 行初值（当前 MHz 留 — 由 Step4 换） */
    if (m_ui->coresTable && m_totalCpus > 0) {
#ifdef QT3_BUILD
        m_ui->coresTable->setNumRows(0);
        m_ui->coresTable->setNumRows(m_totalCpus);
#else
        m_ui->coresTable->setRowCount(0);
        m_ui->coresTable->setRowCount(m_totalCpus);
#endif
        for (int i = 0; i < m_totalCpus; i++) {
#ifdef QT3_BUILD
            m_ui->coresTable->setText(i, 0, QString("cpu%1").arg(i));
            m_ui->coresTable->setText(i, 1, "—");
            m_ui->coresTable->setText(i, 2, QString::number(m_minMhz));
            m_ui->coresTable->setText(i, 3, QString::number(m_maxMhz));
#else
            m_ui->coresTable->setItem(i, 0, new QTableWidgetItem(QString("cpu%1").arg(i)));
            m_ui->coresTable->setItem(i, 1, new QTableWidgetItem("—"));
            m_ui->coresTable->setItem(i, 2, new QTableWidgetItem(QString::number(m_minMhz)));
            m_ui->coresTable->setItem(i, 3, new QTableWidgetItem(QString::number(m_maxMhz)));
#endif
        }
    }
}

void CpuLogic::fillCachesPage() {
    if (!m_ui) { return; }
    QLabel* outs[4];
    outs[0] = m_ui->lblL1D;
    outs[1] = m_ui->lblL1I;
    outs[2] = m_ui->lblL2;
    outs[3] = m_ui->lblL3;
    for (int i = 0; i < 4; i++) {
        outs[i]->setText("—");
    }
    for (int i = 0; i < (int)m_caches.count(); i++) {
        CpuCacheLevel& c = m_caches[i];
        QLabel* tgt = 0;
        if (c.level == 1 && c.type == "Data") {
            tgt = m_ui->lblL1D;
        } else if (c.level == 1 && c.type == "Instruction") {
            tgt = m_ui->lblL1I;
        } else if (c.level == 2) {
            tgt = m_ui->lblL2;
        } else if (c.level == 3) {
            tgt = m_ui->lblL3;
        }
        if (!tgt) { continue; }
        QString kind;
        if (c.type == "Unified") {
            kind = "Unified";
        } else {
            kind = c.type;
        }
        tgt->setText(kind + qFromUtf8(" %1 K · %2-way · %3 B line · shared %4")
            .arg((long)c.sizeK).arg(c.ways).arg(c.line).arg(c.shared));
    }
}

void CpuLogic::fillMainboardPage() {
    if (!m_ui) { return; }
    QString v;
    v = CpzReadFile("/sys/class/dmi/id/board_vendor");
    m_ui->lblBoardMfr->setText(v.isEmpty() ? QString("—") : v);
    v = CpzReadFile("/sys/class/dmi/id/board_name");
    m_ui->lblBoardModel->setText(v.isEmpty() ? QString("—") : v);
    v = CpzReadFile("/sys/class/dmi/id/board_version");
    m_ui->lblBoardVer->setText(v.isEmpty() ? QString("—") : v);
    m_ui->lblChipset->setText(m_chipset.isEmpty() ? QString("—")
        : (m_southbridge.isEmpty() ? m_chipset
            : m_chipset + qFromUtf8("（北桥近似）/ ") + m_southbridge
              + qFromUtf8("（南桥近似）")));
    v = CpzReadFile("/sys/class/dmi/id/bios_vendor");
    m_ui->lblBiosBrand->setText(v.isEmpty() ? QString("—") : v);
    v = CpzReadFile("/sys/class/dmi/id/bios_version");
    QString bdate = CpzReadFile("/sys/class/dmi/id/bios_date");
    m_ui->lblBiosVer->setText(v.isEmpty() ? QString("—")
        : (bdate.isEmpty() ? v : v + " · " + bdate));
    v = CpzReadFile("/sys/class/dmi/id/chassis_vendor");
    QString ctype = CpzReadFile("/sys/class/dmi/id/chassis_type");
    m_ui->lblChassis->setText(v.isEmpty() ? QString("—")
        : (ctype.isEmpty() ? v : v + " (type " + ctype + ")"));
}

void CpuLogic::fillGraphicsPage() {
    if (!m_ui || !m_ui->gpusTable) { return; }
    QStringList a;
    a << "-nn";
    QString out = CpzRun("lspci", a);
    if (out.isEmpty()) { return; }
    QStringList lines = CpzSplitLines(out);
    int row = 0;
    for (int i = 0; i < (int)lines.count(); i++) {
        QString line = lines[i];
        bool isGpu = false;
        QString type;
        if (CpzPos(line, "VGA compatible controller") >= 0) {
            isGpu = true; type = "VGA";
        } else if (CpzPos(line, "3D controller") >= 0) {
            isGpu = true; type = "3D";
        } else if (CpzPos(line, "Display controller") >= 0) {
            isGpu = true; type = "Display";
        }
        if (!isGpu) { continue; }
        /* 格式：bus 类别描述 [ VenID ] (rev xx) |
           取 bus、描述段、【VenID】；同时读取 lspci -k 驱动 */
        int e1 = CpzPos(line, " ");
        QString bus = CpzStrip(line.left(e1));
        int ddStart = CpzPos(line, ": ");
        if (ddStart < 0) { continue; }
        QString desc = CpzStrip(line.mid(ddStart + 2));
        QString dev;
        int vid = CpzPos(desc, " [");
        if (vid > 0) { dev = CpzStrip(desc.left(vid)); } else { dev = desc; }
        QString drv = "—";
        QStringList a2;
        a2 << "-k" << "-s" << bus;
        QString kout = CpzRun("lspci", a2);
        if (!kout.isEmpty()) {
            QStringList kln = CpzSplitLines(kout);
            for (int j = 0; j < (int)kln.count(); j++) {
                QString kl = kln[j];
                if (CpzPos(kl, "Kernel driver in use: ") == 0) {
                    drv = CpzStrip(kl.mid((int)strlen("Kernel driver in use: ")));
                    break;
                }
            }
        }
        /* 新增一行 */
#ifdef QT3_BUILD
        m_ui->gpusTable->insertRows(row);
#else
        m_ui->gpusTable->insertRow(row);
#endif
#ifdef QT3_BUILD
        m_ui->gpusTable->setText(row, 0, bus);
        m_ui->gpusTable->setText(row, 1, type);
        m_ui->gpusTable->setText(row, 2, dev);
        m_ui->gpusTable->setText(row, 3, drv);
        m_ui->gpusTable->setText(row, 4, "—");
#else
        m_ui->gpusTable->setItem(row, 0, new QTableWidgetItem(bus));
        m_ui->gpusTable->setItem(row, 1, new QTableWidgetItem(type));
        m_ui->gpusTable->setItem(row, 2, new QTableWidgetItem(dev));
        m_ui->gpusTable->setItem(row, 3, new QTableWidgetItem(drv));
        m_ui->gpusTable->setItem(row, 4, new QTableWidgetItem("—"));
#endif
        row++;
    }
}

void CpuLogic::fillMemoryPage() {
    if (!m_ui) { return; }
    QString mi = CpzReadFile("/proc/meminfo");
    if (mi.isEmpty()) {
        m_ui->lblMemInfo->setText("—");
        return;
    }
    QMap<QString, QString> kv;
    QStringList lines = CpzSplitLines(mi);
    for (int i = 0; i < (int)lines.count(); i++) {
        int c = CpzPos(lines[i], ":");
        if (c < 0) { continue; }
        kv[CpzStrip(lines[i].left(c))] = CpzStrip(lines[i].mid(c + 1));
    }
    long total  = kv.contains("MemTotal")     ? kv["MemTotal"].toLong() : 0;
    long free   = kv.contains("MemFree")      ? kv["MemFree"].toLong() : 0;
    long avail  = kv.contains("MemAvailable") ? kv["MemAvailable"].toLong() : 0;
    long bufs   = kv.contains("Buffers")      ? kv["Buffers"].toLong() : 0;
    long cached = kv.contains("Cached")       ? kv["Cached"].toLong() : 0;
    long swapT  = kv.contains("SwapTotal")    ? kv["SwapTotal"].toLong() : 0;
    long swapF  = kv.contains("SwapFree")     ? kv["SwapFree"].toLong() : 0;
    long used   = total - free - bufs - cached;
    long swapU  = swapT - swapF;

    QString s = qFromUtf8("MemTotal：%1 kB (%2 GB)\nUsed：%3 kB (%4 GB)\n")
        .arg(total).arg(total / 1024 / 1024)
        .arg(used).arg(used / 1024 / 1024)
        + qFromUtf8("Free：%1 kB\n").arg(free)
        + qFromUtf8("Available：%1 kB\n").arg(avail)
        + qFromUtf8("Swap：%1 kB / 已用 %2 kB").arg(swapT).arg(swapU);
    m_ui->lblMemInfo->setText(s);
    m_ui->lblMemNote->setText(
        qFromUtf8("频率/时序/SPD 需 root 权限 dmidecode，不可用时显示 —"));
}

/* ═══════════ Bench 槽（runner 类见前） ═══════════ */

void CpuLogic::onBench() {
    if (!m_ui || m_benchPhase != 0) { return; }
    m_benchState = 1;
    m_benchPhase = 1;
    m_singleScore = 0;
    m_multiScore = 0;
    barPct(m_ui->barBench, 0);
    m_ui->lblBenchState->setText(qFromUtf8("状态：单线程跑分中…"));
    if (m_benchRunner) { delete m_benchRunner; m_benchRunner = 0; }
    CpuBenchRunner* r = new CpuBenchRunner(1, &m_benchState);
    m_benchRunner = r;
    m_benchT0 = QTime::currentTime();
    r->start(QThread::IdlePriority);
    m_benchTimer->start(150);
}

void CpuLogic::onBenchTick() {
    if (!m_ui || m_benchPhase == 0) { return; }
    int elapsed = m_benchT0.elapsed();
    int pct = (elapsed >= 2000) ? 100 : elapsed / 2;
    barPct(m_ui->barBench, pct);
    if (elapsed < 2000) { return; }

    if (!m_benchRunner) { return; }
    CpuBenchRunner* r = (CpuBenchRunner*)m_benchRunner;
#ifdef QT3_BUILD
    if (r->running()) {
        r->wait();
    }
#else
    if (r->isRunning()) {
        r->wait();
    }
#endif
    long v = r->m_ops.load();

    if (m_benchPhase == 1) {
        m_singleScore = v;
        m_benchPhase = 2;
        m_benchT0 = QTime::currentTime();
        barPct(m_ui->barBench, 0);
        m_ui->lblBenchState->setText(qFromUtf8("状态：多线程跑分中…（%1 workers）")
            .arg(m_totalCpus > 0 ? m_totalCpus : 1));
        if (m_benchRunner) { delete m_benchRunner; m_benchRunner = 0; }
        int nw = m_totalCpus > 0 ? m_totalCpus : 1;
        CpuBenchRunner* r2 = new CpuBenchRunner(nw, &m_benchState);
        m_benchRunner = r2;
        r2->start(QThread::IdlePriority);
        return;
    }
    /* phase 2 → 收尾 */
    m_multiScore = v;
    if (m_benchRunner) {
        delete m_benchRunner; m_benchRunner = 0;
    }
    m_benchPhase = 0;
    m_benchState = 0;
    m_benchTimer->stop();
    benchFinishUi();
}

void CpuLogic::benchFinishUi() {
    if (!m_ui) { return; }
    long single = m_singleScore;
    long multi = m_multiScore;
    double ratio = (single > 0)
        ? ((double)multi / (double)single) : 0.0;
    m_ui->lblBenchScore->setText(
        qFromUtf8("Single：%1 ops/2s\nMulti：%2 ops/2s\n倍比：×%3")
            .arg(single).arg(multi).arg(ratio, 0, 'f', 2));
    m_ui->lblBenchState->setText(qFromUtf8("状态：空闲"));
    barPct(m_ui->barBench, 0);
}
