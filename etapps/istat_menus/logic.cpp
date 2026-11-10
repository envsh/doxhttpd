#include "logic.h"
#include "compat34.h"
#include "compatcore34.h"          /* qRuncmdCaptureOuterr */
#include <qfile.h>
#include <qtimer.h>
#include <qpainter.h>
#include <qcolor.h>
#include <qstringlist.h>
#include <qdatetime.h>
#include <time.h>
#include <string.h>

/* ═══════════ 文本/文件 helpers（cpuz 同源，前缀 Istm） ═══════════ */

static QString IstmStrip(const QString& s) {
    QString t = s;
#ifdef QT3_BUILD
    return t.stripWhiteSpace();
#else
    return t.trimmed();
#endif
}

static int IstmPos(const QString& s, const char* needle) {
#ifdef QT3_BUILD
    return s.find(QString(needle));
#else
    return s.indexOf(QString(needle), 0);
#endif
}

static QStringList IstmSplitLines(const QString& s) {
#ifdef QT3_BUILD
    return QStringList::split('\n', s, false);
#else
    return s.split('\n', QString::KeepEmptyParts);
#endif
}

static QString IstmReadFile(const QString& path) {
    QFile f(path);
#ifdef QT3_BUILD
    if (!f.open(IO_ReadOnly)) { return QString(); }
#else
    if (!f.open(QIODevice::ReadOnly)) { return QString(); }
#endif
    /* Qt3 QIODevice::readAll 内部 while(!atEnd()) 在 sysfs 上会死循环
       （readBlock 失败不推进读位 → atEnd 永不成立 → CPU 空转）。
       改为确定性 readBlock：n<=0 即停，n>0 必推进。 */
    QByteArray a;
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
    return IstmStrip(QString::fromUtf8(a.data(), (int)a.size()));
}

static QString IstmRun(const QString& program, const QStringList& args) {
    QString out;
    qRuncmdCaptureOuterr(program, args, &out);
    return out;
}

/* 空白符分词（/proc/stat、/proc/swaps、/proc/pressure、loadavg 共用） */
static QStringList IstmTokens(const QString& line) {
    QStringList out;
    QString cur;
    for (int i = 0; i < (int)line.length(); i++) {
        QChar ch = line[i];
        if (ch == ' ' || ch == '\t') {
            if (!cur.isEmpty()) { out.append(cur); cur = QString(); }
        } else {
            cur += ch;
        }
    }
    if (!cur.isEmpty()) { out.append(cur); }
    return out;
}

/* "Key: 1234 kB" → kv[k]=1234（KB 值取首个 token） */
static void IstmParseKv(const QString& text, QMap<QString, long long>& kv) {
    QStringList lines = IstmSplitLines(text);
    for (int i = 0; i < (int)lines.count(); i++) {
        int c = IstmPos(lines[i], ":");
        if (c <= 0) { continue; }
        QString k = IstmStrip(lines[i].left(c));
        QString v = IstmStrip(lines[i].mid(c + 1));
        long long num = 0;
        if (!v.isEmpty()) {
            QStringList tk = IstmTokens(v);
            if (tk.count() > 0) {
                num = (long long)tk[0].toDouble();
            }
        }
        kv.insert(k, num);
    }
}

/* ── 2 进制 KB humanize（iStat 口径：MiB=1024KiB） ── */
static QString IstmHumanKB(long long kb) {
    if (kb < 0) { kb = 0; }
    if (kb < 1024) { return QString("%1 KB").arg((int)kb); }
    double m = (double)kb / 1024.0;
    if (m < 1024.0) { return QString("%1 MB").arg(m, 0, 'f', 1); }
    return QString("%1 GB").arg(m / 1024.0, 0, 'f', 2);
}

static QString IstmHumanDur(double secs) {
    long long s = (long long)secs;
    long long day = s / 86400;
    long long hr = (s % 86400) / 3600;
    return qFromUtf8("%1 天 %2 小时").arg((int)day).arg((int)hr);
}

/* B/s → iStat 口径速率文本（2 分制） */
static QString IstmHumanRate(double bps) {
    if (bps < 1024.0) {
        return QString("%1 B/s").arg((int)(bps + 0.5));
    }
    double k = bps / 1024.0;
    if (k < 1024.0) { return QString("%1 KB/s").arg(k, 0, 'f', 1); }
    return QString("%1 MB/s").arg(k / 1024.0, 0, 'f', 2);
}

/* 表格行数跨栈（Qt3 setNumRows / Qt4 setRowCount） */
static void IstmSetRows(IstmTable* t, int rows) {
    if (!t) { return; }
#ifdef QT3_BUILD
    t->setNumRows(rows);
#else
    t->setRowCount(rows);
#endif
}

static void IstmBarPct(QProgressBar* bar, int pct) {
    if (!bar) { return; }
#ifdef QT3_BUILD
    bar->setProgress(pct);
#else
    bar->setValue(pct);
#endif
}

static void IstmSetCell(IstmTable* t, int row, int col, const QString& s) {
    if (!t) { return; }
#ifdef QT3_BUILD
    t->setText(row, col, s);
#else
    QTableWidgetItem* it = t->item(row, col);
    if (!it) { t->setItem(row, col, new QTableWidgetItem(s)); }
    else { it->setText(s); }
#endif
}

/* PSI 行 token 里取 "key=value" 的 value（如 avg10=0.06 → 0.06） */
static QString IstmPsiTok(const QStringList& tk, const char* key) {
    for (int i = 0; i < (int)tk.count(); i++) {
        if (IstmPos(tk[i], key) == 0) {
            return IstmStrip(tk[i].mid((int)strlen(key)));
        }
    }
    return QString();
}

/* Step7：世界钟 6 城（固定 UTC offset，不做 DST——iStat 简化口径） */
static QString IstmWorldClocks() {
    struct tm gbuf;
    time_t t = time(0);
    gmtime_r(&t, &gbuf);
    long long secOfDay = (long long)gbuf.tm_hour * 3600
        + (long long)gbuf.tm_min * 60;
    static const char* cities[] =
        { "UTC", "纽约", "伦敦", "北京", "东京", "悉尼" };
    static const int offs[] = { 0, -5, 0, 8, 9, 10 };
    /* 纽约 EST -5：夏令时期间差 1h（注记） */
    QString s;
    for (int i = 0; i < 6; i++) {
        long long sec = secOfDay + (long long)offs[i] * 3600;
        sec = ((sec % 86400) + 86400) % 86400;
        int hh = (int)(sec / 3600);
        int mm = (int)((sec % 3600) / 60);
        if (i > 0) { s += "\n"; }
        s += qFromUtf8("%1 %2:%3（UTC%4%5）")
            .arg(qFromUtf8(cities[i]))
            .arg(hh, 2, 10, QChar('0'))
            .arg(mm, 2, 10, QChar('0'))
            .arg(offs[i] >= 0 ? QString("+") : QString("-"))
            .arg(offs[i] >= 0 ? offs[i] : -offs[i]);
    }
    return s;
}

/* Step7：顶条本地时钟（每 tick） */
static void IstmTopClockText(QLabel* l) {
    if (!l) { return; }
    QTime now = QTime::currentTime();
    QDate d = QDate::currentDate();
    static const char* wd[] =
        { "一", "二", "三", "四", "五", "六", "日" };
    l->setText(qFromUtf8("%1 · %2 周%3")
        .arg(now.toString("hh:mm"))
        .arg(d.toString("MM-dd"))
        .arg(qFromUtf8(wd[d.dayOfWeek() - 1])));
}

/* ═══════════ IstatGraph ═══════════ */

IstatGraph::IstatGraph(QWidget* parent)
    : QFrame(parent)
{
    m_cap = 60;
    m_head = 0;
    m_n = 0;
    m_colR = 76;
    m_colG = 175;
    m_colB = 80;
    for (int i = 0; i < ISTM_RING_MAX; i++) { m_vals[i] = 0.0; }
    setFixedHeight(42);
    setMinimumWidth(80);
}

void IstatGraph::setRing(int cap) {
    if (cap >= 10 && cap <= ISTM_RING_MAX) { m_cap = cap; }
}

void IstatGraph::setColor(int r, int g, int b) {
    m_colR = r;
    m_colG = g;
    m_colB = b;
}

void IstatGraph::addSample(double v01) {
    if (v01 < 0.0) { v01 = 0.0; }
    if (v01 > 1.0) { v01 = 1.0; }
    m_vals[m_head] = v01;
    m_head = (m_head + 1) % m_cap;
    if (m_n < m_cap) { m_n++; }
    update();
}

void IstatGraph::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(0, 0, width(), height(), QColor(246, 246, 246));
    p.fillRect(0, height() - 2, width(), 1, QColor(150, 150, 150));
    if (m_n <= 0) { return; }
    double step = (double)width() / (double)m_cap;
    int bh0 = height() - 4;
    for (int s = 0; s < m_n; s++) {
        int idx = m_head - m_n + s;
        while (idx < 0) { idx += m_cap; }
        int bh = (int)((double)bh0 * m_vals[idx] + 0.5);
        int bw = (int)step >= 2 ? (int)step - 1 : 1;
        p.fillRect((int)(s * step) + 1, height() - 2 - bh, bw, bh,
            QColor(m_colR, m_colG, m_colB));
    }
}

/* ═══════════ 温度（coretemp Package 汇总 + per-core topology 映射） ═══════════ */

/* cpuN → topology.core_id → coretemp "Core X"（HT 下同物理核共用一路温度） */
static int IstmCoreTemp(int cpuIdx) {
    QString cid = IstmReadFile(QString(
        "/sys/devices/system/cpu/cpu%1/topology/core_id").arg(cpuIdx));
    if (cid.isEmpty()) { return -1; }
    for (int h = 0; h < 12; h++) {
        QString dir = QString("/sys/class/hwmon/hwmon%1").arg(h);
        QString nm = IstmReadFile(dir + "/name");
        if (nm != "coretemp") { continue; }
        for (int j = 1; j <= 16; j++) {
            QString lab = IstmReadFile(dir
                + QString("/temp%1_label").arg(j));
            if (lab.isEmpty()) { continue; }
            QString want = qFromUtf8("Core ") + cid;
            if (lab != want) { continue; }
            QString vin = IstmReadFile(dir
                + QString("/temp%1_input").arg(j));
            if (vin.isEmpty()) { continue; }
            int mc = vin.toInt();
            if (mc <= 0 || mc >= 110000) { continue; }
            return mc / 1000;
        }
    }
    return -1;
}

/* 顶条汇总温度：coretemp "Package id 0"（cpuz CpzTemp 精简版） */
static int IstmPkgTemp() {
    for (int h = 0; h < 12; h++) {
        QString dir = QString("/sys/class/hwmon/hwmon%1").arg(h);
        QString nm = IstmReadFile(dir + "/name");
        if (nm != "coretemp") { continue; }
        QString vin = IstmReadFile(dir + "/temp1_input");
        QString lab = IstmReadFile(dir + "/temp1_label");
        if (IstmPos(lab, "Package") < 0) { continue; }
        int mc = vin.toInt();
        if (mc <= 0 || mc >= 110000) { continue; }
        return mc / 1000;
    }
    return -1;
}

/* ═══════════ Logic 基础 ═══════════ */

IstatLogic::IstatLogic(QObject* parent)
    : QObject(parent)
{
    m_ui = 0;
    m_tickTimer = 0;
    m_tick = 0;
    m_ncpu = 0;
    m_prevTotal = 0;
    m_prevBusy = 0;
    m_cpuPct = 0;
    for (int i = 0; i < ISTM_MAXCORES; i++) {
        m_prevCoreAll[i] = 0;
        m_prevCoreBusy[i] = 0;
        m_corePct[i] = 0;
    }
    m_memTotalKB = 0;
    m_memUsedKB = 0;
    m_netRows = 0;
    m_netPeakRx = 65536.0;
    m_netPeakTx = 65536.0;
    m_prevSect = 0;
    m_prevIos = 0;
    m_diskRows = 0;
    m_diskPeak = 1024.0;                   /* 1MB/s 下限 */
    m_diskName = QString();
    m_diskIops = 0;
    m_diskQueue = 0;
    m_diskKbps = 0.0;
    m_alertCpuHi = 0;
    m_alertMemHi = 0;
    m_alertCoolCpu = 0;
    m_alertCoolMem = 0;
    m_alertCoolDisk = 0;
    m_alertCoolBat = 0;
    m_alertNetHi = 0;
    m_alertCoolNet = 0;
    m_netSumRx = 0.0;
    m_netSumTx = 0.0;
    m_alertHist = QString();
}

IstatLogic::~IstatLogic() {
}

void IstatLogic::setWidgets(IstatUi* ui) {
    m_ui = ui;
    probeCpuStatic();
    fillGpuRow();              /* GPU 硬件清单不变：一次性 */

    m_tickTimer = new QTimer(this);
    connect(m_tickTimer, SIGNAL(timeout()), this, SLOT(onTick()));
    m_tickTimer->start(1000);
}

/* /proc/cpuinfo：逻辑核数 + "model name" */
void IstatLogic::probeCpuStatic() {
    m_cpuModel = QString();
    m_ncpu = 0;
    QStringList lines = IstmSplitLines(
        IstmReadFile("/proc/cpuinfo"));
    for (int i = 0; i < (int)lines.count(); i++) {
        const QString& ln = lines[i];
        if (IstmPos(ln, "model name") == 0 && m_cpuModel.isEmpty()) {
            int c = IstmPos(ln, ":");
            if (c > 0) { m_cpuModel = IstmStrip(ln.mid(c + 1)); }
        } else if (IstmPos(ln, "processor") == 0) {
            if (m_ncpu < ISTM_MAXCORES) { m_ncpu++; }
        }
    }
}

/* ═══════════ CPU 页 ═══════════ */

static int IstmCurMhz(int cpuIdx) {
    QString v = IstmReadFile(QString(
        "/sys/devices/system/cpu/cpu%1/cpufreq/scaling_cur_freq")
        .arg(cpuIdx));
    if (v.isEmpty()) { return -1; }
    return v.toInt() / 1000;               /* kHz→MHz */
}

void IstatLogic::sampleCpu() {
    QStringList lines = IstmSplitLines(
        IstmReadFile("/proc/stat"));
    for (int i = 0; i < (int)lines.count(); i++) {
        QStringList tk = IstmTokens(lines[i]);
        if (tk.count() < 8) { continue; }
        long long total = 0;
        for (int j = 1; j <= 10 && j < (int)tk.count(); j++) {
            total += (long long)tk[j].toDouble();
        }
        long long busy = (long long)tk[1].toDouble()
            + (long long)tk[2].toDouble()        /* nice */
            + (long long)tk[3].toDouble()        /* system */
            + (long long)tk[6].toDouble()        /* irq */
            + (long long)tk[7].toDouble()        /* softirq */
            + (long long)tk[8].toDouble();       /* steal */
        if (tk[0] == "cpu") {
            long long dAll = total - m_prevTotal;
            long long dBusy = busy - m_prevBusy;
            if (dAll > 0) {
                m_cpuPct = (int)(100 * dBusy / dAll);
                if (m_cpuPct < 0) { m_cpuPct = 0; }
                if (m_cpuPct > 100) { m_cpuPct = 100; }
            }
            m_prevTotal = total;
            m_prevBusy = busy;
        } else if (IstmPos(tk[0], "cpu") == 0) {
            int idx = tk[0].mid(3).toInt();
            if (idx < 0 || idx >= ISTM_MAXCORES) { continue; }
            long long dAll = total - m_prevCoreAll[idx];
            long long dBusy = busy - m_prevCoreBusy[idx];
            if (dAll > 0) {
                m_corePct[idx] = (int)(100 * dBusy / dAll);
                if (m_corePct[idx] < 0) { m_corePct[idx] = 0; }
                if (m_corePct[idx] > 100) { m_corePct[idx] = 100; }
            }
            m_prevCoreAll[idx] = total;
            m_prevCoreBusy[idx] = busy;
        }
    }

    if (!m_ui) { return; }

    m_ui->lblCpuBig->setText(qFromUtf8("%1%").arg(m_cpuPct));
    m_ui->graphCpu->addSample((double)m_cpuPct / 100.0);

    QStringList la = IstmTokens(IstmReadFile("/proc/loadavg"));
    QString laTxt = qFromUtf8("load —");
    if (la.count() >= 3) {
        laTxt = qFromUtf8("load %1 %2 %3")
            .arg(la[0]).arg(la[1]).arg(la[2]);
    }
    QStringList up = IstmTokens(IstmReadFile("/proc/uptime"));
    if (up.count() >= 1) {
        laTxt += qFromUtf8(" · uptime ") +
            IstmHumanDur(up[0].toDouble());
    }
    if (!m_cpuModel.isEmpty()) {
        laTxt += " · " + m_cpuModel;
    }
    m_ui->lblLoad->setText(laTxt);

    for (int i = 0; i < m_ncpu; i++) {
        IstmSetCell(m_ui->tblCores, i, 0, QString("cpu%1").arg(i));
        int mhz = IstmCurMhz(i);
        IstmSetCell(m_ui->tblCores, i, 1,
            mhz > 0 ? QString("%1").arg(mhz) : QString("—"));
        IstmSetCell(m_ui->tblCores, i, 2,
            qFromUtf8("%1%").arg(m_corePct[i]));
        int tc = IstmCoreTemp(i);
        IstmSetCell(m_ui->tblCores, i, 3,
            tc >= 0 ? qFromUtf8("%1°C").arg(tc) : QString("—"));
    }
}

/* ═══════════ Memory 页 ═══════════ */

void IstatLogic::sampleMem() {
    QMap<QString, long long> kv;
    IstmParseKv(IstmReadFile("/proc/meminfo"), kv);
    long long total = kv.contains("MemTotal") ? kv["MemTotal"] : 0;
    long long avail = kv.contains("MemAvailable") ? kv["MemAvailable"] : 0;
    long long freeM = kv.contains("MemFree") ? kv["MemFree"] : 0;
    if (avail == 0 && freeM > 0) {
        avail = freeM;                     /* 老内核无 Available 回退 */
    }
    m_memTotalKB = total;
    m_memUsedKB = total - avail;

    if (!m_ui) { return; }

    int pct = total > 0 ? (int)(100 * m_memUsedKB / total) : 0;
    if (pct < 0) { pct = 0; }
    if (pct > 100) { pct = 100; }

    m_ui->lblMemBig->setText(qFromUtf8("%1 / %2（%3%）")
        .arg(IstmHumanKB(m_memUsedKB))
        .arg(IstmHumanKB(total))
        .arg(pct));
    IstmBarPct(m_ui->barMem, pct);
    m_ui->graphMem->addSample((double)pct / 100.0);

    long long cached = kv.contains("Cached") ? kv["Cached"] : 0;
    long long sre = kv.contains("SReclaimable") ? kv["SReclaimable"] : 0;
    long long shmem = kv.contains("Shmem") ? kv["Shmem"] : 0;
    long long swc = kv.contains("SwapCached") ? kv["SwapCached"] : 0;
    m_ui->lblMemKv->setText(qFromUtf8(
        "available %1\ncached %2（+SReclaimable %3）\nshmem %4 · swapcached %5")
        .arg(IstmHumanKB(avail))
        .arg(IstmHumanKB(cached))
        .arg(IstmHumanKB(sre))
        .arg(IstmHumanKB(shmem))
        .arg(IstmHumanKB(swc)));

    /* swap 总量 + /proc/swaps 分设备："/dev/sda6 partition 2097148 123 0 -2" */
    long long swTot = kv.contains("SwapTotal") ? kv["SwapTotal"] : 0;
    long long swFree = kv.contains("SwapFree") ? kv["SwapFree"] : 0;
    long long swUsed = swTot - swFree;
    if (swUsed < 0) { swUsed = 0; }
    QString swTxt = qFromUtf8("swap %1 / %2（%3%）")
        .arg(IstmHumanKB(swUsed))
        .arg(IstmHumanKB(swTot))
        .arg(swTot > 0 ? (int)(100 * swUsed / swTot) : 0);
    QStringList sl = IstmSplitLines(IstmReadFile("/proc/swaps"));
    for (int i = 1; i < (int)sl.count(); i++) {     /* 第 0 行表头跳过 */
        QStringList tk = IstmTokens(sl[i]);
        if (tk.count() < 5) { continue; }
        swTxt += qFromUtf8("\n· %1：%2 / %3")
            .arg(tk[0])
            .arg(IstmHumanKB((long long)tk[4].toDouble()))
            .arg(IstmHumanKB((long long)tk[3].toDouble()));
    }
    m_ui->lblSwap->setText(swTxt);

    /* PSI：some avg10=.. avg60=.. avg300=.. / full … */
    QStringList pn = IstmSplitLines(
        IstmReadFile("/proc/pressure/memory"));
    QString psiTxt = qFromUtf8("PSI —");
    if (pn.count() >= 2) {
        QStringList tSome = IstmTokens(pn[0]);
        QStringList tFull = IstmTokens(pn[1]);
        psiTxt = qFromUtf8("PSI some %1 / %2 / %3 · full %4")
            .arg(IstmPsiTok(tSome, "avg10="))
            .arg(IstmPsiTok(tSome, "avg60="))
            .arg(IstmPsiTok(tSome, "avg300="))
            .arg(IstmPsiTok(tFull, "avg10="));
    }
    m_ui->lblPsi->setText(psiTxt);

    if (m_ui->topMem) {
        m_ui->topMem->setText(qFromUtf8("MEM %1%").arg(pct));
    }
}

/* ═══════════ Network 页（/proc/net/dev 差分 + ip/ss + 公网 IP best-effort） ═══════════ */

void IstatLogic::sampleNet() {
    if (!m_ui) { return; }
    QStringList lines = IstmSplitLines(
        IstmReadFile("/proc/net/dev"));
    double sumRx = 0.0;
    double sumTx = 0.0;
    int row = 0;
    for (int i = 2; i < (int)lines.count(); i++) {   /* 前 2 行表头跳过 */
        QStringList tk = IstmTokens(lines[i]);
        if (tk.count() < 10) { continue; }
        QString name = tk[0];
        if (IstmPos(name, ":") == (int)name.length() - 1) {
            name = name.left(name.length() - 1);
        }
        if (name == "lo") { continue; }              /* 环回不计入 */
        long long rx = (long long)tk[1].toDouble();
        long long tx = (long long)tk[9].toDouble();
        double rRx = 0.0;
        double rTx = 0.0;
        if (m_prevRx.contains(name) && m_prevTx.contains(name)) {
            long long d = rx - m_prevRx[name];
            if (d > 0) { rRx = (double)d; }
            d = tx - m_prevTx[name];
            if (d > 0) { rTx = (double)d; }
        }
        m_prevRx.insert(name, rx);
        m_prevTx.insert(name, tx);
        m_rateRx.insert(name, rRx);
        m_rateTx.insert(name, rTx);
        sumRx += rRx;
        sumTx += rTx;

        if (row < ISTM_MAXIF) {                      /* 表内逐行填充 */
            QString ipTxt = m_ifIp.contains(name)
                ? m_ifIp[name] : QString("—");
            IstmSetCell(m_ui->tblNet, row, 0, name);
            IstmSetCell(m_ui->tblNet, row, 1, ipTxt);
            IstmSetCell(m_ui->tblNet, row, 2, IstmHumanRate(rRx));
            IstmSetCell(m_ui->tblNet, row, 3, IstmHumanRate(rTx));
            IstmSetCell(m_ui->tblNet, row, 4, IstmHumanKB(rx));
            IstmSetCell(m_ui->tblNet, row, 5, IstmHumanKB(tx));
            row++;
        }
    }
    for (int e = row; e < m_netRows; e++) {          /* 行脚清空残留 */
        IstmSetCell(m_ui->tblNet, e, 0, QString(""));
        IstmSetCell(m_ui->tblNet, e, 1, QString(""));
        IstmSetCell(m_ui->tblNet, e, 2, QString(""));
        IstmSetCell(m_ui->tblNet, e, 3, QString(""));
        IstmSetCell(m_ui->tblNet, e, 4, QString(""));
        IstmSetCell(m_ui->tblNet, e, 5, QString(""));
    }

    /* 图归一化：峰值自适应（下限 64KB/s，避免空转全平） */
    if (sumRx > m_netPeakRx) { m_netPeakRx = sumRx; }
    if (sumTx > m_netPeakTx) { m_netPeakTx = sumTx; }
    m_ui->graphNetRx->addSample(sumRx / m_netPeakRx);
    m_ui->graphNetTx->addSample(sumTx / m_netPeakTx);

    m_ui->lblNetBig->setText(qFromUtf8("↓ %1  ↑ %2")
        .arg(IstmHumanRate(sumRx))
        .arg(IstmHumanRate(sumTx)));
    if (m_ui->topNet) {
        m_ui->topNet->setText(qFromUtf8("NET ↓ %1 ↑ %2")
            .arg(IstmHumanRate(sumRx))
            .arg(IstmHumanRate(sumTx)));
    }
    m_netSumRx = sumRx;
    m_netSumTx = sumTx;
}

/* 每 10 tick：ip -brief addr + ss -s + 表行数重估 */
void IstatLogic::fillNetStatic() {
    if (!m_ui) { return; }
    m_ifIp.clear();
    m_netRows = 0;

    QStringList ipArgs;
    ipArgs << "-brief" << "addr";
    QString out = IstmRun("ip", ipArgs);
    QStringList lines = IstmSplitLines(out);
    for (int i = 0; i < (int)lines.count(); i++) {
        QStringList tk = IstmTokens(lines[i]);
        if (tk.count() < 2) { continue; }
        QString name = tk[0];
        if (name == "lo") { continue; }
        /* "wlan0 UP 192.168.49.136/24 fe80::…" → state + 首个 v4 */
        QString disp = tk[1];
        for (int t = 2; t < (int)tk.count() && t < 4; t++) {
            if (IstmPos(tk[t], ":") >= 0) { continue; }   /* 跳 IPv6 */
            disp += " " + tk[t];
            break;
        }
        m_ifIp.insert(name, disp);
    }

    /* 表行数 = 本轮 /proc/net/dev 的非 lo IF 数 */
    QStringList dev = IstmSplitLines(
        IstmReadFile("/proc/net/dev"));
    for (int i = 2; i < (int)dev.count(); i++) {
        QStringList tk = IstmTokens(dev[i]);
        if (tk.count() < 2) { continue; }
        m_netRows++;
    }
    if (m_netRows > ISTM_MAXIF) { m_netRows = ISTM_MAXIF; }
    IstmSetRows(m_ui->tblNet, m_netRows);

    /* ss -s 头部 TCP 统计行 */
    QStringList ssArgs;
    ssArgs << "-s";
    QString sin = IstmRun("ss", ssArgs);
    QStringList sl = IstmSplitLines(sin);
    m_ssLine = sl.count() > 1 ? IstmStrip(sl[1]) : QString();

    refreshNetPub();
}

/* 公网 IP：curl 主备；失败 "—"。每 120 tick 一次 */
void IstatLogic::fetchPubIp() {
    QString txt = IstmRun("curl", QStringList()
        << "-s" << "-m" << "5" << "https://ifconfig.me");
    txt = IstmStrip(txt);
    if (txt.isEmpty()) {
        txt = IstmRun("curl", QStringList()
            << "-s" << "-m" << "5" << "https://api.ipify.org");
        txt = IstmStrip(txt);
    }
    if (txt.length() > 64) { txt = QString(); }        /* 异常回显保护 */
    m_pubIp = txt;
    refreshNetPub();
}

void IstatLogic::refreshNetPub() {
    if (!m_ui) { return; }
    QString txt = qFromUtf8("公网 IP：") +
        (m_pubIp.isEmpty() ? QString("—") : m_pubIp);
    if (!m_ssLine.isEmpty()) {
        txt += "\n" + m_ssLine;
    }
    m_ui->lblNetPub->setText(txt);
}

/* ═══════════ Disk 页（/sys/block/<d>/stat 差分 + df 分区用量） ═══════════ */

/* 主块设备探测：nvme0n1 → sda → sdb → vda → hda 依序命中（本机 sda） */
static QString IstmPickDisk() {
    static const char* cands[] =
        { "nvme0n1", "sda", "sdb", "vda", "hda" };
    for (int i = 0; i < 5; i++) {
        QString v = IstmReadFile(
            QString("/sys/block/%1/stat").arg(cands[i]));
        if (!v.isEmpty()) { return QString(cands[i]); }
    }
    return QString();
}

void IstatLogic::sampleDisk() {
    if (!m_ui) { return; }
    if (m_diskName.isEmpty()) { m_diskName = IstmPickDisk(); }
    if (m_diskName.isEmpty()) {
        m_ui->lblDiskBig->setText(qFromUtf8("主块设备未找到"));
        return;
    }
    QStringList tk = IstmTokens(IstmReadFile(
        QString("/sys/block/%1/stat").arg(m_diskName)));
    if (tk.count() < 9) { return; }
    long long sect = (long long)tk[2].toDouble()      /* 列 3：扇区读 */
        + (long long)tk[6].toDouble();                /* 列 7：扇区写 */
    long long ios = (long long)tk[0].toDouble()       /* 列 1：读完成 */
        + (long long)tk[4].toDouble();                /* 列 5：写完成 */
    long long queue = (long long)tk[8].toDouble();    /* 列 9：in_flight */

    double kbps = 0.0;
    int iops = 0;
    if (m_prevSect > 0 || m_prevIos > 0) {            /* 首 tick 置零 */
        long long dS = sect - m_prevSect;
        long long dI = ios - m_prevIos;
        if (dS > 0) { kbps = (double)dS * 512.0 / 1024.0; }
        if (dI > 0) { iops = (int)dI; }
    }
    m_prevSect = sect;
    m_prevIos = ios;
    m_diskKbps = kbps;
    m_diskIops = iops;
    m_diskQueue = (int)queue;

    m_ui->lblDiskBig->setText(qFromUtf8("%1 KB/s（%2）")
        .arg(m_diskKbps, 0, 'f', 1)
        .arg(m_diskName));
    m_ui->lblDiskIops->setText(qFromUtf8("IOPS %1 · 队列 %2")
        .arg(m_diskIops)
        .arg(m_diskQueue));
    if (m_diskKbps > m_diskPeak) { m_diskPeak = m_diskKbps; }
    m_ui->graphDisk->addSample(m_diskKbps / m_diskPeak);
}

/* 每 10 tick：df 分区用量（仅真实块设备分区，排除 loop/tmpfs） */
void IstatLogic::fillDiskStatic() {
    if (!m_ui) { return; }
    m_diskRows = 0;
    QString out = IstmRun("df",
        QStringList() << "-k" << "-P");
    QStringList lines = IstmSplitLines(out);
    int row = 0;
    for (int i = 1; i < (int)lines.count() && row < ISTM_MAXIF; i++) {
        QStringList tk = IstmTokens(lines[i]);
        if (tk.count() < 6) { continue; }
        if (IstmPos(tk[0], "/dev/") != 0) { continue; }
        if (IstmPos(tk[0], "/dev/loop") == 0) { continue; }
        IstmSetCell(m_ui->tblDisk, row, 0, tk[0]);
        IstmSetCell(m_ui->tblDisk, row, 1, qFromUtf8("分区"));
        IstmSetCell(m_ui->tblDisk, row, 2,
            IstmHumanKB((long long)tk[1].toDouble()));
        IstmSetCell(m_ui->tblDisk, row, 3, tk[4]);
        /* 挂载点可能含空格：join 余下 token */
        QString mnt;
        for (int t = 5; t < (int)tk.count(); t++) {
            if (t > 5) { mnt += " "; }
            mnt += tk[t];
        }
        IstmSetCell(m_ui->tblDisk, row, 4, mnt);
        row++;
        m_diskRows = row;
    }
    IstmSetRows(m_ui->tblDisk, m_diskRows);
}

/* ═══════════ GPU 清单（lspci -knn 一次性；desc + kernel driver） ═══════════ */

void IstatLogic::fillGpuRow() {
    if (!m_ui || !m_ui->lblGpu) { return; }
    QString out = IstmRun("lspci",
        QStringList() << "-knn");
    QStringList lines = IstmSplitLines(out);
    QString txt;
    QString pending;
    for (int i = 0; i < (int)lines.count(); i++) {
        QString ln = IstmStrip(lines[i]);
        if (IstmPos(ln, "VGA compatible controller") >= 0
            || IstmPos(ln, "3D controller") >= 0
            || IstmPos(ln, "Display controller") >= 0) {
            /* "00:02.0 VGA … [0300]: Intel … [8086:0416] (rev 06)" */
            int c = IstmPos(ln, "]:");
            if (c > 0) { pending = IstmStrip(ln.mid(c + 1)); }
        } else if (IstmPos(ln, "Kernel driver in use:") == 0
            && !pending.isEmpty()) {
            pending += qFromUtf8("（%1）")
                .arg(IstmStrip(ln.mid(21)));
            if (!txt.isEmpty()) { txt += "\n"; }
            txt += qFromUtf8("GPU：") + pending;
            pending = QString();
        }
    }
    m_ui->lblGpu->setText(txt.isEmpty()
        ? qFromUtf8("GPU：（lspci 未找到显示控制器）")
        : txt);
}

/* ═══════════ Sensors / Battery（hwmon 全表 + power_supply，每 10 tick） ═══════════ */

void IstatLogic::sampleSensors() {
    if (!m_ui) { return; }
    int row = 0;
    for (int h = 0; h < 12 && row < ISTM_MAXIF; h++) {
        QString dir = QString("/sys/class/hwmon/hwmon%1").arg(h);
        QString nm = IstmReadFile(dir + "/name");
        if (nm.isEmpty()) { continue; }
        for (int ch = 1; ch <= 16 && row < ISTM_MAXIF; ch++) {
            /* 温度通道（毫℃→℃），有 label 用 label，无则 tempN */
            QString tv = IstmReadFile(dir
                + QString("/temp%1_input").arg(ch));
            if (!tv.isEmpty()) {
                int mc = tv.toInt();
                if (mc > 0 && mc < 110000) {
                    QString lab = IstmReadFile(dir
                        + QString("/temp%1_label").arg(ch));
                    if (lab.isEmpty()) {
                        lab = QString("temp%1").arg(ch);
                    }
                    IstmSetCell(m_ui->tblSens, row, 0, nm);
                    IstmSetCell(m_ui->tblSens, row, 1, lab);
                    IstmSetCell(m_ui->tblSens, row, 2,
                        qFromUtf8("%1°C").arg(mc / 1000));
                    row++;
                }
                continue;
            }
            /* 电压通道（mV→V） */
            QString vv = IstmReadFile(dir
                + QString("/in%1_input").arg(ch));
            if (!vv.isEmpty()) {
                IstmSetCell(m_ui->tblSens, row, 0, nm);
                IstmSetCell(m_ui->tblSens, row, 1,
                    QString("in%1").arg(ch));
                IstmSetCell(m_ui->tblSens, row, 2,
                    qFromUtf8("%1 V").arg(
                        vv.toDouble() / 1000.0, 0, 'f', 3));
                row++;
                continue;
            }
            /* 电流通道 */
            QString cv = IstmReadFile(dir
                + QString("/curr%1_input").arg(ch));
            if (!cv.isEmpty()) {
                IstmSetCell(m_ui->tblSens, row, 0, nm);
                IstmSetCell(m_ui->tblSens, row, 1,
                    QString("curr%1").arg(ch));
                IstmSetCell(m_ui->tblSens, row, 2,
                    qFromUtf8("%1 mA").arg(cv.toInt()));
                row++;
            }
        }
        /* fan*_input：本机无 → 命中即行（保留通用性） */
        for (int ch = 1; ch <= 4 && row < ISTM_MAXIF; ch++) {
            QString fv = IstmReadFile(dir
                + QString("/fan%1_input").arg(ch));
            if (!fv.isEmpty()) {
                IstmSetCell(m_ui->tblSens, row, 0, nm);
                IstmSetCell(m_ui->tblSens, row, 1,
                    QString("fan%1").arg(ch));
                IstmSetCell(m_ui->tblSens, row, 2,
                    qFromUtf8("%1 RPM").arg(fv.toInt()));
                row++;
            }
        }
    }
    IstmSetRows(m_ui->tblSens, row > 0 ? row : 1);
    m_ui->lblSensNote->setText(qFromUtf8(
        "温度/电压/电流/风扇逐一 hwmon 扫描；额外电压轨、S.M.A.R.T. 需 root；本机无风扇"));
}

void IstatLogic::sampleBattery() {
    if (!m_ui) { return; }
    QString base = "/sys/class/power_supply/BAT1/";
    int cap = IstmReadFile(base + "capacity").toInt();
    if (cap < 0) { cap = 0; }
    if (cap > 100) { cap = 100; }
    long long now = (long long)IstmReadFile(
        base + "charge_now").toDouble();          /* μAh */
    long long full = (long long)IstmReadFile(
        base + "charge_full").toDouble();
    long long design = (long long)IstmReadFile(
        base + "charge_full_design").toDouble();
    long long vol = (long long)IstmReadFile(
        base + "voltage_now").toDouble();         /* μV */
    long long cur = (long long)IstmReadFile(
        base + "current_now").toDouble();         /* μA */
    QString status = IstmReadFile(base + "status");
    QString cycles = IstmReadFile(base + "cycle_count");
    QString tech = IstmReadFile(base + "technology");
    QString mfr = IstmReadFile(base + "manufacturer");
    QString model = IstmReadFile(base + "model_name");

    m_ui->lblBatBig->setText(qFromUtf8("%1%  ·  %2")
        .arg(cap).arg(status));
    m_ui->graphBat->addSample((double)cap / 100.0);

    double health = design > 0
        ? ((double)full / (double)design) * 100.0 : 0.0;
    double watts = ((double)vol * 1e-6) * ((double)cur * 1e-6);   /* μV·μA→W */
    QString wt = QString::number(watts, 'f', 1);
    m_ui->lblBatHealth->setText(qFromUtf8("健康 %1% · %2 W")
        .arg(health, 0, 'f', 1)
        .arg(wt));

    /* μAh→mAh（÷1000），再按 KB 口径 humanize（1 mAh ≈ 1 KB 量级显示） */
    m_ui->lblBatKv->setText(qFromUtf8(
        "剩余 %1 / %2 mAh（设计 %3 mAh）\n充电循环 %4 · %5 · %6 %7")
        .arg((double)now / 1000.0, 0, 'f', 0)
        .arg((double)full / 1000.0, 0, 'f', 0)
        .arg((double)design / 1000.0, 0, 'f', 0)
        .arg(cycles.isEmpty() ? QString("0") : cycles)
        .arg(tech.isEmpty() ? QString("—") : tech)
        .arg(mfr.isEmpty() ? QString("—") : mfr)
        .arg(model.isEmpty() ? QString("—") : model));

    /* AC 适配器 */
    QString acText = IstmReadFile(
        "/sys/class/power_supply/ACAD/online");
    m_ui->lblBatAc->setText(acText == "1"
        ? qFromUtf8("🔌 AC 已接（ACAD）")
        : qFromUtf8("🔌 未接 AC（ACAD 0）"));

    /* 顶条电池段（三态图标：⚡充电 / 🔌接AC未充 / 🔋释电） */
    if (m_ui->topBat) {
        QString icon = qFromUtf8("🔋");
        if (acText == "1") {
            if (status == "Charging") {
                icon = qFromUtf8("⚡");
            } else {
                icon = qFromUtf8("🔌");
            }
        }
        m_ui->topBat->setText(qFromUtf8("%1 %2%%3")
            .arg(icon)
            .arg(cap)
            .arg(acText == "1" ? " · AC" : ""));
    }
}

/* ═══════════ Notifications（阈值触发 + About 历史展示，无系统 tray） ═══════════ */
/* checkAlerts 每 10 tick 调用：连续计数单位=10s（30s=3 轮），冷却=30 轮（5min） */

static void IstmAlertPush(QString& hist, const QString& msg) {
    QString ln = qFromUtf8("[%1] %2")
        .arg(QTime::currentTime().toString("hh:mm:ss"))
        .arg(msg);
    hist = ln + "\n" + hist;                     /* 新的在前 */
    QStringList ls = IstmSplitLines(hist);
    if (ls.count() > 10) {
        QString h;
        for (int i = 0; i < 10; i++) {
            if (i > 0) { h += "\n"; }
            h += ls[i];
        }
        hist = h;
    }
}

void IstatLogic::checkAlerts() {
    if (!m_ui) { return; }

    /* CPU >90% 连续 3 轮（30s），冷却 30 轮 */
    if (m_cpuPct > 90) {
        m_alertCpuHi++;
    } else {
        m_alertCpuHi = 0;
    }
    if (m_alertCpuHi >= 3 && m_alertCoolCpu <= 0) {
        m_alertCoolCpu = 30;
        IstmAlertPush(m_alertHist,
            qFromUtf8("CPU 持续 >90%（30s）"));
    }

    /* 内存 >90% 连续 3 轮（30s），冷却 30 轮 */
    int memPct = m_memTotalKB > 0
        ? (int)(100 * m_memUsedKB / m_memTotalKB) : 0;
    if (memPct > 90) {
        m_alertMemHi++;
    } else {
        m_alertMemHi = 0;
    }
    if (m_alertMemHi >= 3 && m_alertCoolMem <= 0) {
        m_alertCoolMem = 30;
        IstmAlertPush(m_alertHist,
            qFromUtf8("内存 持续 >90%（30s）"));
    }

    /* 分区已用 >=90%（tblDisk 第 3 列百分比），冷却 30 轮 */
    for (int r = 0; r < m_diskRows; r++) {
        QString used;
#ifdef QT3_BUILD
        used = m_ui->tblDisk->text(r, 3);
#else
        QTableWidgetItem* it = m_ui->tblDisk->item(r, 3);
        if (!it) { continue; }
        used = it->text();
#endif
        used = IstmStrip(used);
        if (used.endsWith("%")) {
            int pc = used.left(used.length() - 1).toInt();
            if (pc >= 90 && m_alertCoolDisk <= 0) {
                m_alertCoolDisk = 30;
                IstmAlertPush(m_alertHist,
                    qFromUtf8("分区已用 %1%%（>=90%）").arg(pc));
            }
            if (m_alertCoolDisk > 0) { break; }
        }
    }

    /* 电量 <15 且未接 AC，冷却 30 轮 */
    if (m_alertCoolBat > 0) {
        m_alertCoolBat--;
    } else {
        int batPct = IstmReadFile(
            "/sys/class/power_supply/BAT1/capacity").toInt();
        bool ac = IstmReadFile(
            "/sys/class/power_supply/ACAD/online") == "1";
        if (batPct >= 0 && batPct < 15 && !ac) {
            IstmAlertPush(m_alertHist,
                qFromUtf8("电量 %1%（<15%）且未接 AC").arg(batPct));
        }
    }

    if (m_alertCoolCpu > 0) { m_alertCoolCpu--; }
    if (m_alertCoolMem > 0) { m_alertCoolMem--; }
    if (m_alertCoolDisk > 0) { m_alertCoolDisk--; }

    /* Step7：网络合计 >5MB/s 连续 3 轮（30s），冷却 30 轮 */
    if (m_netSumRx + m_netSumTx > 5.0 * 1024.0 * 1024.0) {
        m_alertNetHi++;
    } else {
        m_alertNetHi = 0;
    }
    if (m_alertNetHi >= 3 && m_alertCoolNet <= 0) {
        m_alertCoolNet = 30;
        IstmAlertPush(m_alertHist,
            qFromUtf8("网络流量 >5MB/s（30s），瞬时 %1")
            .arg(IstmHumanRate(m_netSumRx + m_netSumTx)));
    }
    if (m_alertCoolNet > 0) { m_alertCoolNet--; }

    fillAbout();
}

void IstatLogic::fillAbout() {
    if (!m_ui || !m_ui->lblAbout) { return; }
    QString t = qFromUtf8(
        "iStat Menus 1.0.0 (Linux)\n"
        "对标 macOS iStat Menus 6.50 落地子集\n"
        "CPU / Memory / Disk / Network / Sensors / Battery / Time + 顶条快照\n"
        "S.M.A.R.T./SPD/额外电压轨需 root 未实现；本机无风扇；天气未接（官方付费源+本机公网受限）\n");
    t += m_alertHist.isEmpty() ? QString("\n最近提醒：（无）") :
        (qFromUtf8("\n最近提醒（阈值：CPU/内存>90 连续 30s · 分区>=90% · "
            "电量<15% 未接 AC · 网络>5MB/s 30s；5min 冷却）\n") + m_alertHist);
    m_ui->lblAbout->setText(t);
    if (m_ui->lblWorld) {
        m_ui->lblWorld->setText(qFromUtf8(
            "世界钟（固定 UTC offset，无 DST）\n") + IstmWorldClocks());
    }
}

/* ═══════════ Top 进程（每 10 tick；ps pcpu,pmem,comm 冒泡取前 5） ═══════════ */

void IstatLogic::fillTopTables() {
    if (!m_ui) { return; }
    QStringList a;
    a << "-eo" << "pcpu,pmem,comm" << "--no-headers";
    QString out = IstmRun("ps", a);

    double sPcpu[5]; double sPmem[5]; QString sName[5];
    for (int i = 0; i < 5; i++) {
        sPcpu[i] = -1.0;
        sPmem[i] = 0.0;
        sName[i] = QString();
    }
    QStringList lines = IstmSplitLines(out);
    for (int i = 0; i < (int)lines.count(); i++) {
        QStringList tk = IstmTokens(lines[i]);
        if (tk.count() < 3) { continue; }
        double cpu = tk[0].toDouble();
        double mem = tk[1].toDouble();
        int slot = 5;                        /* 插入位：第一个比它小的 */
        for (int r = 0; r < 5; r++) {
            if (cpu > sPcpu[r]) { slot = r; break; }
        }
        if (slot >= 5) { continue; }
        for (int r = 4; r > slot; r--) {     /* 后移让位 */
            sPcpu[r] = sPcpu[r - 1];
            sPmem[r] = sPmem[r - 1];
            sName[r] = sName[r - 1];
        }
        sPcpu[slot] = cpu;
        sPmem[slot] = mem;
        /* 进程名 = 第 3 个 token 起拼接（可能含空格的 comm 被分词，重新拼） */
        QString nm;
        for (int t = 2; t < (int)tk.count(); t++) {
            if (t > 2) { nm += " "; }
            nm += tk[t];
        }
        sName[slot] = IstmStrip(nm);
    }

    for (int r = 0; r < 5; r++) {
        QString nm = sName[r].isEmpty() ? QString("—") : sName[r];
        QString cp = sPcpu[r] >= 0
            ? QString::number(sPcpu[r], 'f', 1) + "%" : QString("—");
        QString mp = sPcpu[r] >= 0
            ? QString::number(sPmem[r], 'f', 1) + "%" : QString("—");
        IstmSetCell(m_ui->tblTopCpu, r, 0, nm);
        IstmSetCell(m_ui->tblTopCpu, r, 1, cp);
        IstmSetCell(m_ui->tblTopCpu, r, 2, mp);
        IstmSetCell(m_ui->tblTopMem, r, 0, nm);
        IstmSetCell(m_ui->tblTopMem, r, 1, cp);
        IstmSetCell(m_ui->tblTopMem, r, 2, mp);
    }
}

/* ═══════════ 顶条 ═══════════ */

void IstatLogic::sampleTemp() {
    if (!m_ui || !m_ui->topTemp) { return; }
    int t = IstmPkgTemp();
    m_ui->topTemp->setText(t >= 0
        ? qFromUtf8("%1°C").arg(t) : qFromUtf8("—"));
}

void IstatLogic::updateTopbar() {
    if (!m_ui) { return; }
    if (m_ui->topCpu) {
        m_ui->topCpu->setText(qFromUtf8("CPU %1%").arg(m_cpuPct));
    }
    /* topMem 已在 sampleMem 内置；topNet/topBat 已由 sampleNet/sampleBattery */
    sampleTemp();
    IstmTopClockText(m_ui->topClock);
}

/* ═══════════ 节拍 ═══════════ */

void IstatLogic::onTick() {
    m_tick++;
    sampleCpu();
    sampleMem();
    sampleNet();
    sampleDisk();
    updateTopbar();
    if (m_tick == 1 || (m_tick % 10) == 0) {
        fillTopTables();
    }
    if (m_tick == 2 || (m_tick % 10) == 2) {   /* 与 Top 错峰 */
        fillNetStatic();
    }
    if (m_tick == 3 || (m_tick % 10) == 3) {   /* 与 Net 错峰 */
        fillDiskStatic();
    }
    if (m_tick == 4 || (m_tick % 10) == 4) {   /* 与 Disk 错峰 */
        sampleSensors();
        sampleBattery();
        checkAlerts();
    }
    /* 公网 IP 查询延迟到 tick 30（界面已出、数据走 30s 之后）：
       IstmRun 是同步 fork/execve，主线程 curl 不通时 -m 5×2 最多吃 10s，
       放 tick==1 会导致启动即完全卡死且首帧数据全空。 */
    if ((m_tick % 120) == 30) {
        fetchPubIp();
    }
}