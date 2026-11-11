#include "compat34.h"
#include "qprocess34.h"
#include "compatcore34.h"
#include "logic.h"
#include "cJSON.h"
#include "qsettings34.h"
#include "v2flag.h"
#ifdef QT3_BUILD
#include <qpainter.h>
#include <qpen.h>
#include <qcolor.h>
#include <qimage.h>
#else
#include <QPainter>
#include <QPen>
#include <QColor>
#include <QImage>
#include <QIcon>
#include <QTableWidgetItem>
#endif
#ifndef QT3_BUILD
#include <QHeaderView>
#endif

#include <qdir.h>
#include <qfile.h>
#include <qfileinfo.h>
#include <qlineedit.h>
#include <qlabel.h>
#include <qtextedit.h>
#include <qpushbutton.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <qprocess.h>
#include <qtimer.h>
#include <qcheckbox.h>
#include <qcombobox.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#ifdef QT3_BUILD
#include <qtextstream.h>
#else
#include <QTextStream>
#endif

/* ── 固定版本号（锁定，不可升级） ── */
#define V2CORE_VERSION      "v5.35.0"
#define V2CORE_GH_PREFIX    "https://gh-proxy.com/https://github.com/v2fly/v2ray-core/releases/download/v5.35.0/"
#define V2CORE_GH_DIRECT    "https://github.com/v2fly/v2ray-core/releases/download/v5.35.0/"
#define XCORE_VERSION       "v26.11.9"
#define XCORE_GH_PREFIX     "https://gh-proxy.com/https://github.com/XTLS/Xray-core/releases/download/v26.11.9/"
#define XCORE_GH_DIRECT     "https://github.com/XTLS/Xray-core/releases/download/v26.11.9/"

/* ── Qt3/Qt4 小工具 ── */
static int sFind(const QString& s, char c, int from) {
#ifdef QT3_BUILD
    return s.find(c, from);
#else
    return s.indexOf(QChar(c), from);
#endif
}
static int sFindStr(const QString& s, const char* sub, int from) {
#ifdef QT3_BUILD
    return s.find(sub, from);
#else
    return s.indexOf(QString(sub), from);
#endif
}
static int sFindQ(const QString& s, const QString& sub, int from) {
#ifdef QT3_BUILD
    return s.find(sub, from);
#else
    return s.indexOf(sub, from);
#endif
}
static int sRFind(const QString& s, char c, int from) {
#ifdef QT3_BUILD
    return s.findRev(c, from);
#else
    return s.lastIndexOf(QChar(c), from);
#endif
}
static QStringList splitLines(const QString& content) {
#ifdef QT3_BUILD
    return QStringList::split('\n', content);
#else
    return content.split(QChar('\n'));
#endif
}

/* C：按目标架构选择内核包名 */
static QString v2ZipName(CoreKind kind) {
    if (kind == CoreXray) {
#if defined(__aarch64__)
        return QString("Xray-linux-arm64-v8a.zip");
#elif defined(__arm__)
        return QString("Xray-linux-arm32-v7a.zip");
#elif defined(__i386__)
        return QString("Xray-linux-32.zip");
#else
        return QString("Xray-linux-64.zip");
#endif
    }
#if defined(__aarch64__)
    return QString("v2ray-linux-arm64-v8a.zip");
#elif defined(__arm__)
    return QString("v2ray-linux-arm32-v7a.zip");
#elif defined(__i386__)
    return QString("v2ray-linux-32.zip");
#else
    return QString("v2ray-linux-64.zip");
#endif
}

/* Qt3/Qt4 下拉框索引差异 */
static int v2ComboCurrent(QComboBox* c) {
#ifdef QT3_BUILD
    return c ? c->currentItem() : -1;
#else
    return c ? c->currentIndex() : -1;
#endif
}
static void v2ComboSetCurrent(QComboBox* c, int i) {
    if (!c || i < 0) { return; }
#ifdef QT3_BUILD
    c->setCurrentItem(i);
#else
    c->setCurrentIndex(i);
#endif
}
/* 统计文本文件行数（access.log 请求数用） */
static long long v2CountLines(const QString& path) {
    QFile f(path);
#ifdef QT3_BUILD
    if (!f.open(IO_ReadOnly)) { return 0; }
#else
    if (!f.open(QIODevice::ReadOnly)) { return 0; }
#endif
    long long n = 0;
    QTextStream ts(&f);
    while (!ts.atEnd()) {
        ts.readLine();
        ++n;
    }
    f.close();
    return n;
}

/* 字节/速率格式化（流量显示用） */
static QString v2Bytes(long long b) {
    if (b < 0) { return QString("0 B"); }
    if (b < 1024) { return QString::number((long)b) + " B"; }
    if (b < 1024 * 1024) {
        return QString::number(b / 1024.0, 'f', 1) + " KB";
    }
    if (b < 1024LL * 1024 * 1024) {
        return QString::number(b / 1048576.0, 'f', 1) + " MB";
    }
    return QString::number(b / 1073741824.0, 'f', 2) + " GB";
}

static QString v2BytesRate(long long bytesPerSec) {
    return v2Bytes(bytesPerSec) + "/s";
}

static bool v2LatBetter(int a, int b);   /* 定义在测速段，此处前置声明 */

/* 完整日期时间戳（测试结果标注用） */
static QString v2NowStamp() {
#ifdef QT3_BUILD
    return QDate::currentDate().toString("yyyy-MM-dd") + " "
         + QTime::currentTime().toString("hh:mm:ss");
#else
    return QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
#endif
}

static void v2TableInit(V2Table* t, const QStringList& cols) {
#ifdef QT3_BUILD
    t->setNumCols((int)cols.count());
    t->setColumnLabels(cols);
    t->setReadOnly(true);
    t->setSelectionMode(QTable::SingleRow);
#else
    t->setColumnCount((int)cols.count());
    t->setHorizontalHeaderLabels(cols);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
#endif
}
static void v2TableClear(V2Table* t) {
#ifdef QT3_BUILD
    t->setNumRows(0);
#else
    t->setRowCount(0);
#endif
}
static void v2TableSetRows(V2Table* t, int n) {
#ifdef QT3_BUILD
    t->setNumRows(n);
#else
    t->setRowCount(n);
#endif
}
static void v2TableSetCell(V2Table* t, int r, int c, const QString& s) {
    if (!t) { return; }
#ifdef QT3_BUILD
    t->setText(r, c, s);
#else
    QTableWidgetItem* it = t->item(r, c);
    if (!it) { t->setItem(r, c, new QTableWidgetItem(s)); }
    else { it->setText(s); }
#endif
}
static int v2TableCols(V2Table* t) {
#ifdef QT3_BUILD
    return t ? t->numCols() : 0;
#else
    return t ? t->columnCount() : 0;
#endif
}
static int v2TableRow(V2Table* t) {
    return t ? t->currentRow() : -1;
}
static void v2TableSetRow(V2Table* t, int r) {
    if (t && r >= 0) { t->setCurrentCell(r, 0); }
}

/* 去重：同 proto+addr+port+凭据 视为同一节点；ss 用原始链接比对 */
static bool nodeExists(V2Node* arr, int n, const V2Node& x) {
    for (int i = 0; i < n; ++i) {
        if (x.proto == "ss") {
            if (arr[i].proto == "ss" && arr[i].raw == x.raw) { return true; }
            continue;
        }
        if (arr[i].proto == x.proto && arr[i].addr == x.addr
            && arr[i].port == x.port && arr[i].uuid == x.uuid) {
            return true;
        }
    }
    return false;
}

/* 去重：同 proto+addr+port+凭据 视为同一节点；ss 用原始链接比对 */
static bool hasShareScheme(const QString& s);   /* 前向声明：onSubFetched 先用 */

/* URL 解码：把 %XX 还原为字节后按 UTF-8 组装（节点名常为 %E7%AB%8B...） */
static QString v2UrlDecode(const QString& in, bool plusAsSpace) {
    QByteArray src = qToUtf8(in);
    QByteArray out;
    int n = 0;
    for (int i = 0; i < (int)src.size(); ++i) {
        char c = src.at(i);
        if (c == '%' && i + 2 < (int)src.size()) {
            char h1 = src.at(i + 1);
            char h2 = src.at(i + 2);
            int hi = (h1 >= '0' && h1 <= '9') ? h1 - '0'
                : (h1 >= 'a' && h1 <= 'f') ? h1 - 'a' + 10
                : (h1 >= 'A' && h1 <= 'F') ? h1 - 'A' + 10 : -1;
            int lo = (h2 >= '0' && h2 <= '9') ? h2 - '0'
                : (h2 >= 'a' && h2 <= 'f') ? h2 - 'a' + 10
                : (h2 >= 'A' && h2 <= 'F') ? h2 - 'A' + 10 : -1;
            if (hi >= 0 && lo >= 0) {
                out.resize(n + 1);
                out[n++] = (char)((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        if (c == '+' && plusAsSpace) { c = ' '; }
        out.resize(n + 1);
        out[n++] = c;
    }
    return qFromUtf8(out.data(), n);
}

static void writeUtf8File(const QString& path, const QString& content) {
#ifdef QT3_BUILD
    QByteArray u = content.utf8();
    /* Qt3 QByteArray::size() 含尾 NUL：按 strlen 精确写入，避免文件尾多出 \0 */
    const size_t n = u.data() ? strlen(u.data()) : 0;
#else
    QByteArray u = content.toUtf8();
    const size_t n = (size_t)u.size();
#endif
    FILE* fp = fopen(QFile::encodeName(path).data(), "wb");
    if (!fp) { return; }
    fwrite(u.data(), 1, n, fp);
    fclose(fp);
}
static QString readUtf8File(const QString& path) {
    QString s;
    FILE* fp = fopen(QFile::encodeName(path).data(), "rb");
    if (!fp) { return s; }
    char buf[8192];
    QByteArray arr;
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        int old = (int)arr.size();
        arr.resize(old + (int)n);
        memcpy(arr.data() + old, buf, n);
    }
    fclose(fp);
    if ((int)arr.size() > 0) {
        s = qFromUtf8(arr.data(), (int)arr.size());
    }
    return s;
}
static bool fileExists(const QString& path) {
    return QFile::exists(path);
}

static void baAppend(QByteArray& dst, const QByteArray& src) {
    if (src.size() <= 0) { return; }
    int old = (int)dst.size();
    dst.resize(old + (int)src.size());
    memcpy(dst.data() + old, src.data(), (size_t)src.size());
}

/* 取 QByteArray 尾部（最多 max 字节）转 QString */
static QString tailStr(const QByteArray& arr, int max) {
    int from = (int)arr.size() > max ? (int)arr.size() - max : 0;
    int len = (int)arr.size() - from;
    if (len <= 0) { return QString(); }
    return qFromUtf8(arr.data() + from, len);
}

/* 在 s 中找最后一次出现 needle 的那一行（含 needle） */
static QString lastNeedleLine(const QString& s, const char* needle) {
    int pos = -1;
    int from = 0;
    while (true) {
        int p = sFindStr(s, needle, from);
        if (p < 0) { break; }
        pos = p;
        from = p + 1;
    }
    if (pos < 0) { return QString(); }
    int end = sFind(s, '\n', pos);
    if (end < 0) { end = (int)s.length(); }
#ifdef QT3_BUILD
    QString line = s.mid(pos, end - pos).stripWhiteSpace();
#else
    QString line = s.mid(pos, end - pos).trimmed();
#endif
    return line;
}

/* 取最后一个"N.N%"样式的百分比（curl -# 进度条），无则 -1 */
static int extractLastPercent(const QString& s) {
    for (int i = (int)s.length() - 1; i >= 0; --i) {
        if (s[i] != '%') { continue; }
        int j = i - 1;
        char buf[16];
        int n = 0;
        while (j >= 0 && n < 7) {
#ifdef QT3_BUILD
            char c = s[j].latin1();
#else
            char c = s[j].toLatin1();
#endif
            if ((c >= '0' && c <= '9') || c == '.') {
                buf[n++] = c;
                --j;
            } else {
                break;
            }
        }
        if (n <= 0) { return -1; }
        char rev[16];
        for (int k = 0; k < n; ++k) { rev[k] = buf[n - 1 - k]; }
        rev[n] = '\0';
        bool ok = false;
        double d = QString(rev).toDouble(&ok);
        if (!ok) { return -1; }
        int pct = (int)d;
        if (pct >= 0 && pct <= 100) { return pct; }
        return -1;
    }
    return -1;
}

/* ───────────────────────────────────────────── */

V2RayLogic::V2RayLogic(QObject* parent)
    : QObject(parent) {
    nodes.clear();
    subProc = 0;
    dlProc = 0;
    unzipProc = 0;
    coreProc = 0;
    latProc = 0;
    testCoreProc = 0;
    testCurlProc = 0;
    coreDlBusy = false;
    latBusy = false;
    latGotConnected = false;
    curLatIndex = -1;
    testIsReal = false;
    testCancel = false;
    testDone = 0;
    testTotal = 0;
    testCoreWaitTries = 0;
    testPort = 0;
    testUrl = "https://www.gstatic.com/generate_204";
    subEdit = 0;
    subList = 0;
    nodeList = 0;
    coreState = 0;
    coreTestSocksIcon = 0;
    coreTestSocksText = 0;
    coreTestHttpIcon = 0;
    coreTestHttpText = 0;
    coreTestMsg = 0;
    coreTestTime = 0;
    lastTestSocksKind = 2;
    lastTestHttpKind = 2;
    trafficLabel = 0;
    trafficTimer = 0;
    statsProc = 0;
    apiPort = 0;
    trafficUpBase = trafficDownBase = 0;
    trafficUpLast = trafficDownLast = 0;
    trafficUpRate = trafficDownRate = 0;
    reqCountBase = reqCountLast = 0;
    totalUpBase = totalDownBase = 0;
    totalUp = totalDown = 0;
    totalFlushTick = 0;
    favList = 0;
    nodeFilterEdit = 0;
    nodeSortBox = 0;
    activeNodeLabel = 0;
    log = 0;
    startBtn = 0;
    stopBtn = 0;
    bypassCnCheck = 0;
    bypassLanCheck = 0;
    listenAddrBox = 0;
    listenPortEdit = 0;
    listenSocksEdit = 0;
    listenHttpEdit = 0;
    probeProc = 0;
    probeStage = 0;
    updBtn = 0;
    updAllBtn = 0;
    nodeStatus = 0;
    coreDlBtn = 0;
    coreSelBox = 0;
    redetectBtn = 0;
    detectProc = 0;
    coreKind = CoreV2ray;
    coreSourceSystem = false;
    dlCoreKind = 0;
    geoProc = 0;
    geoBusy = false;
    geoCurIdx = -1;
    geoToolOk = false;
    testTcpSelBtn = 0;
    testRealSelBtn = 0;
    testTcpAllBtn = 0;
    testRealAllBtn = 0;
    testStopBtn = 0;
    coreStopRequested = false;
    appQuitting = false;
    dlTry = 0;
    autoUpdateIntervalMin = 120;
    autoUpdateEnabled = true;
    autoUpdateChk = 0;
    autoUpdateFreqBox = 0;
    pendingSubIndex = 0;
    updatingAll = false;
    dlProgressPct = -1;
    ensureDirs();
    loadSubscriptions();
    loadSubMeta();
    loadCoreSettings();
    loadGeoCache();
    loadNodes();
    if (subList) { refreshSubList(); }
    detectSystemXray();
    refreshCoreCombo();
    QTimer::singleShot(300, this, SLOT(onLookupCountryAll()));
    /* 订阅定时自动更新（默认开启、每 120 分钟，串行执行） */
    {
        QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
        autoUpdateEnabled = st.valueInt("sub/autoUpdate", 1) != 0;
        autoUpdateIntervalMin = st.valueInt("sub/autoUpdateMin", 120);
        if (autoUpdateIntervalMin < 5) { autoUpdateIntervalMin = 120; }
    }
    autoUpdateTimer = new QTimer(this);
    connect(autoUpdateTimer, SIGNAL(timeout()), this, SLOT(onAutoUpdateTick()));
    if (autoUpdateEnabled) {
        autoUpdateTimer->start(autoUpdateIntervalMin * 60 * 1000);
    }
    /* B：退出前落盘（此时主窗/控件仍存活；析构里不能再碰兄弟控件） */
    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(onSaveSettings()));
    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(onSaveAllOnQuit()));
    /* 退出时统一终止所有子进程，避免残留 xray/v2ray/curl */
    connect(qApp, SIGNAL(aboutToQuit()), this, SLOT(shutdownProcesses()));
}

void V2RayLogic::setWidgets(QLineEdit* subEdit_, V2Table* subList_, V2Table* nodeList_,
                            QLabel* coreState_, QTextEdit* log_,
                            QPushButton* startBtn_, QPushButton* stopBtn_) {
    subEdit = subEdit_;
    subList = subList_;
    nodeList = nodeList_;
    coreState = coreState_;
    log = log_;
    startBtn = startBtn_;
    stopBtn = stopBtn_;
    refreshSubList();
    refreshNodeList();
    /* 恢复上次选中节点（activeNode） */
    {
        QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
        QString active = st.valueStr("activeNode", "");
        int ai = indexByNodeId(active);
        if (ai < 0 && nodes.count() > 0) { ai = 0; }
        if (ai >= 0 && nodeList) { v2TableSetRow(nodeList, ai); }
    }
    refreshCoreState();
    appendLog(qFromUtf8("V2Ray 订阅管理器已就绪（核心版本固定 ") + qFromUtf8(V2CORE_VERSION) + qFromUtf8("）"));
}

QString V2RayLogic::baseDir() const {
#ifdef QT3_BUILD
    QString home = QDir::homeDirPath();
#else
    QString home = QDir::homePath();
#endif
    return home + "/.config/v2ray_suber";
}
QString V2RayLogic::coreDir() const {
    return baseDir() + "/core";
}
QString V2RayLogic::xrayDir() const {
    return coreDir() + "/xray";
}
QString V2RayLogic::coreWorkDir() const {
    if (coreKind == CoreXray) { return xrayDir(); }
    return coreDir();
}
QString V2RayLogic::coreConfigPath() const {
    return coreWorkDir() + "/config_run.json";
}

/* geo 文件实际所在目录：xrayDir → coreDir 依次探测 */
QString V2RayLogic::geoAssetDir() const {
    if (fileExists(xrayDir() + "/geoip.dat")
        && fileExists(xrayDir() + "/geosite.dat")) { return xrayDir(); }
    if (fileExists(coreDir() + "/geoip.dat")
        && fileExists(coreDir() + "/geosite.dat")) { return coreDir(); }
    return QString();
}
QString V2RayLogic::coreBinaryPath() const {
    if (coreKind == CoreV2ray) { return coreDir() + "/v2ray"; }
    if (coreSourceSystem && !systemXrayPath.isEmpty()) { return systemXrayPath; }
    return xrayDir() + "/xray";
}
QString V2RayLogic::zipPath() const {
    if (coreKind == CoreXray) { return baseDir() + "/xray-core.zip"; }
    return baseDir() + "/v2ray-core.zip";
}
void V2RayLogic::ensureDirs() {
    /* Qt3 QDir 无 mkpath，用 POSIX mkdir 逐级创建（路径为 ASCII，可安全 utf8 解码） */
    QString home;
#ifdef QT3_BUILD
    home = QDir::homeDirPath();
#else
    home = QDir::homePath();
#endif
#ifdef QT3_BUILD
    mkdir(QString(home + "/.config").utf8().data(), 0755);
    mkdir(baseDir().utf8().data(), 0755);
    mkdir(coreDir().utf8().data(), 0755);
    mkdir(xrayDir().utf8().data(), 0755);
#else
    mkdir(qToUtf8(QString(home + "/.config")).data(), 0755);
    mkdir(qToUtf8(baseDir()).data(), 0755);
    mkdir(qToUtf8(coreDir()).data(), 0755);
    mkdir(qToUtf8(xrayDir()).data(), 0755);
#endif
}

bool V2RayLogic::coreInstalled() const {
    if (!fileExists(coreBinaryPath())) { return false; }
    if (access(QFile::encodeName(coreBinaryPath()).data(), X_OK) != 0) { return false; }
    return true;
}

bool V2RayLogic::hasGeoFiles() const {
    return !geoAssetDir().isEmpty();
}

/* ── 订阅列表管理 ── */
void V2RayLogic::loadSubscriptions() {
    subs.clear();
    QString data = readUtf8File(baseDir() + "/subscriptions.txt");
    if (data.isEmpty()) { return; }
    QStringList lines = splitLines(data);
    for (int i = 0; i < (int)lines.count(); ++i) {
#ifdef QT3_BUILD
        QString u = lines[i].stripWhiteSpace();
#else
        QString u = lines[i].trimmed();
#endif
        if (u.isEmpty()) { continue; }
        subs.append(u);
    }
}
void V2RayLogic::saveSubscriptions() {
    QString all;
    for (int i = 0; i < (int)subs.count(); ++i) {
        all += subs[i] + "\n";
    }
    writeUtf8File(baseDir() + "/subscriptions.txt", all);
}
void V2RayLogic::saveSubLast(const QString& content) {
    writeUtf8File(baseDir() + "/sub_last.txt", content);
}

void V2RayLogic::refreshSubList() {
    if (!subList) { return; }
    if (v2TableCols(subList) == 0) {
        QStringList cols;
        cols << qFromUtf8("订阅地址") << qFromUtf8("上次更新") << qFromUtf8("节点数");
        v2TableInit(subList, cols);
#ifdef QT3_BUILD
        subList->setColumnWidth(0, 520);
        subList->setColumnStretchable(0, true);   /* URL 列动态占满剩余宽度 */
        subList->setColumnWidth(1, 84);
        subList->setColumnWidth(2, 60);
#else
        subList->horizontalHeader()->setStretchLastSection(false);
        subList->horizontalHeader()->setResizeMode(0, QHeaderView::Stretch);
        subList->setColumnWidth(1, 84);
        subList->setColumnWidth(2, 60);
#endif
    }
    v2TableClear(subList);
    v2TableSetRows(subList, (int)subs.count());
    for (int i = 0; i < (int)subs.count(); ++i) {
        QString meta = subMeta.contains(subs[i]) ? subMeta[subs[i]] : QString();
        QString t;
        QString n = qFromUtf8("—");
        if (!meta.isEmpty()) {
            t = meta;
            int bar = sFindStr(meta, "|", 0);
            if (bar > 0) {
                t = meta.left(bar);
                n = meta.mid(bar + 1);
            }
        } else {
            /* 后备：sub_meta 丢失时由 nodes.json 反推（节点数 + 最近更新时间） */
            int cnt = 0;
            QString last;
            for (int k = 0; k < (int)nodes.count(); ++k) {
                if (nodes[k].sourceSub != subs[i]) { continue; }
                ++cnt;
                if (last.isEmpty() || nodes[k].updatedAt > last) {
                    last = nodes[k].updatedAt;
                }
            }
            if (cnt > 0) {
                if (last.length() >= 19) { last = last.mid(11); }
                t = last;
                n = QString::number(cnt);
                subMeta.insert(subs[i], t + "|" + n);   /* 自愈：写回 meta */
            }
        }
        v2TableSetCell(subList, i, 0, subs[i]);
        v2TableSetCell(subList, i, 1, t.isEmpty() ? qFromUtf8("—") : t);
        v2TableSetCell(subList, i, 2, n);
    }
}
#ifdef QT3_BUILD
/* 国旗 + 文本 的自绘单元格（参考 qlcomp/emojiitems.h 的 EmojiTableItem） */
class V2GeoItem : public QTableItem {
public:
    V2GeoItem(QTable* t, const QString& text, const QPixmap& flag)
        : QTableItem(t, QTableItem::Never, text), m_flag(flag) {}
    void paint(QPainter* p, const QColorGroup& cg, const QRect& cr, bool selected) {
        p->fillRect(cr, selected ? cg.brush(QColorGroup::Highlight)
                                 : cg.brush(QColorGroup::Base));
        int x = cr.x() + 2;
        if (!m_flag.isNull()) {
            p->drawPixmap(x, cr.y() + (cr.height() - m_flag.height()) / 2, m_flag);
            x += m_flag.width() + 4;
        }
        p->setPen(selected ? cg.highlightedText() : cg.text());
        p->drawText(QRect(x, cr.y(), cr.width() - (x - cr.x()), cr.height()),
                    Qt::AlignVCenter | Qt::AlignLeft, text());
    }
private:
    QPixmap m_flag;
};
#endif

/* 直接在 QImage 上落像素（Qt3 对 alpha QPixmap 用 QPainter 绘制在部分环境不生效） */
static void v2PlotPixel(QImage& img, int x, int y, QRgb c, int thick) {
    for (int dy = 0; dy < thick; ++dy) {
        for (int dx = 0; dx < thick; ++dx) {
            int px = x + dx;
            int py = y + dy;
            if (px >= 0 && px < img.width() && py >= 0 && py < img.height()) {
                img.setPixel(px, py, c);
            }
        }
    }
}

static void v2DrawLine(QImage& img, int x0, int y0, int x1, int y1,
                       QRgb c, int thick) {
    int dx = x1 - x0;
    int dy = y1 - y0;
    int adx = dx < 0 ? -dx : dx;
    int ady = dy < 0 ? -dy : dy;
    int steps = adx > ady ? adx : ady;
    if (steps == 0) {
        v2PlotPixel(img, x0, y0, c, thick);
        return;
    }
    for (int i = 0; i <= steps; ++i) {
        int x = x0 + dx * i / steps;
        int y = y0 + dy * i / steps;
        v2PlotPixel(img, x, y, c, thick);
    }
}

/* 可用性图标：0=绿勾 1=红叉 2=灰线 */
static QPixmap v2StatusIcon(int kind) {
#ifdef QT3_BUILD
    QImage img(14, 14, 32);
    img.setAlphaBuffer(true);
#else
    QImage img(14, 14, QImage::Format_ARGB32);
#endif
    img.fill(0);
    QColor col(140, 140, 140);
    int thick = 1;
    if (kind == 0) {
        col = QColor(30, 150, 60);
        thick = 2;
    } else if (kind == 1) {
        col = QColor(200, 40, 40);
        thick = 2;
    }
    QRgb c = qRgba(col.red(), col.green(), col.blue(), 255);
    if (kind == 0) {
        v2DrawLine(img, 2, 7, 6, 11, c, thick);
        v2DrawLine(img, 6, 11, 12, 3, c, thick);
    } else if (kind == 1) {
        v2DrawLine(img, 3, 3, 11, 11, c, thick);
        v2DrawLine(img, 11, 3, 3, 11, c, thick);
    } else {
        v2DrawLine(img, 3, 7, 11, 7, c, thick);
    }
    QPixmap pm;
    pm.convertFromImage(img);
    return pm;
}

void V2RayLogic::refreshNodeList() {
    if (!nodeList) { return; }
    if (v2TableCols(nodeList) == 0) {
        /* 列顺序：可用/状态 紧跟端口，测速列前置；国家列隐藏（国旗 tooltip 显示） */
        QStringList cols;
        cols << qFromUtf8("名称") << qFromUtf8("协议") << qFromUtf8("国旗")
             << qFromUtf8("地址") << qFromUtf8("端口")
             << qFromUtf8("可用") << qFromUtf8("状态")
             << qFromUtf8("真连(ms)") << "TCP(ms)"
             << qFromUtf8("传输") << qFromUtf8("安全");
        v2TableInit(nodeList, cols);
#ifdef QT3_BUILD
        nodeList->setColumnWidth(0, 200);
        nodeList->setColumnWidth(1, 60);
        nodeList->setColumnWidth(2, 40);
        nodeList->setColumnWidth(3, 170);
        nodeList->setColumnWidth(4, 60);
        nodeList->setColumnWidth(5, 48);
        nodeList->setColumnWidth(6, 70);
        nodeList->setColumnWidth(7, 80);
        nodeList->setColumnWidth(8, 70);
        nodeList->setColumnWidth(9, 60);
        nodeList->setColumnWidth(10, 70);
#else
        nodeList->horizontalHeader()->setStretchLastSection(false);
        nodeList->setColumnWidth(0, 200);
        nodeList->setColumnWidth(1, 60);
        nodeList->setColumnWidth(2, 40);
        nodeList->setColumnWidth(3, 170);
        nodeList->setColumnWidth(4, 60);
        nodeList->setColumnWidth(5, 48);
        nodeList->setColumnWidth(6, 70);
        nodeList->setColumnWidth(7, 80);
        nodeList->setColumnWidth(8, 70);
        nodeList->setColumnWidth(9, 60);
        nodeList->setColumnWidth(10, 70);
#endif
    }
    /* 视图层过滤 + 排序（不动 nodes 主数据/activeNode） */
    V2IndexList view;
    for (int i = 0; i < (int)nodes.count(); ++i) { view.append(i); }
    if (nodeFilterEdit) {
        QString f = nodeFilterEdit->text();
#ifdef QT3_BUILD
        f = f.stripWhiteSpace();
#else
        f = f.trimmed();
#endif
        if (!f.isEmpty()) {
            V2IndexList fv;
            for (int i = 0; i < (int)nodes.count(); ++i) {
                const V2Node& n = nodes[i];
                bool hit = false;
#ifdef QT3_BUILD
                hit = hit || n.name.find(f, false) >= 0;
                hit = hit || n.country.find(f, false) >= 0;
                hit = hit || n.countryName.find(f, false) >= 0;
                hit = hit || n.addr.find(f, false) >= 0;
                hit = hit || n.proto.find(f, false) >= 0;
#else
                hit = hit || n.name.indexOf(f, 0, Qt::CaseInsensitive) >= 0;
                hit = hit || n.country.indexOf(f, 0, Qt::CaseInsensitive) >= 0;
                hit = hit || n.countryName.indexOf(f, 0, Qt::CaseInsensitive) >= 0;
                hit = hit || n.addr.indexOf(f, 0, Qt::CaseInsensitive) >= 0;
                hit = hit || n.proto.indexOf(f, 0, Qt::CaseInsensitive) >= 0;
#endif
                if (hit) { fv.append(i); }
            }
            view = fv;
        }
    }
    if (nodeSortBox) {
        int mode = -1;
#ifdef QT3_BUILD
        mode = nodeSortBox->currentItem();
#else
        mode = nodeSortBox->currentIndex();
#endif
        if (mode == 1 || mode == 2) {
            /* 按延时稳定排序（未测/失败按 v2LatBetter 沉底），插入排序 */
            for (int k = 1; k < (int)view.count(); ++k) {
                int key = view[k];
                int j = k - 1;
                while (j >= 0) {
                    const V2Node& a = nodes[key];
                    const V2Node& b = nodes[view[j]];
                    int ma = (mode == 1) ? a.realMs : a.tcpMs;
                    int mb = (mode == 1) ? b.realMs : b.tcpMs;
                    if (v2LatBetter(ma, mb)) {
                        view[j + 1] = view[j];
                        --j;
                    } else {
                        break;
                    }
                }
                view[j + 1] = key;
            }
        } else if (mode == 3) {
            for (int k = 1; k < (int)view.count(); ++k) {
                int key = view[k];
                int j = k - 1;
                while (j >= 0 && nodes[key].name < nodes[view[j]].name) {
                    view[j + 1] = view[j];
                    --j;
                }
                view[j + 1] = key;
            }
        }
    }
    nodeViewIdx = view;
    v2TableClear(nodeList);
    v2TableSetRows(nodeList, (int)view.count());
    for (int r = 0; r < (int)view.count(); ++r) {
        int i = view[r];
        const V2Node& n = nodes[i];
        QString name = n.name.isEmpty()
            ? (qFromUtf8("节点") + QString::number(i + 1)) : n.name;
        QString tcp = (n.tcpMs == -2) ? qFromUtf8("—")
                    : (n.tcpMs < 0 ? qFromUtf8("超时") : QString::number(n.tcpMs));
        QString real = (n.realMs == -2) ? qFromUtf8("—")
                     : (n.realMs < 0 ? qFromUtf8("超时") : QString::number(n.realMs));
        QString st;
        if (!n.supported) {
            st = (n.proto == "hysteria2") ? qFromUtf8("未实现") : qFromUtf8("需Xray");
        } else if (n.realMs > 0) {
            st = qFromUtf8("可用");
        } else if (n.realMs == -1) {
            st = qFromUtf8("不可用");
        } else if (n.tcpMs > 0) {
            st = qFromUtf8("端口通");
        } else if (n.tcpMs == -1) {
            st = qFromUtf8("端口失败");
        } else {
            st = qFromUtf8("未测");
        }
        QString geoText = n.country.isEmpty() ? qFromUtf8("—")
            : (n.countryName.isEmpty() ? n.country
               : n.country + " " + n.countryName);
        QPixmap flag;
        if (n.country.length() == 2) { flag = v2FlagPixmap(n.country, 14); }
        int avail = 2;
        if (!n.supported) {
            avail = 1;
        } else if (n.realMs > 0) {
            avail = 0;
        } else if (n.realMs == -1) {
            avail = 1;
        }
        v2TableSetCell(nodeList, r, 0, name);
        v2TableSetCell(nodeList, r, 1, n.proto);
        if (!flag.isNull()) {
#ifdef QT3_BUILD
            nodeList->setPixmap(r, 2, flag);
#else
            QTableWidgetItem* fi = new QTableWidgetItem();
            fi->setIcon(QIcon(flag));
            nodeList->setItem(r, 2, fi);
#endif
        }
        v2TableSetCell(nodeList, r, 3, n.addr);
        v2TableSetCell(nodeList, r, 4, n.port);
        /* 列顺序：5=可用 6=状态 7=真连 8=TCP 9=传输 10=安全（国家列隐藏） */
#ifdef QT3_BUILD
        nodeList->setPixmap(r, 5, v2StatusIcon(avail));
#else
        {
            QTableWidgetItem* ai = new QTableWidgetItem();
            ai->setIcon(QIcon(v2StatusIcon(avail)));
            ai->setToolTip(geoText);
            nodeList->setItem(r, 5, ai);
        }
#endif
        v2TableSetCell(nodeList, r, 6, st);
        v2TableSetCell(nodeList, r, 7, real);
        v2TableSetCell(nodeList, r, 8, tcp);
        v2TableSetCell(nodeList, r, 9, n.net.isEmpty() ? QString("tcp") : n.net);
        v2TableSetCell(nodeList, r, 10, n.security.isEmpty() ? QString("none") : n.security);
    }
    emit nodesChanged();
    refreshFavList();
}

/* 国旗列 tooltip 文本：国家代码 + 中文名（隐藏的国家列信息） */
QString V2RayLogic::nodeCountryTip(int row) const {
    int idx = -1;
    if (!nodeViewIdx.isEmpty()) {
        if (row < 0 || row >= (int)nodeViewIdx.count()) { return QString(); }
        idx = nodeViewIdx[row];
    } else {
        idx = row;
    }
    if (idx < 0 || idx >= (int)nodes.count()) { return QString(); }
    const V2Node& n = nodes[idx];
    if (n.country.isEmpty()) { return qFromUtf8("国家：未知"); }
    QString tip = n.country;
    if (!n.countryName.isEmpty()) { tip += " " + n.countryName; }
    return tip;
}

/* 收藏表国旗列 tooltip 文本（row 经 favNodeIndex 映射回节点） */
QString V2RayLogic::favCountryTip(int row) const {
    int idx = favNodeIndex(row);
    if (idx < 0 || idx >= (int)nodes.count()) { return QString(); }
    const V2Node& n = nodes[idx];
    if (n.country.isEmpty()) { return qFromUtf8("国家：未知"); }
    QString tip = n.country;
    if (!n.countryName.isEmpty()) { tip += " " + n.countryName; }
    return tip;
}

/* ── 收藏页列表 ── */
int V2RayLogic::favNodeIndex(int row) const {
    if (row < 0 || !favList) { return -1; }
    int k = 0;
    for (int i = 0; i < (int)nodes.count(); ++i) {
        if (!nodes[i].favorite) { continue; }
        if (k == row) { return i; }
        ++k;
    }
    return -1;
}

void V2RayLogic::refreshFavList() {
    if (!favList) { return; }
    if (v2TableCols(favList) == 0) {
        QStringList cols;
        cols << qFromUtf8("名称") << qFromUtf8("协议") << qFromUtf8("国旗")
             << qFromUtf8("地址") << qFromUtf8("端口")
             << qFromUtf8("真连(ms)") << qFromUtf8("可用");
        v2TableInit(favList, cols);
#ifdef QT3_BUILD
        favList->setColumnWidth(0, 200);
        favList->setColumnWidth(1, 60);
        favList->setColumnWidth(2, 40);
        favList->setColumnWidth(3, 170);
        favList->setColumnWidth(4, 60);
        favList->setColumnWidth(5, 80);
        favList->setColumnWidth(6, 48);
#else
        favList->setColumnWidth(0, 200);
        favList->setColumnWidth(1, 60);
        favList->setColumnWidth(2, 40);
        favList->setColumnWidth(3, 170);
        favList->setColumnWidth(4, 60);
        favList->setColumnWidth(5, 80);
        favList->setColumnWidth(6, 48);
#endif
    }
    int cnt = 0;
    for (int i = 0; i < (int)nodes.count(); ++i) {
        if (nodes[i].favorite) { ++cnt; }
    }
    v2TableClear(favList);
    v2TableSetRows(favList, cnt);
    int r = 0;
    for (int i = 0; i < (int)nodes.count(); ++i) {
        const V2Node& n = nodes[i];
        if (!n.favorite) { continue; }
        QString name = n.name.isEmpty()
            ? (qFromUtf8("节点") + QString::number(i + 1)) : n.name;
        QString real = (n.realMs == -2) ? qFromUtf8("—")
                     : (n.realMs < 0 ? qFromUtf8("超时") : QString::number(n.realMs));
        QString geoText = n.country.isEmpty() ? qFromUtf8("—")
            : (n.countryName.isEmpty() ? n.country
               : n.country + " " + n.countryName);
        QPixmap flag;
        if (n.country.length() == 2) { flag = v2FlagPixmap(n.country, 14); }
        int avail = 2;
        if (!n.supported) {
            avail = 1;
        } else if (n.realMs > 0) {
            avail = 0;
        } else if (n.realMs == -1) {
            avail = 1;
        }
        v2TableSetCell(favList, r, 0, name);
        v2TableSetCell(favList, r, 1, n.proto);
        if (!flag.isNull()) {
#ifdef QT3_BUILD
            favList->setPixmap(r, 2, flag);
#else
            QTableWidgetItem* fi = new QTableWidgetItem();
            fi->setIcon(QIcon(flag));
            favList->setItem(r, 2, fi);
#endif
        }
        v2TableSetCell(favList, r, 3, n.addr);
        v2TableSetCell(favList, r, 4, n.port);
        v2TableSetCell(favList, r, 5, real);
#ifdef QT3_BUILD
        favList->setPixmap(r, 6, v2StatusIcon(avail));
#else
        {
            QTableWidgetItem* ai = new QTableWidgetItem();
            ai->setIcon(QIcon(v2StatusIcon(avail)));
            ai->setToolTip(geoText);
            favList->setItem(r, 6, ai);
        }
#endif
        ++r;
    }
}

/* ── 收藏操作 ── */
void V2RayLogic::onToggleFavorite() {
    int idx = selectedNodeIndex();
    if (idx < 0) { appendLogWithPrefix(qFromUtf8("[收藏]"), qFromUtf8("请先选中节点")); return; }
    nodes[idx].favorite = !nodes[idx].favorite;
    saveNodes();
    refreshNodeList();
    appendLogWithPrefix(qFromUtf8("[收藏]"),
        (nodes[idx].favorite ? qFromUtf8("已加入收藏：") : qFromUtf8("已取消收藏："))
        + nodes[idx].name);
}

void V2RayLogic::onUnfavFav() {
    if (!favList) { return; }
    int row = v2TableRow(favList);
    int idx = favNodeIndex(row);
    if (idx < 0) { appendLogWithPrefix(qFromUtf8("[收藏]"), qFromUtf8("请先选中收藏节点")); return; }
    nodes[idx].favorite = false;
    saveNodes();
    refreshNodeList();
    appendLogWithPrefix(qFromUtf8("[收藏]"), qFromUtf8("已取消收藏：") + nodes[idx].name);
}

void V2RayLogic::onCopyFavLink() {
    if (!favList) { return; }
    int row = v2TableRow(favList);
    int idx = favNodeIndex(row);
    if (idx < 0) { appendLogWithPrefix(qFromUtf8("[收藏]"), qFromUtf8("请先选中收藏节点")); return; }
    QApplication::clipboard()->setText(nodes[idx].raw);
    appendLogWithPrefix(qFromUtf8("[收藏]"), qFromUtf8("已复制节点链接：") + nodes[idx].name);
}

void V2RayLogic::onStartFavCore() {
    if (!favList) { return; }
    int row = v2TableRow(favList);
    int idx = favNodeIndex(row);
    if (idx < 0) { appendLogWithPrefix(qFromUtf8("[收藏]"), qFromUtf8("请先选中收藏节点")); return; }
    startCoreAtIndex(idx);
}

void V2RayLogic::setRouteWidgets(QCheckBox* bypassCnCheck_, QCheckBox* bypassLanCheck_) {
    bypassCnCheck = bypassCnCheck_;
    bypassLanCheck = bypassLanCheck_;
    /* B：载入已保存的路由选项（默认两项均 true） */
    QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
    if (bypassCnCheck) { bypassCnCheck->setChecked(st.valueBool("bypassCn", true)); }
    if (bypassLanCheck) { bypassLanCheck->setChecked(st.valueBool("bypassLan", true)); }
    if (bypassCnCheck && !hasGeoFiles()) {
        appendLogWithPrefix(qFromUtf8("[核心]"),
            qFromUtf8("提示：未找到 geoip.dat / geosite.dat（xray/ 与 core/ 目录），勾选“绕过中国大陆”将不生效（重新下载安装核心即可）"));
    }
}

/* D：监听地址/端口（B：随路由选项一起持久化） */
void V2RayLogic::setListenWidgets(QComboBox* addrBox, QLineEdit* portEdit) {
    setListenWidgets(addrBox, portEdit, 0);
}

void V2RayLogic::setListenWidgets(QComboBox* addrBox, QLineEdit* socksEdit,
                                  QLineEdit* httpEdit) {
    listenAddrBox = addrBox;
    listenSocksEdit = socksEdit;
    listenHttpEdit = httpEdit;
    listenPortEdit = socksEdit;   /* 兼容旧路径 */
    QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
    QString addr = st.valueStr("listenAddr", "127.0.0.1");
    if (listenAddrBox) {
#ifdef QT3_BUILD
        listenAddrBox->setCurrentText(addr);
#else
        listenAddrBox->setEditText(addr);
#endif
    }
    int socks = st.valueInt("listenSocksPort", st.valueInt("listenPort", 10808));
    int http = st.valueInt("listenHttpPort", socks == 10808 ? 10809 : socks + 1);
    if (http == socks) { http = socks + 1; }
    if (listenSocksEdit) { listenSocksEdit->setText(QString::number(socks)); }
    if (listenHttpEdit) { listenHttpEdit->setText(QString::number(http)); }
}

QString V2RayLogic::listenAddress() const {
    QString a = listenAddrBox ? listenAddrBox->currentText() : QString();
#ifdef QT3_BUILD
    a = a.stripWhiteSpace();
#else
    a = a.trimmed();
#endif
    if (a.isEmpty()) { a = "127.0.0.1"; }
    return a;
}

int V2RayLogic::listenSocksPort() const {
    if (!listenSocksEdit) { return 10808; }
    bool ok = false;
    int p = listenSocksEdit->text().toInt(&ok);
    if (!ok || p < 1 || p > 65535) { return -1; }
    return p;
}

int V2RayLogic::listenHttpPort() const {
    if (!listenHttpEdit) { return 10809; }
    bool ok = false;
    int p = listenHttpEdit->text().toInt(&ok);
    if (!ok || p < 1 || p > 65535) { return -1; }
    return p;
}

int V2RayLogic::listenPort() const {
    return listenSocksPort();
}

void V2RayLogic::onSaveSettings() {
    saveSettings();
}

void V2RayLogic::setCoreMessage(const QString& msg) {
    if (coreState) { coreState->setText(msg); }
}

void V2RayLogic::setCoreTestWidgets(QLabel* socksIcon, QLabel* socksText,
                                    QLabel* httpIcon, QLabel* httpText,
                                    QLabel* msg) {
    coreTestSocksIcon = socksIcon;
    coreTestSocksText = socksText;
    coreTestHttpIcon = httpIcon;
    coreTestHttpText = httpText;
    coreTestMsg = msg;
    /* 初值：灰线 + 未测试；随后恢复上次测试结果（若有） */
    setCoreTestChips(2, qFromUtf8("SOCKS —"), 2, qFromUtf8("HTTP —"));
    restoreTestResult();
}

void V2RayLogic::setCoreTestTimeWidget(QLabel* timeLabel) {
    coreTestTime = timeLabel;
    if (coreTestTime) {
        /* setCoreTestWidgets（含 restore）先于此调用，回放已恢复的时间 */
        coreTestTime->setText(lastTestTime.isEmpty() ? QString()
            : qFromUtf8("[") + lastTestTime + "]");
    }
}

/* kind：0=绿勾 1=红叉 2=灰线；-1=保留图标。文本一律原始文案，不内嵌时间 */
void V2RayLogic::setCoreTestChips(int socksKind, const QString& socksText,
                                  int httpKind, const QString& httpText) {
    if (coreTestSocksIcon && socksKind >= 0) {
        coreTestSocksIcon->setPixmap(v2StatusIcon(socksKind));
    }
    if (coreTestSocksText) { coreTestSocksText->setText(socksText); }
    if (coreTestHttpIcon && httpKind >= 0) {
        coreTestHttpIcon->setPixmap(v2StatusIcon(httpKind));
    }
    if (coreTestHttpText) { coreTestHttpText->setText(httpText); }
    if (coreTestMsg) { coreTestMsg->setText(QString()); }
}

/* 行尾唯一时间（空串隐藏）。有完整结果才显示 */
void V2RayLogic::setCoreTestTime(const QString& timeText) {
    lastTestTime = timeText;
    if (coreTestTime) { coreTestTime->setText(timeText); }
}

/* 结果落盘（文本原始 + 独立时间键） */
void V2RayLogic::saveTestResult() {
    QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
    st.setValue("core/testSocksKind", lastTestSocksKind);
    st.setValue("core/testSocksText", lastTestSocks);
    st.setValue("core/testHttpKind", lastTestHttpKind);
    st.setValue("core/testHttpText", lastTestHttp);
    st.setValue("core/testTime", lastTestTime);
    st.setValue("core/testMsg", lastTestMsg);
    st.sync();
}

void V2RayLogic::restoreTestResult() {
    QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
    lastTestSocksKind = st.valueInt("core/testSocksKind", 2);
    lastTestSocks = st.valueStr("core/testSocksText", "");
    lastTestHttpKind = st.valueInt("core/testHttpKind", 2);
    lastTestHttp = st.valueStr("core/testHttpText", "");
    lastTestTime = st.valueStr("core/testTime", "");
    lastTestMsg = st.valueStr("core/testMsg", "");
    /* 兼容旧格式：文本内嵌 " [时间]" → 剥离并还原为独立时间键 */
    if (lastTestTime.isEmpty()) {
        QString legacy;
#ifdef QT3_BUILD
        int hp = lastTestHttp.findRev(qFromUtf8(" ["));
#else
        int hp = lastTestHttp.lastIndexOf(qFromUtf8(" ["));
#endif
        if (hp > 0 && lastTestHttp.endsWith("]")) {
            legacy = lastTestHttp.mid(hp + 2, lastTestHttp.length() - hp - 3);
            lastTestHttp = lastTestHttp.left(hp);
        }
#ifdef QT3_BUILD
        int sp = lastTestSocks.findRev(qFromUtf8(" ["));
#else
        int sp = lastTestSocks.lastIndexOf(qFromUtf8(" ["));
#endif
        if (sp > 0 && lastTestSocks.endsWith("]")) {
            if (legacy.isEmpty()) {
                legacy = lastTestSocks.mid(sp + 2, lastTestSocks.length() - sp - 3);
            }
            lastTestSocks = lastTestSocks.left(sp);
        }
        lastTestTime = legacy;
    }
    if (lastTestSocks.isEmpty() && lastTestHttp.isEmpty()) { return; }
    if (coreTestSocksIcon && lastTestSocksKind >= 0 && lastTestSocksKind <= 2) {
        coreTestSocksIcon->setPixmap(v2StatusIcon(lastTestSocksKind));
    }
    if (coreTestSocksText && !lastTestSocks.isEmpty()) {
        coreTestSocksText->setText(lastTestSocks);
    }
    if (coreTestHttpIcon && lastTestHttpKind >= 0 && lastTestHttpKind <= 2) {
        coreTestHttpIcon->setPixmap(v2StatusIcon(lastTestHttpKind));
    }
    if (coreTestHttpText && !lastTestHttp.isEmpty()) {
        coreTestHttpText->setText(lastTestHttp);
    }
    if (coreTestTime && !lastTestTime.isEmpty()) {
        coreTestTime->setText(qFromUtf8("[") + lastTestTime + "]");
    }
    if (coreTestMsg && !lastTestMsg.isEmpty()) { coreTestMsg->setText(lastTestMsg); }
}

void V2RayLogic::setCoreTestMessage(const QString& msg) {
    if (!coreTestMsg) { return; }
    /* 提示语不带时间：整行只在行尾显示一个时间（setCoreTestTime） */
    coreTestMsg->setText(msg);
}

void V2RayLogic::setTrafficWidget(QLabel* lbl) {
    trafficLabel = lbl;
    refreshTrafficLabel();
}

/* 累计流量落盘（写入后基线不动，累计=基线+本次会话） */
void V2RayLogic::flushTrafficTotals() {
    QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
    st.setValue("core/totalUp", QString::number(totalUp));
    st.setValue("core/totalDown", QString::number(totalDown));
    st.sync();
}

void V2RayLogic::refreshTrafficLabel() {
    if (!trafficLabel) { return; }
    bool running = coreProc && coreProc->isRunning();
    if (!running || apiPort <= 0) {
        trafficLabel->setText(qFromUtf8("流量：—（核心未运行）"));
        return;
    }
    trafficLabel->setText(qFromUtf8("速度：↑ ")
        + v2BytesRate(trafficUpRate) + qFromUtf8(" ↓ ")
        + v2BytesRate(trafficDownRate)
        + qFromUtf8(" · 流量：↑ ") + v2Bytes(trafficUpLast)
        + qFromUtf8(" ↓ ") + v2Bytes(trafficDownLast)
        + qFromUtf8(" · 累计：↑ ") + v2Bytes(totalUp)
        + qFromUtf8(" ↓ ") + v2Bytes(totalDown)
        + qFromUtf8(" · 请求 ") + QString::number(reqCountLast)
        + qFromUtf8(" 次"));
}

void V2RayLogic::setFavWidgets(V2Table* fav) {
    favList = fav;
    refreshFavList();
}

void V2RayLogic::setNodeViewWidgets(QLineEdit* filterEdit, QComboBox* sortBox) {
    nodeFilterEdit = filterEdit;
    nodeSortBox = sortBox;
}

void V2RayLogic::onNodeFilterChanged() { refreshNodeList(); }
void V2RayLogic::onNodeSortChanged() { refreshNodeList(); }

void V2RayLogic::setActiveNodeWidget(QLabel* lbl) {
    activeNodeLabel = lbl;
    refreshActiveNodeLabel();
}

void V2RayLogic::refreshActiveNodeLabel() {
    if (!activeNodeLabel) { return; }
    if (nodes.count() == 0) {
        activeNodeLabel->setText(qFromUtf8("当前节点：—"));
        return;
    }
    bool running = coreProc && coreProc->isRunning();
    int idx = -1;
    if (running) {
        QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
        idx = indexByNodeId(st.valueStr("activeNode", ""));
    }
    if (idx < 0) { idx = selectedNodeIndex(); }
    if (idx < 0) {
        QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
        idx = indexByNodeId(st.valueStr("activeNode", ""));
    }
    if (idx < 0 || idx >= (int)nodes.count()) {
        activeNodeLabel->setText(qFromUtf8("当前节点：—"));
        return;
    }
    const V2Node& n = nodes[idx];
    QString name = n.name.isEmpty() ? n.id : n.name;
    QString s = qFromUtf8("当前节点：") + name
        + qFromUtf8(" · ") + n.proto + " " + n.addr + ":" + n.port;
    if (n.realMs > 0) {
        s += qFromUtf8(" · 真连 ") + QString::number(n.realMs) + "ms";
    }
    s += running ? qFromUtf8("（使用中）") : qFromUtf8("（未启动）");
    activeNodeLabel->setText(s);
}

void V2RayLogic::onNodeSelectionChanged() {
    refreshActiveNodeLabel();
}

/* 重启恢复：上次运行中则自动启动核心 */
void V2RayLogic::restoreCoreOnStartup() {
    QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
    bool lastRunning = st.valueInt("core/runningLast", 0) != 0;
    if (!lastRunning || !coreInstalled()) {
        refreshActiveNodeLabel();
        refreshCoreState();
        return;
    }
    appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("检测到上次为运行状态，正在自动恢复…"));
    onStartCore();
}

void V2RayLogic::setCoreWidgets(QPushButton* dlBtn) {
    coreDlBtn = dlBtn;
}

void V2RayLogic::setCoreDownloadBusy(bool busy) {
    if (coreDlBtn) { coreDlBtn->setEnabled(!busy); }
}

void V2RayLogic::setUpdateWidgets(QPushButton* updBtn_, QPushButton* updAllBtn_,
                                  QLabel* nodeStatus_) {
    updBtn = updBtn_;
    updAllBtn = updAllBtn_;
    nodeStatus = nodeStatus_;
}

void V2RayLogic::setUpdateBusy(bool busy, const QString& msg) {
    if (updBtn)     { updBtn->setEnabled(!busy); }
    if (updAllBtn)  { updAllBtn->setEnabled(!busy); }
    if (nodeStatus) { nodeStatus->setText(msg); }
}

void V2RayLogic::setAutoUpdateWidgets(QCheckBox* chk, QComboBox* freq) {
    autoUpdateChk = chk;
    autoUpdateFreqBox = freq;
    static const int mins[] = { 30, 60, 120, 180, 360, 720, 1440 };
    if (autoUpdateChk) {
        autoUpdateChk->setChecked(autoUpdateEnabled);
    }
    if (autoUpdateFreqBox) {
        int idx = 2;   /* 默认 2 小时 */
        for (int i = 0; i < 7; ++i) {
            if (mins[i] == autoUpdateIntervalMin) { idx = i; break; }
        }
        v2ComboSetCurrent(autoUpdateFreqBox, idx);
        autoUpdateFreqBox->setEnabled(autoUpdateEnabled);
    }
}

void V2RayLogic::onAutoUpdateSettingsChanged() {
    static const int mins[] = { 30, 60, 120, 180, 360, 720, 1440 };
    autoUpdateEnabled = autoUpdateChk && autoUpdateChk->isChecked();
    int idx = autoUpdateFreqBox ? v2ComboCurrent(autoUpdateFreqBox) : 2;
    if (idx >= 0 && idx < 7) { autoUpdateIntervalMin = mins[idx]; }
    if (autoUpdateFreqBox) { autoUpdateFreqBox->setEnabled(autoUpdateEnabled); }
    if (autoUpdateTimer) {
        if (autoUpdateEnabled) {
            autoUpdateTimer->start(autoUpdateIntervalMin * 60 * 1000);
        } else {
            autoUpdateTimer->stop();
        }
    }
    QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
    st.setValue("sub/autoUpdate", autoUpdateEnabled);
    st.setValue("sub/autoUpdateMin", autoUpdateIntervalMin);
    st.sync();
    appendLogWithPrefix(qFromUtf8("[订阅]"),
        autoUpdateEnabled
            ? (qFromUtf8("自动更新：开启（每 ") + QString::number(autoUpdateIntervalMin) + qFromUtf8(" 分钟）"))
            : qFromUtf8("自动更新：关闭"));
}

void V2RayLogic::loadSubMeta() {
    subMeta.clear();
    QString data = readUtf8File(baseDir() + "/sub_meta.txt");
    QStringList lines = splitLines(data);
    for (int i = 0; i < (int)lines.count(); ++i) {
#ifdef QT3_BUILD
        QString line = lines[i].stripWhiteSpace();
#else
        QString line = lines[i].trimmed();
#endif
        if (line.isEmpty()) { continue; }
        int tab = sFind(line, '\t', 0);
        if (tab <= 0) { continue; }
        subMeta.insert(line.left(tab), line.mid(tab + 1));
    }
}

void V2RayLogic::saveSubMeta() {
    QString all;
    for (QMap<QString, QString>::iterator it = subMeta.begin(); it != subMeta.end(); ++it) {
#ifdef QT3_BUILD
        all += it.key() + "\t" + it.data() + "\n";
#else
        all += it.key() + "\t" + it.value() + "\n";
#endif
    }
    writeUtf8File(baseDir() + "/sub_meta.txt", all);
}

void V2RayLogic::onSaveAllOnQuit() {
    saveNodes();
    saveSubMeta();
    flushTrafficTotals();   /* 退出前把累计流量写回 */
}

/* 退出时统一终止所有子进程（保留 core/runningLast 以便下次自动恢复） */
void V2RayLogic::shutdownProcesses() {
    appQuitting = true;
    QProcess34* ps[] = { probeProc, coreProc, latProc, testCurlProc, testCoreProc,
                       subProc, dlProc, unzipProc, detectProc, geoProc };
    int n = (int)(sizeof(ps) / sizeof(ps[0]));
    for (int i = 0; i < n; ++i) {
        if (ps[i] && ps[i]->isRunning()) {
            ps[i]->tryTerminate();
        }
    }
}

/* ── 内核选择（v2ray / Xray 系统 / Xray 内置） ── */
void V2RayLogic::loadCoreSettings() {
    QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
    QString kind = st.valueStr("core/kind", "v2ray");
    coreKind = (kind == "xray") ? CoreXray : CoreV2ray;
    coreSourceSystem = (st.valueStr("core/source", "builtin") == "system");
    QString sp = st.valueStr("core/systemPath", "");
    if (!sp.isEmpty()) { systemXrayPath = sp; }
    systemXrayVersion = st.valueStr("core/xrayVersion", "");
}

void V2RayLogic::saveCoreSettings() {
    QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
    st.setValue("core/kind", coreKind == CoreXray ? QString("xray") : QString("v2ray"));
    st.setValue("core/source", coreSourceSystem ? QString("system") : QString("builtin"));
    st.setValue("core/systemPath", systemXrayPath);
    st.setValue("core/xrayVersion", systemXrayVersion);
    st.sync();
}

void V2RayLogic::detectSystemXray() {
    QStringList cand;
    QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
    QString ov = st.valueStr("core/systemPath", "");
    if (!ov.isEmpty()) { cand.append(ov); }
    const char* pe = getenv("PATH");
    QString pathEnv = pe ? QString(pe) : QString();
#ifdef QT3_BUILD
    QStringList dirs = QStringList::split(':', pathEnv);
#else
    QStringList dirs = pathEnv.split(QChar(':'));
#endif
    for (int i = 0; i < (int)dirs.count(); ++i) {
        if (!dirs[i].isEmpty()) { cand.append(dirs[i] + "/xray"); }
    }
    cand.append(QString("/usr/local/bin/xray"));
    cand.append(QString("/usr/bin/xray"));
    cand.append(QString("/opt/xray/xray"));
#ifdef QT3_BUILD
    cand.append(QDir::homeDirPath() + "/.local/bin/xray");
#else
    cand.append(QDir::homePath() + "/.local/bin/xray");
#endif
    QString found;
    for (int i = 0; i < (int)cand.count(); ++i) {
        if (fileExists(cand[i])
            && access(QFile::encodeName(cand[i]).data(), X_OK) == 0) {
            found = cand[i];
            break;
        }
    }
    if (found != systemXrayPath) { systemXrayVersion = QString(); }
    systemXrayPath = found;
    if (detectProc) { delete detectProc; detectProc = 0; }
    if (found.isEmpty()) {
        systemXrayVersion = QString();
        appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("未检测到系统 Xray（可下载内置版）"));
        if (coreKind == CoreXray && coreSourceSystem) {
            coreSourceSystem = false;      /* 系统版没了 → 回落内置 */
            saveCoreSettings();
        }
        if (!st.contains("core/kind")) {
            coreKind = CoreV2ray;          /* 首次运行：无系统 Xray 用 v2ray */
            coreSourceSystem = false;
            saveCoreSettings();
        }
        refreshCoreCombo();
        refreshCoreState();
        return;
    }
    /* 首次运行且未保存过选择：默认使用系统 Xray */
    if (!st.contains("core/kind")) {
        coreKind = CoreXray;
        coreSourceSystem = true;
        saveCoreSettings();
    }
    detectData.resize(0);
    detectProc = new QProcess34(this);
    QProcess34::connectStdout(detectProc, this, SLOT(onDetectOutput()));
    QProcess34::connectExited(detectProc, this, SLOT(onDetectExited()));
    detectProc->addArgument(found);
    detectProc->addArgument("version");
    detectProc->start();
    appendLogWithPrefix(qFromUtf8("[核心]"),
        qFromUtf8("发现系统 Xray：") + found + qFromUtf8("（读取版本…）"));
}

void V2RayLogic::onDetectOutput() {
    if (!detectProc) { return; }
#ifdef QT3_BUILD
    baAppend(detectData, detectProc->readStdout());
#else
    baAppend(detectData, detectProc->readAllStandardOutput());
#endif
}

void V2RayLogic::onDetectExited() {
    if (!detectProc) { return; }
    delete detectProc;
    detectProc = 0;
    QString out = qFromUtf8(detectData.data(), (int)detectData.size());
    int sp = sFind(out, ' ', 0);
    QString ver = (sp > 0) ? out.mid(sp + 1) : QString();
    int sp2 = sFind(ver, ' ', 0);
    if (sp2 > 0) { ver = ver.left(sp2); }
#ifdef QT3_BUILD
    ver = ver.stripWhiteSpace();
#else
    ver = ver.trimmed();
#endif
    systemXrayVersion = ver;
    appendLogWithPrefix(qFromUtf8("[核心]"),
        qFromUtf8("系统 Xray 版本：")
        + (systemXrayVersion.isEmpty() ? qFromUtf8("未知") : systemXrayVersion));
    saveCoreSettings();
    recomputeSupport();
    refreshNodeList();
    refreshCoreCombo();
    refreshCoreState();
}

/* >=1.8 支持 REALITY：1=可用，0=过低，-1=未知 */
int V2RayLogic::xraySystemVersionOk() const {
    if (systemXrayVersion.isEmpty()) { return -1; }
#ifdef QT3_BUILD
    QStringList parts = QStringList::split('.', systemXrayVersion);
#else
    QStringList parts = systemXrayVersion.split(QChar('.'));
#endif
    if (parts.count() < 2) { return -1; }
    bool ok1 = false;
    bool ok2 = false;
    int maj = parts[0].toInt(&ok1);
    int min = parts[1].toInt(&ok2);
    if (!ok1 || !ok2) { return -1; }
    if (maj > 1) { return 1; }
    if (maj < 1) { return 0; }
    return (min >= 8) ? 1 : 0;
}

QString V2RayLogic::coreDisplayName() const {
    if (coreKind == CoreV2ray) {
        return qFromUtf8("v2ray-core ") + qFromUtf8(V2CORE_VERSION);
    }
    if (coreSourceSystem) {
        return qFromUtf8("Xray ") + (systemXrayVersion.isEmpty()
            ? qFromUtf8("(系统)") : systemXrayVersion) + qFromUtf8("（系统）");
    }
    return qFromUtf8("Xray ") + qFromUtf8(XCORE_VERSION) + qFromUtf8("（内置）");
}

void V2RayLogic::refreshCoreCombo() {
    if (!coreSelBox) { return; }
    QString sysText = systemXrayPath.isEmpty()
        ? qFromUtf8("Xray（系统：未找到）")
        : qFromUtf8("Xray（系统：") + systemXrayPath
            + (systemXrayVersion.isEmpty() ? QString("")
               : qFromUtf8(" · ") + systemXrayVersion) + qFromUtf8("）");
    int idx = (coreKind == CoreV2ray) ? 0 : (coreSourceSystem ? 1 : 2);
#ifdef QT3_BUILD
    coreSelBox->clear();
    coreSelBox->insertItem(qFromUtf8("v2ray-core（内置 ") + qFromUtf8(V2CORE_VERSION) + qFromUtf8("）"));
    coreSelBox->insertItem(sysText);
    coreSelBox->insertItem(qFromUtf8("Xray（内置 ") + qFromUtf8(XCORE_VERSION) + qFromUtf8("）"));
    coreSelBox->setCurrentItem(idx);
#else
    coreSelBox->clear();
    coreSelBox->addItem(qFromUtf8("v2ray-core（内置 ") + qFromUtf8(V2CORE_VERSION) + qFromUtf8("）"));
    coreSelBox->addItem(sysText);
    coreSelBox->addItem(qFromUtf8("Xray（内置 ") + qFromUtf8(XCORE_VERSION) + qFromUtf8("）"));
    coreSelBox->setCurrentIndex(idx);
#endif
}

void V2RayLogic::setCoreSelector(QComboBox* box, QPushButton* redetect) {
    coreSelBox = box;
    redetectBtn = redetect;
    refreshCoreCombo();
    refreshCoreState();
}

void V2RayLogic::onRedetectCore() {
    detectSystemXray();
}

void V2RayLogic::onCoreKindChanged(int index) {
    if (coreProc && coreProc->isRunning()) {
        appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("请先停止核心再切换内核"));
        refreshCoreCombo();
        return;
    }
    CoreKind k = CoreV2ray;
    bool srcSystem = false;
    if (index == 1) {
        if (systemXrayPath.isEmpty()) {
            appendLogWithPrefix(qFromUtf8("[核心]"),
                qFromUtf8("未检测到系统 Xray，请点“重新检测”或改用内置版"));
            refreshCoreCombo();
            return;
        }
        k = CoreXray;
        srcSystem = true;
    } else if (index == 2) {
        k = CoreXray;
        srcSystem = false;
    }
    coreKind = k;
    coreSourceSystem = srcSystem;
    saveCoreSettings();
    recomputeSupport();
    refreshNodeList();
    refreshCoreState();
    appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("已切换内核：") + coreDisplayName());
}

/* ── 国家查询（离线 geoiplookup 队列） ── */
QString V2RayLogic::findGeoTool() const {
    const char* pe = getenv("PATH");
    QString pathEnv = pe ? QString(pe) : QString();
#ifdef QT3_BUILD
    QStringList dirs = QStringList::split(':', pathEnv);
#else
    QStringList dirs = pathEnv.split(QChar(':'));
#endif
    for (int i = 0; i < (int)dirs.count(); ++i) {
        if (dirs[i].isEmpty()) { continue; }
        QString p = dirs[i] + "/geoiplookup";
        if (fileExists(p) && access(QFile::encodeName(p).data(), X_OK) == 0) {
            return p;
        }
    }
    return QString();
}

void V2RayLogic::loadGeoCache() {
    geoCache.clear();
    QByteArray tb = qToUtf8(readUtf8File(baseDir() + "/geo_cache.json"));
    if (tb.isEmpty()) { return; }
    cJSON* root = cJSON_Parse(tb.data());
    if (!root) { return; }
    for (cJSON* it = root->child; it; it = it->next) {
        if (it->string && it->valuestring) {
            geoCache.insert(qFromUtf8(it->string), qFromUtf8(it->valuestring));
        }
    }
    cJSON_Delete(root);
}

void V2RayLogic::saveGeoCache() {
    cJSON* root = cJSON_CreateObject();
    for (QMap<QString, QString>::iterator it = geoCache.begin(); it != geoCache.end(); ++it) {
#ifdef QT3_BUILD
        cJSON_AddStringToObject(root, qToUtf8(it.key()).data(), qToUtf8(it.data()).data());
#else
        cJSON_AddStringToObject(root, qToUtf8(it.key()).data(), qToUtf8(it.value()).data());
#endif
    }
    char* txt = cJSON_PrintUnformatted(root);
    if (txt) {
        writeUtf8File(baseDir() + "/geo_cache.json", qFromUtf8(txt));
        cJSON_free(txt);
    }
    cJSON_Delete(root);
}

static bool v2IsIpLiteral(const QString& a) {
    if (a.isEmpty()) { return false; }
    for (int i = 0; i < (int)a.length(); ++i) {
        QChar c = a[i];
        bool ok = c.isDigit() || c == '.' || c == ':'
            || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!ok) { return false; }
    }
    return true;
}

void V2RayLogic::startGeoLookup(bool all) {
    if (geoBusy) {
        appendLog(qFromUtf8("国家查询已在进行中"));
        return;
    }
    geoToolPath = findGeoTool();
    geoToolOk = !geoToolPath.isEmpty();
    geoQueue.clear();
    int sel = selectedNodeIndex();
    for (int i = 0; i < (int)nodes.count(); ++i) {
        if (!all && i != sel) { continue; }
        if (!nodes[i].country.isEmpty()) { continue; }
        if (!v2IsIpLiteral(nodes[i].addr)) { continue; }
        geoQueue.append(i);
    }
    if ((int)geoQueue.count() <= 0) {
        appendLog(qFromUtf8("没有需要查询国家的节点"));
        return;
    }
    if (!geoToolOk) {
        appendLogWithPrefix(qFromUtf8("[国家]"),
            qFromUtf8("未找到 geoiplookup，无法离线查询（可依赖名称关键词）"));
        return;
    }
    geoBusy = true;
    if (nodeStatus) { nodeStatus->setText(qFromUtf8("国家查询中…")); }
    appendLogWithPrefix(qFromUtf8("[国家]"),
        qFromUtf8("开始查询国家，共 ") + QString::number((int)geoQueue.count()) + qFromUtf8(" 个"));
    startNextGeo();
}

void V2RayLogic::startNextGeo() {
    if ((int)geoQueue.count() <= 0) {
        geoBusy = false;
        saveGeoCache();
        refreshNodeList();
        if (nodeStatus) { nodeStatus->setText(qFromUtf8("状态：国家查询完成")); }
        appendLogWithPrefix(qFromUtf8("[国家]"), qFromUtf8("国家查询完成"));
        return;
    }
    geoCurIdx = geoQueue[0];
#ifdef QT3_BUILD
    geoQueue.remove(geoQueue.at(0));
#else
    geoQueue.removeAt(0);
#endif
    if (geoCurIdx < 0 || geoCurIdx >= (int)nodes.count()) {
        QTimer::singleShot(0, this, SLOT(startNextGeo()));
        return;
    }
    QString ip = nodes[geoCurIdx].addr;
    if (geoCache.contains(ip)) {
        QString val = geoCache[ip];
        int bar = sFindStr(val, "|", 0);
        nodes[geoCurIdx].country = (bar > 0) ? val.left(bar) : val;
        nodes[geoCurIdx].countryName = (bar > 0) ? val.mid(bar + 1) : QString();
        nodes[geoCurIdx].geoSource = "geoip";
        QTimer::singleShot(0, this, SLOT(startNextGeo()));
        return;
    }
    if (geoProc) { delete geoProc; geoProc = 0; }
    geoData.resize(0);
    geoProc = new QProcess34(this);
    QProcess34::connectStdout(geoProc, this, SLOT(onGeoOutput()));
    QProcess34::connectExited(geoProc, this, SLOT(onGeoExited()));
    geoProc->addArgument(geoToolPath);
    geoProc->addArgument(ip);
    geoProc->start();
}

void V2RayLogic::onGeoOutput() {
    if (!geoProc) { return; }
#ifdef QT3_BUILD
    baAppend(geoData, geoProc->readStdout());
#else
    baAppend(geoData, geoProc->readAllStandardOutput());
#endif
}

void V2RayLogic::onGeoExited() {
    if (geoProc) {
        delete geoProc;
        geoProc = 0;
        QString out = qFromUtf8(geoData.data(), (int)geoData.size());
        int c = sFindStr(out, ": ", 0);
        if (c > 0 && geoCurIdx >= 0 && geoCurIdx < (int)nodes.count()) {
            QString body = out.mid(c + 2);
            int comma = sFindStr(body, ",", 0);
            QString cc = (comma > 0) ? body.left(comma) : body;
#ifdef QT3_BUILD
            cc = cc.stripWhiteSpace();
#else
            cc = cc.trimmed();
#endif
            if (cc.length() == 2) {
                cc = qToUpper(cc);
                nodes[geoCurIdx].country = cc;
                nodes[geoCurIdx].countryName = countryNameOf(cc);
                nodes[geoCurIdx].geoSource = "geoip";
                geoCache.insert(nodes[geoCurIdx].addr, cc + "|" + countryNameOf(cc));
            }
        }
    }
    QTimer::singleShot(0, this, SLOT(startNextGeo()));
}

void V2RayLogic::onLookupCountrySelected() {
    if (selectedNodeIndex() < 0) {
        appendLog(qFromUtf8("请先选中节点"));
        return;
    }
    startGeoLookup(false);
}
void V2RayLogic::onLookupCountryAll() { startGeoLookup(true); }
void V2RayLogic::onClearGeoCache() {
    geoCache.clear();
    saveGeoCache();
    appendLog(qFromUtf8("国家缓存已清空"));
}

/* ── 端口/连通性测试（全部用 curl，跨平台） ── */
static QString v2CurlCodeOf(const QByteArray& out) {
    QString s = qFromUtf8(out.data(), (int)out.size());
#ifdef QT3_BUILD
    s = s.stripWhiteSpace();
#else
    s = s.trimmed();
#endif
    int sp = sFind(s, ' ', 0);
    if (sp > 0) { s = s.left(sp); }
    return s.isEmpty() ? QString("000") : s;
}

/* gstatic 结果 → 友好文案；kind 0=成功 1=失败 */
static QString v2GstaticLabel(const QString& detail, const QString& err,
                              const QString& prefix, int& kind) {
    QString code = detail;
    QString time;
    int sp = sFind(code, ' ', 0);
    if (sp > 0) {
        time = code.mid(sp + 1);
        code = code.left(sp);
    }
    if (!code.isEmpty() && code != "000") {
        kind = 0;
        QString t = prefix + qFromUtf8(" 可用(") + code;
        if (!time.isEmpty()) { t += " " + time + qFromUtf8("s"); }
        t += ")";
        return t;
    }
    kind = 1;
    QString e = err;
#ifdef QT3_BUILD
    e = e.stripWhiteSpace();
#else
    e = e.trimmed();
#endif
    QString reason = qFromUtf8("连接失败");
    if (e.contains("timed out") || e.contains("Timeout")
        || e.contains("timeout")) {
        reason = qFromUtf8("超时");
    } else if (e.contains("Could not resolve") || e.contains("resolve host")) {
        reason = qFromUtf8("DNS 解析失败");
    } else if (e.contains("Connection refused")) {
        reason = qFromUtf8("连接被拒");
    } else if (e.contains("Connection reset")) {
        reason = qFromUtf8("连接被重置");
    } else if (e.contains("SSL") || e.contains("TLS")
               || e.contains("certificate")) {
        reason = qFromUtf8("TLS 失败");
    } else {
        bool ok = false;
        double secs = time.toDouble(&ok);
        if (ok && secs >= 4.7) { reason = qFromUtf8("超时"); }
    }
    QString t = prefix + qFromUtf8(" 失败[") + reason + "]";
    if (!time.isEmpty()) { t += "(" + time + qFromUtf8("s)"); }
    return t;
}

void V2RayLogic::probeStartPort(int which) {
    int port = (which == 0) ? listenSocksPort() : listenHttpPort();
    if (port < 0) { port = (which == 0) ? 10808 : 10809; }
    if (probeProc) {
        if (probeProc->isRunning()) { probeProc->tryTerminate(); }
        delete probeProc;
        probeProc = 0;
    }
    probeProc = new QProcess34(this);
    QProcess34::connectExited(probeProc, this, SLOT(onProbeExited()));
    probeProc->addArgument("curl");
    probeProc->addArgument("-s");
    probeProc->addArgument("-o");
    probeProc->addArgument("/dev/null");
    probeProc->addArgument("--connect-timeout");
    probeProc->addArgument("1");
    probeProc->addArgument("-m");
    probeProc->addArgument("2");
    probeProc->addArgument("-w");
    probeProc->addArgument("%{http_code}");
    probeProc->addArgument(QString("http://127.0.0.1:") + QString::number(port) + "/");
    probeProc->start();
}

void V2RayLogic::probeStartGstatic(int which) {
    int port = (which == 0) ? listenSocksPort() : listenHttpPort();
    if (port < 0) { port = (which == 0) ? 10808 : 10809; }
    if (probeProc) {
        if (probeProc->isRunning()) { probeProc->tryTerminate(); }
        delete probeProc;
        probeProc = 0;
    }
    probeProc = new QProcess34(this);
    QProcess34::connectExited(probeProc, this, SLOT(onProbeExited()));
    probeProc->addArgument("curl");
    probeProc->addArgument("-s");
    probeProc->addArgument("-S");
    probeProc->addArgument("-o");
    probeProc->addArgument("/dev/null");
    probeProc->addArgument("-m");
    probeProc->addArgument("5");
    probeProc->addArgument("-w");
    probeProc->addArgument("%{http_code} %{time_total}");
    if (which == 0) {
        probeProc->addArgument("--socks5-hostname");
        probeProc->addArgument(QString("127.0.0.1:") + QString::number(port));
    } else {
        probeProc->addArgument("-x");
        probeProc->addArgument(QString("http://127.0.0.1:") + QString::number(port));
    }
    probeProc->addArgument("https://www.gstatic.com/generate_204");
    probeProc->start();
}

void V2RayLogic::onTestPorts() {
    if (probeProc && probeProc->isRunning()) {
        appendLogWithPrefix(qFromUtf8("[端口]"), qFromUtf8("测试进行中，请稍候"));
        return;
    }
    int sp = listenSocksPort();
    int hp = listenHttpPort();
    if (sp < 0 || hp < 0) {
        setCoreTestMessage(qFromUtf8("端口非法"));
        return;
    }
    if (!(coreProc && coreProc->isRunning())) {
        /* 核心未运行：不算测试结果，恢复“—”并提示 */
        setCoreTestChips(2, qFromUtf8("SOCKS —"), 2, qFromUtf8("HTTP —"));
        setCoreTestTime(QString());
        setCoreTestMessage(qFromUtf8("请先启动核心再测试"));
        return;
    }
    probeMode = 0;
    probeStage = 0;
    probeSocksRes = QString();
    probeHttpRes = QString();
    probeSocksKind = 2;
    probeStamp = v2NowStamp();
    setCoreTestChips(2, qFromUtf8("SOCKS 测试中…"),
                     2, qFromUtf8("HTTP 测试中…"));
    setCoreTestTime(QString());
    probeStartPort(0);
}

void V2RayLogic::onTestGstatic() {
    if (probeProc && probeProc->isRunning()) {
        appendLogWithPrefix(qFromUtf8("[测试]"), qFromUtf8("测试进行中，请稍候"));
        return;
    }
    if (!(coreProc && coreProc->isRunning())) {
        setCoreTestMessage(qFromUtf8("请先启动核心再测试"));
        return;
    }
    probeMode = 1;
    probeStage = 0;
    probeSocksRes = QString();
    probeHttpRes = QString();
    probeSocksKind = 2;
    probeStamp = v2NowStamp();
    setCoreTestChips(2, qFromUtf8("SOCKS 测试中…"),
                     2, qFromUtf8("HTTP 测试中…"));
    setCoreTestTime(QString());
    probeStartGstatic(0);
}

void V2RayLogic::onProbeExited() {
    if (!probeProc) { return; }
#ifdef QT3_BUILD
    QByteArray out = probeProc->readStdout();
    QByteArray errOut = probeProc->readStderr();
#else
    QByteArray out = probeProc->readAllStandardOutput();
    QByteArray errOut = probeProc->readAllStandardError();
#endif
    if (probeProc->isRunning()) { probeProc->tryTerminate(); }
    delete probeProc;
    probeProc = 0;
    QString code = v2CurlCodeOf(out);
    bool connected = (code != "000");
    bool running = coreProc && coreProc->isRunning();
    if (probeMode == 0) {
        /* 端口测试：连通=绿勾；运行中未连通=红叉；未运行且空闲=灰线 */
        int sKind = connected ? 0 : (running ? 1 : 2);
        QString sText = connected ? qFromUtf8("SOCKS 可用")
                                  : (running ? qFromUtf8("SOCKS 未监听")
                                             : qFromUtf8("SOCKS 空闲"));
        if (probeStage == 0) {
            probeSocksRes = sText;
            probeSocksKind = sKind;
            probeStage = 1;
            setCoreTestChips(sKind, sText, 2, qFromUtf8("HTTP 测试中…"));
            probeStartPort(1);
            return;
        }
        int hKind = connected ? 0 : (running ? 1 : 2);
        QString hText = connected ? qFromUtf8("HTTP 可用")
                                  : (running ? qFromUtf8("HTTP 未监听")
                                             : qFromUtf8("HTTP 空闲"));
        probeHttpRes = hText;
        setCoreTestChips(sKind, sText, hKind, hText);
        lastTestSocksKind = sKind;
        lastTestSocks = sText;
        lastTestHttpKind = hKind;
        lastTestHttp = hText;
        lastTestMsg = QString();
        setCoreTestTime(probeStamp);   /* 行尾唯一时间 */
        saveTestResult();
        appendLogWithPrefix(qFromUtf8("[端口]"),
            qFromUtf8("SOCKS ") + sText + qFromUtf8(" · HTTP ") + hText);
        return;
    }
    /* gstatic：解析状态码/耗时/错误 → 友好文案（超时/DNS/被拒/TLS…） */
    QString detail = qFromUtf8(out.data(), (int)out.size());
    QString detailErr = qFromUtf8(errOut.data(), (int)errOut.size());
#ifdef QT3_BUILD
    detail = detail.stripWhiteSpace();
    detailErr = detailErr.stripWhiteSpace();
#else
    detail = detail.trimmed();
    detailErr = detailErr.trimmed();
#endif
    if (probeStage == 0) {
        int sk = 1;
        QString sText = v2GstaticLabel(detail, detailErr, "SOCKS", sk);
        probeSocksRes = sText;
        probeSocksKind = sk;
        probeStage = 1;
        setCoreTestChips(sk, sText, 2, qFromUtf8("HTTP 测试中…"));
        probeStartGstatic(1);
        return;
    }
    int hk = 1;
    QString hText = v2GstaticLabel(detail, detailErr, "HTTP", hk);
    probeHttpRes = hText;
    setCoreTestChips(-1, probeSocksRes, hk, hText);   /* kind -1：保留 SOCKS 图标 */
    lastTestSocksKind = probeSocksKind;
    lastTestSocks = probeSocksRes;
    lastTestHttpKind = hk;
    lastTestHttp = hText;
    lastTestMsg = QString();
    setCoreTestTime(probeStamp);   /* 行尾唯一时间 */
    saveTestResult();
    appendLogWithPrefix(qFromUtf8("[测试]"),
        qFromUtf8("gstatic ") + probeSocksRes + qFromUtf8(" · ") + hText);
}


void V2RayLogic::setTestWidgets(QPushButton* tcpSel, QPushButton* realSel,
                                QPushButton* tcpAll, QPushButton* realAll,
                                QPushButton* stopTest) {
    testTcpSelBtn = tcpSel;
    testRealSelBtn = realSel;
    testTcpAllBtn = tcpAll;
    testRealAllBtn = realAll;
    testStopBtn = stopTest;
    if (testStopBtn) { testStopBtn->setEnabled(false); }
}

static void v2SetTestBtns(QPushButton* a, QPushButton* b, QPushButton* c,
                          QPushButton* d, QPushButton* stop, bool busy) {
    if (a) { a->setEnabled(!busy); }
    if (b) { b->setEnabled(!busy); }
    if (c) { c->setEnabled(!busy); }
    if (d) { d->setEnabled(!busy); }
    if (stop) { stop->setEnabled(busy); }
}

void V2RayLogic::onCopySubLink() {
    int idx = selectedSubIndex();
    if (idx < 0) {
        appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("请先选中订阅"));
        return;
    }
    QApplication::clipboard()->setText(subs[idx]);
    appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("已复制订阅链接"));
}

/* 延迟排序：可用优先；“有值”优于“失败/未测”，数值小在前 */
static bool v2LatBetter(int a, int b) {
    bool va = (a > 0);
    bool vb = (b > 0);
    if (va != vb) { return va; }
    if (!va) { return false; }
    return a < b;
}
static bool v2NodeLess(const V2Node& a, const V2Node& b) {
    if (a.supported != b.supported) { return a.supported; }
    if (a.realMs != b.realMs) {
        bool ab = v2LatBetter(a.realMs, b.realMs);
        bool ba = v2LatBetter(b.realMs, a.realMs);
        if (ab != ba) { return ab; }
    }
    if (a.tcpMs != b.tcpMs) {
        bool ab = v2LatBetter(a.tcpMs, b.tcpMs);
        bool ba = v2LatBetter(b.tcpMs, a.tcpMs);
        if (ab != ba) { return ab; }
    }
    return a.name < b.name;
}

void V2RayLogic::onSortByLatency() {
    QString selId;
    int s = selectedNodeIndex();
    if (s >= 0) { selId = nodes[s].id; }
    for (int i = 1; i < (int)nodes.count(); ++i) {
        V2Node key = nodes[i];
        int j = i - 1;
        while (j >= 0 && v2NodeLess(key, nodes[j])) {
            nodes[j + 1] = nodes[j];
            --j;
        }
        nodes[j + 1] = key;
    }
    refreshNodeList();
    int r = indexByNodeId(selId);
    if (r >= 0) { v2TableSetRow(nodeList, r); }
    saveNodes();
    appendLogWithPrefix(qFromUtf8("[节点]"), qFromUtf8("已按延迟排序"));
}

void V2RayLogic::onCleanFailedNodes() {
    V2NodeList keep;
    int removed = 0;
    for (int i = 0; i < (int)nodes.count(); ++i) {
        bool measured = (nodes[i].tcpMs != -2 || nodes[i].realMs != -2);
        bool failed = (nodes[i].realMs == -1) || (nodes[i].tcpMs == -1);
        if (measured && failed) { ++removed; continue; }
        keep.append(nodes[i]);
    }
    nodes = keep;
    refreshNodeList();
    saveNodes();
    appendLogWithPrefix(qFromUtf8("[节点]"),
        qFromUtf8("已清理超时节点：") + QString::number(removed));
}

QString V2RayLogic::nodeId(const V2Node& n) const {
    return n.proto + "|" + n.addr + "|" + n.port + "|" + n.uuid + "|" + n.net + "|" + n.path;
}

int V2RayLogic::indexByNodeId(const QString& id) const {
    if (id.isEmpty()) { return -1; }
    for (int i = 0; i < (int)nodes.count(); ++i) {
        if (nodes[i].id == id) { return i; }
    }
    return -1;
}

void V2RayLogic::appendNodeDedup(const V2Node& n) {
    for (int i = 0; i < (int)nodes.count(); ++i) {
        if (nodes[i].id == n.id) { return; }
    }
    nodes.append(n);
}

void V2RayLogic::removeNodesBySource(const QString& src) {
    V2NodeList keep;
    for (int i = 0; i < (int)nodes.count(); ++i) {
        if (nodes[i].sourceSub != src) { keep.append(nodes[i]); }
    }
    nodes = keep;
}

/* 订阅更新时：保留 正在使用 或 真连成功 的节点，清理未测/失败 */
void V2RayLogic::pruneSubNodesForUpdate(const QString& src) {
    QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
    QString activeId = st.valueStr("activeNode", "");
    V2NodeList keep;
    int kept = 0;
    int removed = 0;
    for (int i = 0; i < (int)nodes.count(); ++i) {
        const V2Node& n = nodes[i];
        if (n.sourceSub != src) { keep.append(n); continue; }
        bool inUse = !activeId.isEmpty() && n.id == activeId;  /* 正在 core 使用 */
        bool usable = (n.realMs > 0);                          /* 真连成功（绿勾） */
        if (inUse || usable) { keep.append(n); ++kept; }
        else { ++removed; }
    }
    nodes = keep;
    if (removed > 0) {
        appendLogWithPrefix(qFromUtf8("[订阅]"),
            qFromUtf8("订阅更新：保留在用/真连可用 ") + QString::number(kept)
            + qFromUtf8(" 个，清理未测/失败 ") + QString::number(removed) + qFromUtf8(" 个"));
    }
}

/* 仅 v2ray-core 能力：REALITY / XTLS / 未实现协议一律不支持 */
bool V2RayLogic::nodeSupportedByCore(const V2Node& n, CoreKind kind) const {
    if (n.proto == "hysteria2") { return false; }
    if (n.proto != "vmess" && n.proto != "vless"
        && n.proto != "trojan" && n.proto != "ss") { return false; }
    if (!n.net.isEmpty() && n.net != "tcp" && n.net != "ws" && n.net != "grpc") { return false; }
    bool reality = (n.security == "reality") || !n.flow.isEmpty();
    if (!reality) { return true; }
    if (kind == CoreV2ray) { return false; }
    if (n.security == "reality" && n.pbk.isEmpty()) { return false; }
    if (n.security == "reality" && coreSourceSystem) {
        int ok = xraySystemVersionOk();
        if (ok == 0) { return false; }
    }
    return true;
}

bool V2RayLogic::nodeSupportedByCore(const V2Node& n) const {
    return nodeSupportedByCore(n, coreKind);
}

void V2RayLogic::recomputeSupport() {
    for (int i = 0; i < (int)nodes.count(); ++i) {
        nodes[i].supported = nodeSupportedByCore(nodes[i], coreKind);
    }
}

void V2RayLogic::loadNodes() {
    nodes.clear();
    bool loaded = false;
    QString text = readUtf8File(baseDir() + "/nodes.json");
    if (!text.isEmpty()) {
        QByteArray tb = qToUtf8(text);
        cJSON* root = cJSON_Parse(tb.data());
        if (root) {
            cJSON* ver = cJSON_GetObjectItem(root, "version");
            cJSON* arr = cJSON_GetObjectItem(root, "nodes");
            if (arr && arr->type == cJSON_Array && (!ver || ver->valueint <= 1)) {
                for (cJSON* it = arr->child; it; it = it->next) {
                    V2Node n;
                    cJSON* j;
#define V2N_LOAD(field, key) \
                    j = cJSON_GetObjectItem(it, key); \
                    if (j && j->valuestring) { n.field = qFromUtf8(j->valuestring); }
                    V2N_LOAD(proto, "proto")
                    V2N_LOAD(name, "name")
                    V2N_LOAD(addr, "addr")
                    V2N_LOAD(port, "port")
                    V2N_LOAD(uuid, "uuid")
                    V2N_LOAD(aid, "aid")
                    V2N_LOAD(scy, "scy")
                    V2N_LOAD(method, "method")
                    V2N_LOAD(net, "net")
                    V2N_LOAD(path, "path")
                    V2N_LOAD(host, "host")
                    V2N_LOAD(serviceName, "serviceName")
                    V2N_LOAD(headerType, "headerType")
                    V2N_LOAD(security, "security")
                    V2N_LOAD(sni, "sni")
                    V2N_LOAD(alpn, "alpn")
                    V2N_LOAD(fp, "fp")
                    V2N_LOAD(pbk, "pbk")
                    V2N_LOAD(sid, "sid")
                    V2N_LOAD(spx, "spx")
                    V2N_LOAD(flow, "flow")
                    V2N_LOAD(sourceSub, "sourceSub")
                    V2N_LOAD(updatedAt, "updatedAt")
                    V2N_LOAD(testedAt, "testedAt")
                    V2N_LOAD(raw, "raw")
                    V2N_LOAD(country, "country")
                    V2N_LOAD(countryName, "countryName")
                    V2N_LOAD(geoSource, "geoSource")
#undef V2N_LOAD
                    j = cJSON_GetObjectItem(it, "allowInsecure");
                    if (j) { n.allowInsecure = (j->valueint != 0); }
                    j = cJSON_GetObjectItem(it, "tcpMs");
                    n.tcpMs = j ? j->valueint : -2;
                    j = cJSON_GetObjectItem(it, "realMs");
                    n.realMs = j ? j->valueint : -2;
                    j = cJSON_GetObjectItem(it, "favorite");
                    n.favorite = j ? (j->valueint != 0) : false;
                    if (n.country.isEmpty()) { applyCountryFromName(n); }
                    n.id = nodeId(n);
                    if (n.id.isEmpty()) { continue; }
                    n.supported = nodeSupportedByCore(n);
                    appendNodeDedup(n);
                }
                loaded = true;
            }
            cJSON_Delete(root);
        } else {
            appendLogWithPrefix(qFromUtf8("[节点]"),
                qFromUtf8("nodes.json 解析失败，已忽略（可重新更新订阅）"));
        }
    }
    if (!loaded && nodes.count() == 0 && fileExists(baseDir() + "/sub_last.txt")) {
        /* 旧用户迁移：用最后一次订阅内容重建一次 */
        QString last = readUtf8File(baseDir() + "/sub_last.txt");
        if (!last.isEmpty()) {
            curSubUrl = subs.count() > 0 ? subs[0] : QString();
            parseSubscription(last, curSubUrl);
            saveNodes();
        }
    }
    emit nodesChanged();
}

void V2RayLogic::saveNodes() {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "version", 1);
    cJSON_AddStringToObject(root, "savedAt",
        qToUtf8(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).data());
    cJSON* arr = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "nodes", arr);
    for (int i = 0; i < (int)nodes.count(); ++i) {
        const V2Node& n = nodes[i];
        cJSON* o = cJSON_CreateObject();
#define V2N_SAVE(field, key) cJSON_AddStringToObject(o, key, qToUtf8(n.field).data());
        V2N_SAVE(proto, "proto")
        V2N_SAVE(name, "name")
        V2N_SAVE(addr, "addr")
        V2N_SAVE(port, "port")
        V2N_SAVE(uuid, "uuid")
        V2N_SAVE(aid, "aid")
        V2N_SAVE(scy, "scy")
        V2N_SAVE(method, "method")
        V2N_SAVE(net, "net")
        V2N_SAVE(path, "path")
        V2N_SAVE(host, "host")
        V2N_SAVE(serviceName, "serviceName")
        V2N_SAVE(headerType, "headerType")
        V2N_SAVE(security, "security")
        V2N_SAVE(sni, "sni")
        V2N_SAVE(alpn, "alpn")
        V2N_SAVE(fp, "fp")
        V2N_SAVE(pbk, "pbk")
        V2N_SAVE(sid, "sid")
        V2N_SAVE(spx, "spx")
        V2N_SAVE(flow, "flow")
        V2N_SAVE(sourceSub, "sourceSub")
        V2N_SAVE(updatedAt, "updatedAt")
        V2N_SAVE(testedAt, "testedAt")
        V2N_SAVE(raw, "raw")
        V2N_SAVE(country, "country")
        V2N_SAVE(countryName, "countryName")
        V2N_SAVE(geoSource, "geoSource")
#undef V2N_SAVE
        cJSON_AddNumberToObject(o, "allowInsecure", n.allowInsecure ? 1 : 0);
        cJSON_AddNumberToObject(o, "tcpMs", n.tcpMs);
        cJSON_AddNumberToObject(o, "realMs", n.realMs);
        cJSON_AddNumberToObject(o, "favorite", n.favorite ? 1 : 0);
        cJSON_AddItemToArray(arr, o);
    }
    char* txt = cJSON_PrintUnformatted(root);
    if (txt) {
        QString tmp = baseDir() + "/nodes.json.tmp";
        writeUtf8File(tmp, qFromUtf8(txt));
        cJSON_free(txt);
        rename(QFile::encodeName(tmp).data(),
               QFile::encodeName(baseDir() + "/nodes.json").data());
    }
    cJSON_Delete(root);
}

void V2RayLogic::saveSettings() {
    if (!bypassCnCheck && !bypassLanCheck && !listenAddrBox
        && !listenSocksEdit && !listenHttpEdit) { return; }
    QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
    if (listenAddrBox || listenSocksEdit || listenHttpEdit) {
        st.setValue("listenAddr", listenAddress());
        int sp = listenSocksPort();
        int hp = listenHttpPort();
        st.setValue("listenSocksPort", sp > 0 ? sp : 10808);
        st.setValue("listenHttpPort", hp > 0 ? hp : 10809);
    }
    if (bypassCnCheck) { st.setValue("bypassCn", bypassCnCheck->isChecked()); }
    if (bypassLanCheck) { st.setValue("bypassLan", bypassLanCheck->isChecked()); }
    st.setValue("sub/autoUpdate", autoUpdateEnabled);
    st.setValue("sub/autoUpdateMin", autoUpdateIntervalMin);
    st.sync();
}

void V2RayLogic::appendLog(const QString& msg) {
    appendLogWithPrefix(QString(), msg);
}

void V2RayLogic::appendLogWithPrefix(const QString& prefix, const QString& msg) {
    if (!log) { return; }
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString line;
    if (prefix.isEmpty()) {
        line = "[" + ts + "] " + msg;
    } else {
        line = "[" + ts + "] " + prefix + " " + msg;
    }
    log->append(line);
    log->append("");
}

int V2RayLogic::selectedSubIndex() {
    if (!subList) { return -1; }
    int idx = v2TableRow(subList);
    if (idx < 0 || idx >= (int)subs.count()) { return -1; }
    return idx;
}
int V2RayLogic::selectedNodeIndex() {
    if (!nodeList) { return -1; }
    int row = v2TableRow(nodeList);
    if (!nodeViewIdx.isEmpty()) {
        if (row < 0 || row >= (int)nodeViewIdx.count()) { return -1; }
        return nodeViewIdx[row];
    }
    if (row < 0 || row >= (int)nodes.count()) { return -1; }
    return row;
}

void V2RayLogic::onAddSubscription() {
    if (!subEdit) { return; }
#ifdef QT3_BUILD
    QString url = subEdit->text().stripWhiteSpace();
#else
    QString url = subEdit->text().trimmed();
#endif
    if (url.isEmpty()) { appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("订阅地址为空")); return; }
    if (sFind(url, ':', 0) <= 0
        || (url.left(7) != "http://" && url.left(8) != "https://")) {
        appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("订阅地址需以 http:// 或 https:// 开头"));
        return;
    }
    for (int i = 0; i < (int)subs.count(); ++i) {
        if (subs[i] == url) { appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("该订阅已存在")); return; }
    }
    subs.append(url);
    saveSubscriptions();
    refreshSubList();
    appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("已添加订阅：") + url);
}
void V2RayLogic::onDelSubscription() {
    int idx = selectedSubIndex();
    if (idx < 0) { appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("请先选中要删除的订阅")); return; }
    appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("已删除订阅：") + subs[idx]);
    removeNodesBySource(subs[idx]);
    saveNodes();
    refreshNodeList();
    subMeta.remove(subs[idx]);
    saveSubMeta();
#ifdef QT3_BUILD
    subs.remove(subs.at(idx));
#else
    subs.removeAt(idx);
#endif
    saveSubscriptions();
    refreshSubList();
}
void V2RayLogic::startSubFetch(const QString& url) {
    if (subProc) { delete subProc; subProc = 0; }
    subData.resize(0);
    subProc = new QProcess34(this);
    QProcess34::connectStdout(subProc, this, SLOT(onSubOutput()));
    QProcess34::connectStderr(subProc, this, SLOT(onSubErrOutput()));
    QProcess34::connectExited(subProc, this, SLOT(onSubFetched()));
    subProc->addArgument("curl");
    subProc->addArgument("-SsL");
    subProc->addArgument("-f");                 /* HTTP >= 400 视为失败 */
    subProc->addArgument("--max-time");
    subProc->addArgument("30");
    subProc->addArgument(url);
    curSubUrl = url;            /* 先定来源，再启动：退出槽里 prune/parse 只用已定的来源 */
    subProc->start();
    setUpdateBusy(true, qFromUtf8("更新中：") + url);
    appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("正在更新：") + url);
}

void V2RayLogic::onUpdateSubscription() {
    if (subProc && subProc->isRunning()) { appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("上一次订阅更新仍在进行中")); return; }
    if (updatingAll) { appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("“更新全部订阅”进行中，请稍候")); return; }
    int idx = selectedSubIndex();
    if (idx < 0) {
        if ((int)subs.count() >= 1) { idx = 0; } else {
            appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("没有可用订阅，请先添加"));
            if (nodeStatus) { nodeStatus->setText(qFromUtf8("状态：没有可用订阅，请先添加")); }
            return;
        }
    }
    startSubFetch(subs[idx]);
}

void V2RayLogic::onUpdateAllSubscriptions() {
    updateAllSubscriptions();
}

void V2RayLogic::updateAllSubscriptions() {
    if (updatingAll) { appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("“更新全部订阅”进行中，请稍候")); return; }
    if (subProc && subProc->isRunning()) { appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("上一次订阅更新仍在进行中，请稍候")); return; }
    if ((int)subs.count() <= 0) { appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("没有可用订阅，请先添加")); return; }
    pendingSubUrls.clear();
    for (int i = 0; i < (int)subs.count(); ++i) { pendingSubUrls.append(subs[i]); }
    pendingSubIndex = 0;
    updatingAll = true;
    appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("开始更新全部订阅，共 ") + QString::number((int)pendingSubUrls.count()) + qFromUtf8(" 条"));
    startNextPendingSub();
}

void V2RayLogic::startNextPendingSub() {
    if (!updatingAll) { return; }
    if (pendingSubIndex >= (int)pendingSubUrls.count()) {
        pendingSubUrls.clear();
        pendingSubIndex = 0;
        updatingAll = false;
        setUpdateBusy(false, qFromUtf8("全部订阅更新完成"));
        appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("全部订阅更新完成"));
        return;
    }
    QString url = pendingSubUrls[pendingSubIndex];
    ++pendingSubIndex;
    startSubFetch(url);
}

void V2RayLogic::onAutoUpdateTick() {
    if (!autoUpdateEnabled) { return; }
    if ((int)subs.count() <= 0) { return; }
    if (subProc && subProc->isRunning()) { return; }
    if (updatingAll) { return; }
    appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("定时自动更新订阅（每 ") + QString::number(autoUpdateIntervalMin) + qFromUtf8(" 分钟）"));
    updateAllSubscriptions();
}

void V2RayLogic::onSubOutput() {
    if (!subProc) { return; }
    baAppend(subData, subProc->readStdout());
}
void V2RayLogic::onSubErrOutput() {
    if (!subProc) { return; }
    QByteArray e = subProc->readStderr();
    if ((int)e.size() <= 0) { return; }
    QString es = lastNeedleLine(qFromUtf8(e.data(), (int)e.size()), "curl:");
    if (!es.isEmpty()) {
        appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("下载出错：") + es);
    }
}
void V2RayLogic::onSubFetched() {
    if (!subProc) { return; }
    bool fetchOk = true;
#ifdef QT3_BUILD
    fetchOk = subProc->normalExit();
#else
    fetchOk = (subProc->exitStatus() == QProcess::NormalExit)
        && (subProc->exitCode() == 0);
#endif
    QString src = curSubUrl;         /* 本次抓取的来源快照（之后只用它） */
    int exitCode = -1;
#ifdef QT3_BUILD
    if (subProc->normalExit()) { exitCode = subProc->exitStatus(); }
#else
    if (subProc->exitStatus() == QProcess::NormalExit) { exitCode = subProc->exitCode(); }
#endif
    baAppend(subData, subProc->readStdout());
    delete subProc;
    subProc = 0;
    QString content = qFromUtf8(subData.data(), (int)subData.size());
    if (!fetchOk || content.isEmpty()) {
        appendLogWithPrefix(qFromUtf8("[订阅]"),
            qFromUtf8("更新失败（网络/HTTP 错误，curl 退出码 ")
            + QString::number(exitCode) + qFromUtf8("），已保留原节点列表：") + src);
        if (!updatingAll) { setUpdateBusy(false, qFromUtf8("更新失败（已保留原节点）")); }
        if (updatingAll) { QTimer::singleShot(0, this, SLOT(startNextPendingSub())); }
        return;
    }
    QString plain = maybeDecodeBase64(content);
    if (!hasShareScheme(plain)) {
        appendLogWithPrefix(qFromUtf8("[订阅]"),
            qFromUtf8("订阅内容未发现节点链接（可能不是有效订阅），已保留原节点列表"));
        if (!updatingAll) { setUpdateBusy(false, qFromUtf8("更新失败（内容无效，已保留原节点）")); }
        if (updatingAll) { QTimer::singleShot(0, this, SLOT(startNextPendingSub())); }
        return;
    }
    /* C：记录原选中节点 id（跨订阅更新也能恢复） */
    QString prevId;
    int prevIdx = selectedNodeIndex();
    if (prevIdx >= 0) { prevId = nodes[prevIdx].id; }
    saveSubLast(plain);
    pruneSubNodesForUpdate(src);       /* 保留在用/真连可用，清理未测/失败 */
    parseSubscription(plain, src);
    refreshNodeList();
    {
        QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
        QString activeId = st.valueStr("activeNode", "");
        bool coreRunning = coreProc && coreProc->isRunning();
        /* core 运行时固定选中“在用节点”，且不改写 activeNode */
        QString wantSel = coreRunning ? activeId : prevId;
        int restore = indexByNodeId(wantSel);
        if (restore < 0 && nodes.count() > 0) { restore = 0; }
        if (restore >= 0 && nodeList) {
            v2TableSetRow(nodeList, restore);
            if (!coreRunning) {
                st.setValue("activeNode", nodes[restore].id);
                st.sync();
            }
        }
    }
    saveNodes();
    startGeoLookup(true);
    /* A：记录该订阅的更新时间/节点数 */
    int subNodes = 0;
    for (int k = 0; k < (int)nodes.count(); ++k) {
        if (nodes[k].sourceSub == src) { ++subNodes; }
    }
    subMeta.insert(src,
        QTime::currentTime().toString("hh:mm:ss") + "|" + QString::number(subNodes));
    saveSubMeta();
    refreshSubList();
    appendLogWithPrefix(qFromUtf8("[订阅]"),
        qFromUtf8("更新完成：该订阅 ") + QString::number(subNodes)
        + qFromUtf8(" 个节点，列表共 ") + QString::number((int)nodes.count()) + qFromUtf8(" 个"));
    if (!updatingAll) {
        setUpdateBusy(false, qFromUtf8("更新完成：") + QString::number((int)nodes.count()) + qFromUtf8(" 个节点（列表共）"));
    }
    if (updatingAll) { QTimer::singleShot(0, this, SLOT(startNextPendingSub())); }
}

/* 订阅内容若是 base64 整包则解码；含分享链接明文则原样返回 */
static bool hasShareScheme(const QString& s) {
    return sFindStr(s, "vmess://", 0) >= 0 || sFindStr(s, "vless://", 0) >= 0
        || sFindStr(s, "trojan://", 0) >= 0 || sFindStr(s, "ss://", 0) >= 0;
}

QString V2RayLogic::maybeDecodeBase64(const QString& in) {
    if (hasShareScheme(in)) { return in; }
    QByteArray b = qToUtf8(in);
    QByteArray dec = base64Decode(std::string(b.data(), b.size()));
    if ((int)dec.size() <= 0) { return in; }
    QString s = qFromUtf8(dec.data(), (int)dec.size());
    if (hasShareScheme(s)) { return s; }
    return in;
}

void V2RayLogic::clearNodes() {
    nodes.clear();
}

/* ── 国家/地区（名称优先，GeoIP 兜底） ── */
struct V2GeoEntry { const char* key; const char* cc; const char* cn; };
static const V2GeoEntry kV2GeoTable[] = {
    {"美国","US","美国"},{"US","US","美国"},{"United States","US","美国"},
    {"香港","HK","香港"},{"HK","HK","香港"},
    {"台湾","TW","台湾"},{"TW","TW","台湾"},
    {"日本","JP","日本"},{"JP","JP","日本"},{"Japan","JP","日本"},
    {"新加坡","SG","新加坡"},{"SG","SG","新加坡"},
    {"韩国","KR","韩国"},{"KR","KR","韩国"},
    {"英国","GB","英国"},{"UK","GB","英国"},{"GB","GB","英国"},
    {"德国","DE","德国"},{"DE","DE","德国"},
    {"法国","FR","法国"},{"FR","FR","法国"},
    {"荷兰","NL","荷兰"},{"NL","NL","荷兰"},
    {"俄罗斯","RU","俄罗斯"},{"RU","RU","俄罗斯"},
    {"乌克兰","UA","乌克兰"},{"UA","UA","乌克兰"},
    {"罗马尼亚","RO","罗马尼亚"},{"RO","RO","罗马尼亚"},
    {"立陶宛","LT","立陶宛"},{"LT","LT","立陶宛"},
    {"瑞典","SE","瑞典"},{"SE","SE","瑞典"},
    {"挪威","NO","挪威"},{"NO","NO","挪威"},
    {"芬兰","FI","芬兰"},{"FI","FI","芬兰"},
    {"丹麦","DK","丹麦"},{"DK","DK","丹麦"},
    {"波兰","PL","波兰"},{"PL","PL","波兰"},
    {"捷克","CZ","捷克"},{"CZ","CZ","捷克"},
    {"奥地利","AT","奥地利"},{"AT","AT","奥地利"},
    {"瑞士","CH","瑞士"},{"CH","CH","瑞士"},
    {"意大利","IT","意大利"},{"IT","IT","意大利"},
    {"西班牙","ES","西班牙"},{"ES","ES","西班牙"},
    {"葡萄牙","PT","葡萄牙"},{"PT","PT","葡萄牙"},
    {"希腊","GR","希腊"},{"GR","GR","希腊"},
    {"土耳其","TR","土耳其"},{"TR","TR","土耳其"},
    {"以色列","IL","以色列"},{"IL","IL","以色列"},
    {"伊拉克","IQ","伊拉克"},{"IQ","IQ","伊拉克"},
    {"伊朗","IR","伊朗"},{"IR","IR","伊朗"},
    {"沙特","SA","沙特"},{"SA","SA","沙特"},
    {"巴基斯坦","PK","巴基斯坦"},{"PK","PK","巴基斯坦"},
    {"孟加拉","BD","孟加拉"},{"BD","BD","孟加拉"},
    {"柬埔寨","KH","柬埔寨"},{"KH","KH","柬埔寨"},
    {"缅甸","MM","缅甸"},{"MM","MM","缅甸"},
    {"白俄罗斯","BY","白俄罗斯"},{"BY","BY","白俄罗斯"},
    {"塞尔维亚","RS","塞尔维亚"},{"RS","RS","塞尔维亚"},
    {"匈牙利","HU","匈牙利"},{"HU","HU","匈牙利"},
    {"保加利亚","BG","保加利亚"},{"BG","BG","保加利亚"},
    {"斯洛伐克","SK","斯洛伐克"},{"SK","SK","斯洛伐克"},
    {"斯洛文尼亚","SI","斯洛文尼亚"},{"SI","SI","斯洛文尼亚"},
    {"克罗地亚","HR","克罗地亚"},{"HR","HR","克罗地亚"},
    {"爱尔兰","IE","爱尔兰"},{"IE","IE","爱尔兰"},
    {"冰岛","IS","冰岛"},{"IS","IS","冰岛"},
    {"比利时","BE","比利时"},{"BE","BE","比利时"},
    {"卢森堡","LU","卢森堡"},{"LU","LU","卢森堡"},
    {"爱沙尼亚","EE","爱沙尼亚"},{"EE","EE","爱沙尼亚"},
    {"拉脱维亚","LV","拉脱维亚"},{"LV","LV","拉脱维亚"},
    {"亚美尼亚","AM","亚美尼亚"},{"AM","AM","亚美尼亚"},
    {"格鲁吉亚","GE","格鲁吉亚"},{"GE","GE","格鲁吉亚"},
    {"阿塞拜疆","AZ","阿塞拜疆"},{"AZ","AZ","阿塞拜疆"},
    {"乌兹别克斯坦","UZ","乌兹别克斯坦"},{"UZ","UZ","乌兹别克斯坦"},
    {"秘鲁","PE","秘鲁"},{"PE","PE","秘鲁"},
    {"委内瑞拉","VE","委内瑞拉"},{"VE","VE","委内瑞拉"},
    {"乌拉圭","UY","乌拉圭"},{"UY","UY","乌拉圭"},
    {"摩洛哥","MA","摩洛哥"},{"MA","MA","摩洛哥"},
    {"尼日利亚","NG","尼日利亚"},{"NG","NG","尼日利亚"},
    {"肯尼亚","KE","肯尼亚"},{"KE","KE","肯尼亚"},
    {"阿联酋","AE","阿联酋"},{"AE","AE","阿联酋"},
    {"哈萨克斯坦","KZ","哈萨克斯坦"},{"KZ","KZ","哈萨克斯坦"},
    {"印度","IN","印度"},{"IN","IN","印度"},
    {"泰国","TH","泰国"},{"TH","TH","泰国"},
    {"越南","VN","越南"},{"VN","VN","越南"},
    {"印尼","ID","印尼"},{"ID","ID","印尼"},
    {"菲律宾","PH","菲律宾"},{"PH","PH","菲律宾"},
    {"马来西亚","MY","马来西亚"},{"MY","MY","马来西亚"},
    {"加拿大","CA","加拿大"},{"CA","CA","加拿大"},
    {"墨西哥","MX","墨西哥"},{"MX","MX","墨西哥"},
    {"巴西","BR","巴西"},{"BR","BR","巴西"},
    {"阿根廷","AR","阿根廷"},{"AR","AR","阿根廷"},
    {"智利","CL","智利"},{"CL","CL","智利"},
    {"哥伦比亚","CO","哥伦比亚"},{"CO","CO","哥伦比亚"},
    {"澳大利亚","AU","澳大利亚"},{"AU","AU","澳大利亚"},
    {"新西兰","NZ","新西兰"},{"NZ","NZ","新西兰"},
    {"南非","ZA","南非"},{"ZA","ZA","南非"},
    {"埃及","EG","埃及"},{"EG","EG","埃及"},
    {0,0,0}
};

static bool v2IsLanAddr(const QString& a) {
    if (a == "::1" || a.left(3) == "127" || a.left(3) == "10.") { return true; }
    if (a.left(8) == "192.168.") { return true; }
    if (a.left(4) == "172.") {
        bool ok = false;
        int second = a.mid(4, 2).toInt(&ok);
        if (ok && second >= 16 && second <= 31) { return true; }
    }
    return false;
}

/* 名称里的国旗 emoji（🇺🇸 等，QString 为代理对） */
static bool v2FlagInName(const QString& name, QString& cc) {
    for (int i = 0; i + 3 < (int)name.length(); ++i) {
        if ((unsigned short)name[i].unicode() == 0xD83C
            && (unsigned short)name[i+1].unicode() >= 0xDDE6
            && (unsigned short)name[i+1].unicode() <= 0xDDFF
            && (unsigned short)name[i+2].unicode() == 0xD83C
            && (unsigned short)name[i+3].unicode() >= 0xDDE6
            && (unsigned short)name[i+3].unicode() <= 0xDDFF) {
            unsigned short a = (unsigned short)(name[i+1].unicode() - 0xDDE6 + 'A');
            unsigned short b = (unsigned short)(name[i+3].unicode() - 0xDDE6 + 'A');
            cc = QString(QChar(a)) + QString(QChar(b));
            return true;
        }
    }
    return false;
}

QString V2RayLogic::countryNameOf(const QString& cc) const {
    for (int i = 0; kV2GeoTable[i].key; ++i) {
        if (cc == kV2GeoTable[i].cc) { return qFromUtf8(kV2GeoTable[i].cn); }
    }
    return QString();
}

void V2RayLogic::applyCountryFromName(V2Node& n) const {
    if (!n.country.isEmpty()) { return; }
    if (v2IsLanAddr(n.addr)) {
        n.country = "LAN";
        n.countryName = qFromUtf8("本机/局域网");
        n.geoSource = "lan";
        return;
    }
    QString cc;
    if (v2FlagInName(n.name, cc)) {
        n.country = cc;
        n.countryName = countryNameOf(cc);
        n.geoSource = "emoji";
        return;
    }
    for (int i = 0; kV2GeoTable[i].key; ++i) {
        QString key = qFromUtf8(kV2GeoTable[i].key);
        int pos = sFindQ(n.name, key, 0);
        if (pos < 0) { continue; }
        if (key.length() == 2) {   /* 代码需词边界，避免 utiltools 命中 US */
            bool leftOk = (pos == 0) || !n.name[pos-1].isLetterOrNumber();
            bool rightOk = (pos + 2 >= (int)n.name.length())
                || !n.name[pos+2].isLetterOrNumber();
            if (!leftOk || !rightOk) { continue; }
        }
        n.country = kV2GeoTable[i].cc;
        n.countryName = qFromUtf8(kV2GeoTable[i].cn);
        n.geoSource = "name";
        return;
    }
}

void V2RayLogic::parseSubscription(const QString& content, const QString& src) {
    QStringList lines = splitLines(content);
    QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    for (int i = 0; i < (int)lines.count(); ++i) {
#ifdef QT3_BUILD
        QString line = lines[i].stripWhiteSpace();
#else
        QString line = lines[i].trimmed();
#endif
        if (line.isEmpty()) { continue; }
        bool ok = false;
        QString err;
        V2Node node;
        if (line.left(8) == "vmess://") {
            node.proto = "vmess";
            parseVmess(line, node, ok, err);
        } else if (line.left(8) == "vless://") {
            node.proto = "vless";
            parseVlessTrojan(line, node, ok, err);
        } else if (line.left(9) == "trojan://") {
            node.proto = "trojan";
            parseVlessTrojan(line, node, ok, err);
        } else if (line.left(5) == "ss://") {
            parseShadowsocks(line, node, ok, err);
        } else if (line.left(12) == "hysteria2://") {
            node.proto = "hysteria2";
            node.raw = line;
            int at = sFind(line, '@', 12);
            int hash = sRFind(line, '#', -1);
            QString hostport;
            if (at > 0) {
                node.uuid = line.mid(12, at - 12);
                int q = sFind(line, '?', at);
                int end = (q >= 0) ? q : ((hash >= 0) ? hash : (int)line.length());
                hostport = line.mid(at + 1, end - at - 1);
            } else {
                hostport = line.mid(12);
            }
            int colon = sRFind(hostport, ':', -1);
            if (colon > 0) {
                node.addr = hostport.left(colon);
                node.port = hostport.mid(colon + 1);
            }
            node.name = (hash >= 0) ? v2UrlDecode(line.mid(hash + 1), true)
                                    : (node.addr + ":" + node.port);
            node.supported = false;
            node.id = nodeId(node);
            node.sourceSub = src;
            node.updatedAt = now;
            appendNodeDedup(node);
            appendLog(qFromUtf8("提示：hysteria2 节点暂不支持启动（已保留展示）"));
            continue;
        } else {
            continue;
        }
        if (ok) {
            applyCountryFromName(node);
            node.sourceSub = src;
            node.updatedAt = now;
            appendNodeDedup(node);
            continue;
        }
        appendLog(qFromUtf8("节点解析失败（") + node.proto + qFromUtf8("）：") + err);
    }
}

void V2RayLogic::parseShadowsocks(const QString& line, V2Node& node, bool& ok, QString& err) {
    ok = false;
    node.proto = "ss";
    node.raw = line;
    QString body = line.mid(5);                 /* 去 "ss://" */
    int hash = sRFind(body, '#', -1);
    if (hash >= 0) {
        node.name = body.mid(hash + 1);
        body = body.left(hash);
    }
    int qmark = sFind(body, '?', 0);            /* 忽略插件参数 */
    if (qmark >= 0) { body = body.left(qmark); }
    int at = sFind(body, '@', 0);
    QString userinfo;
    QString hostport;
    if (at >= 0) {
        userinfo = body.left(at);
        hostport = body.mid(at + 1);
    } else {
        QByteArray b = qToUtf8(body);
        QByteArray dec = base64Decode(std::string(b.data(), b.size()));
        if ((int)dec.size() <= 0) { err = qFromUtf8("base64 解码失败"); return; }
        QString plain = qFromUtf8(dec.data(), (int)dec.size());
        int a2 = sFind(plain, '@', 0);
        if (a2 < 0) { err = qFromUtf8("缺少 @"); return; }
        userinfo = plain.left(a2);
        hostport = plain.mid(a2 + 1);
    }
    int colon = sRFind(hostport, ':', -1);
    if (colon <= 0) { err = qFromUtf8("缺少端口"); return; }
    node.addr = hostport.left(colon);
    node.port = hostport.mid(colon + 1);
    int sep = sFind(userinfo, ':', 0);
    if (sep <= 0) {
        /* SIP002：userinfo 本身是 base64(method:password) */
        QByteArray ub = qToUtf8(userinfo);
        QByteArray ud = base64Decode(std::string(ub.data(), ub.size()));
        if ((int)ud.size() > 0) {
            userinfo = qFromUtf8(ud.data(), (int)ud.size());
            sep = sFind(userinfo, ':', 0);
        }
    }
    if (sep <= 0) { err = qFromUtf8("缺少 method:password"); return; }
    node.method = userinfo.left(sep);
    node.uuid = userinfo.mid(sep + 1);
    if (node.name.isEmpty()) { node.name = node.addr + ":" + node.port; }
    node.name = v2UrlDecode(node.name, true);
    node.id = nodeId(node);
    node.supported = true;
    ok = true;
}


void V2RayLogic::parseVmess(const QString& line, V2Node& node, bool& ok, QString& err) {
    ok = false;
    /* base64 解码复用 qlcomp（qltox 同款，Qt3/Qt4 双端实现） */
    QByteArray b = qToUtf8(line.mid(8));
    QByteArray raw = base64Decode(std::string(b.data(), b.size()));
    if ((int)raw.size() <= 0) { err = qFromUtf8("base64 解码失败"); return; }
    QByteArray z(raw);
    z.resize((int)z.size() + 1);
    z[(int)z.size() - 1] = 0;
    cJSON* root = cJSON_Parse(z.data());
    if (!root) { err = qFromUtf8("JSON 解析失败"); return; }
    cJSON* j;
    j = cJSON_GetObjectItem(root, "add");
    if (j && j->valuestring) { node.addr = qFromUtf8(j->valuestring); }
    j = cJSON_GetObjectItem(root, "port");
    if (j) {
        if (j->type == cJSON_String && j->valuestring) {
            node.port = j->valuestring;
        } else {
            node.port = QString::number(j->valueint);
        }
    }
    j = cJSON_GetObjectItem(root, "id");
    if (j && j->valuestring) { node.uuid = j->valuestring; }
    j = cJSON_GetObjectItem(root, "aid");
    if (j) {
        node.aid = (j->type == cJSON_String && j->valuestring)
            ? QString(j->valuestring) : QString::number(j->valueint);
    }
    j = cJSON_GetObjectItem(root, "scy");
    if (j && j->valuestring) { node.scy = j->valuestring; }
    j = cJSON_GetObjectItem(root, "net");
    if (j && j->valuestring) { node.net = j->valuestring; }
    j = cJSON_GetObjectItem(root, "path");
    if (j && j->valuestring) { node.path = j->valuestring; }
    j = cJSON_GetObjectItem(root, "host");
    if (j && j->valuestring) { node.host = j->valuestring; }
    j = cJSON_GetObjectItem(root, "tls");
    if (j && j->valuestring) { node.security = j->valuestring; }
    j = cJSON_GetObjectItem(root, "sni");
    if (j && j->valuestring) { node.sni = j->valuestring; }
    j = cJSON_GetObjectItem(root, "alpn");
    if (j && j->valuestring) { node.alpn = j->valuestring; }
    j = cJSON_GetObjectItem(root, "ps");
    if (j && j->valuestring) { node.name = j->valuestring; }
    cJSON_Delete(root);
    node.raw = line;
    if (node.addr.isEmpty()) { err = qFromUtf8("缺少 add"); return; }
    if (node.port.isEmpty()) { err = qFromUtf8("缺少 port"); return; }
    if (node.uuid.isEmpty()) { err = qFromUtf8("缺少 id"); return; }
    if (node.name.isEmpty()) { node.name = node.addr + ":" + node.port; }
    node.name = v2UrlDecode(node.name, true);
    node.path = v2UrlDecode(node.path, true);
    node.host = v2UrlDecode(node.host, false);
    node.sni  = v2UrlDecode(node.sni, false);
    node.id = nodeId(node);
    node.supported = nodeSupportedByCore(node);
    ok = true;
}

void V2RayLogic::parseVlessTrojan(const QString& line, V2Node& node, bool& ok, QString& err) {
    ok = false;
    node.raw = line;
    int schemePos = sFindStr(line, "://", 0);
    if (schemePos < 0) { err = qFromUtf8("非法链接"); return; }
    int schemeEnd = schemePos + 3;
    int at = sFind(line, '@', schemeEnd);
    if (at < 0) { err = qFromUtf8("缺少 @"); return; }
    node.uuid = line.mid(schemeEnd, at - schemeEnd);   /* 修复：原实现整体错位 1 字节 */
    /* host:port 段 */
    int slash = sFind(line, '/', at);
    int qmark = sFind(line, '?', at);
    int hash = sRFind(line, '#', -1);
    int end = -1;
    if (slash >= 0 && (qmark < 0 || slash < qmark)) { end = slash; }
    else if (qmark >= 0) { end = qmark; }
    if (end < 0) { end = (int)line.length(); }
    QString hostport = line.mid(at + 1, end - at - 1);
    int lastColon = sRFind(hostport, ':', -1);
    if (lastColon < 0) { err = qFromUtf8("缺少端口"); return; }
    node.addr = hostport.left(lastColon);
    node.port = hostport.mid(lastColon + 1);
    /* 备注 */
    if (hash >= 0 && hash + 1 < (int)line.length()) { node.name = line.mid(hash + 1); }
    /* 查询参数 */
    if (qmark >= 0) {
        int qend = (hash >= 0 ? hash : (int)line.length());
        QString q = line.mid(qmark + 1, qend - qmark - 1);
#ifdef QT3_BUILD
        QStringList kv = QStringList::split('&', q);
#else
        QStringList kv = q.split(QChar('&'));
#endif
        for (int i = 0; i < (int)kv.count(); ++i) {
            int eq = sFind(kv[i], '=', 0);
            if (eq < 0) { continue; }
            QString key = kv[i].left(eq);
            QString val = kv[i].mid(eq + 1);
            if (key == "type") { node.net = val; }
            else if (key == "path") { node.path = val; }
            else if (key == "host") { node.host = val; }
            else if (key == "security") { node.security = val; }
            else if (key == "sni") { node.sni = val; }
            else if (key == "alpn") { node.alpn = val; }
            else if (key == "flow") { node.flow = val; }
            else if (key == "pbk") { node.pbk = val; }
            else if (key == "sid") { node.sid = val; }
            else if (key == "fp") { node.fp = val; }
            else if (key == "spx") { node.spx = val; }
            else if (key == "insecure") { node.allowInsecure = (val == "1" || val == "true"); }
            else if (key == "serviceName") { node.serviceName = val; }
            else if (key == "headerType") { node.headerType = val; }
            else { /* 忽略 */ }
        }
    }
    /* 简单校验 */
    bool portOk = false;
    node.port.toInt(&portOk);
    if (!portOk) { err = qFromUtf8("端口非法"); return; }
    if (node.addr.isEmpty()) { err = qFromUtf8("缺少地址"); return; }
    if (node.uuid.isEmpty()) { err = qFromUtf8("缺少凭据"); return; }
    if (node.name.isEmpty()) { node.name = node.addr + ":" + node.port; }
    node.name = v2UrlDecode(node.name, true);
    node.path = v2UrlDecode(node.path, true);
    node.host = v2UrlDecode(node.host, false);
    node.sni  = v2UrlDecode(node.sni, false);
    node.id = nodeId(node);
    node.supported = nodeSupportedByCore(node);
    ok = true;
}

/* ── 核心 ── */
void V2RayLogic::onDownloadCore() {
    if (coreDlBusy) { appendLog(qFromUtf8("正在下载核心，请稍候")); return; }
    if (coreProc && coreProc->isRunning()) {
        /* C：核心运行中不覆盖可执行文件（ETXTBSY），先请求停止 */
        appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("核心正在运行：已发送停止请求，请稍后再点“下载安装”"));
        onStopCore();
        return;
    }
    if (coreKind == CoreXray && coreSourceSystem) {
        appendLogWithPrefix(qFromUtf8("[核心下载]"),
            qFromUtf8("当前使用系统 Xray，无需下载；如需内置版请切换内核"));
        return;
    }
    QFile::remove(zipPath());   /* C：清掉上次残留的半截包，保证本次下载/回退判定准确 */
    dlCoreKind = (coreKind == CoreXray) ? 1 : 0;
    dlZipName = v2ZipName(coreKind);
    dlTry = 0;
    startCoreDownload();
}

void V2RayLogic::startCoreDownload() {
    const char* proxy = (dlCoreKind == 1) ? XCORE_GH_PREFIX : V2CORE_GH_PREFIX;
    const char* direct = (dlCoreKind == 1) ? XCORE_GH_DIRECT : V2CORE_GH_DIRECT;
    QString url = QString(dlTry == 0 ? proxy : direct) + dlZipName;
    ensureDirs();
    coreDlBusy = true;
    setCoreDownloadBusy(true);
    setCoreMessage(dlTry == 0 ? qFromUtf8("状态：正在下载核心（gh-proxy）…")
                              : qFromUtf8("状态：正在下载核心（直连 GitHub）…"));
    dlProgressPct = -1;
    dlErrData.resize(0);
    if (dlProc) { delete dlProc; dlProc = 0; }
    dlProc = new QProcess34(this);
    QProcess34::connectStdout(dlProc, this, SLOT(onCoreDlOutput()));
    QProcess34::connectStderr(dlProc, this, SLOT(onCoreDlErrOutput()));
    QProcess34::connectExited(dlProc, this, SLOT(onCoreDlDone()));
    dlProc->addArgument("curl");
    dlProc->addArgument("-S");
    dlProc->addArgument("-#");
    dlProc->addArgument("-L");
    dlProc->addArgument("-o");
    dlProc->addArgument(zipPath());
    dlProc->addArgument("--max-time");
    dlProc->addArgument("120");
    dlProc->addArgument(url);
    dlProc->start();
    appendLogWithPrefix(qFromUtf8("[核心下载]"),
        qFromUtf8("正在下载 v2ray-core ") + qFromUtf8(V2CORE_VERSION)
        + (dlTry == 0 ? qFromUtf8("（gh-proxy.com 加速）") : qFromUtf8("（直连 GitHub）")));
    appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("URL：") + url);
}
void V2RayLogic::onCoreDlOutput() {
    /* 下载内容已写入文件，stdout 无需展示 */
}
void V2RayLogic::onCoreDlErrOutput() {
    if (!dlProc) { return; }
#ifdef QT3_BUILD
    QByteArray chunk = dlProc->readStderr();
#else
    QByteArray chunk = dlProc->readAllStandardError();
#endif
    if ((int)chunk.size() <= 0) { return; }
    int oldLen = (int)dlErrData.size();
    baAppend(dlErrData, chunk);
    /* 新增区域（回退一点避免跨块截断）标记 curl 错误行 */
    QString added;
    if (oldLen < (int)dlErrData.size()) {
        int from = oldLen > 16 ? oldLen - 16 : 0;
        added = qFromUtf8(dlErrData.data() + from, (int)dlErrData.size() - from);
    } else {
        added = tailStr(chunk, 4096);
    }
    QString errLine = lastNeedleLine(added, "curl:");
    if (!errLine.isEmpty()) {
        appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("curl 报错：") + errLine);
    }
    /* 进度百分比（curl -# 进度条输出形如 #####  37.5%） */
    QString tail = tailStr(dlErrData, 4096);
    int pct = extractLastPercent(tail);
    if (pct >= 0) {
        if (dlProgressPct < 0 || pct >= dlProgressPct + 10) {
            dlProgressPct = pct;
            appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("下载进度约 ") + QString::number(pct) + qFromUtf8("%"));
            setCoreMessage(qFromUtf8("状态：正在下载核心 … ")
                + QString::number(pct) + qFromUtf8("%"));
        }
    }
}
void V2RayLogic::onCoreDlDone() {
    if (!dlProc) { return; }
    delete dlProc;
    dlProc = 0;
    coreDlBusy = false;
    if (!fileExists(zipPath())) {
        if (dlTry == 0) {
            dlTry = 1;
            appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("gh-proxy 下载失败（未生成 zip），改用直连 GitHub 重试"));
            startCoreDownload();
            return;
        }
        appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("下载失败（未生成 zip 文件），请检查网络或 gh-proxy.com 可用性"));
        setCoreMessage(qFromUtf8("状态：核心下载失败（详见日志）"));
        setCoreDownloadBusy(false);
        refreshCoreState();
        return;
    }
    /* 文件大小过小视为下载不完整 */
    QFileInfo fi(zipPath());
    if ((long)fi.size() < 1024 * 1024) {
        QFile::remove(zipPath());
        if (dlTry == 0) {
            dlTry = 1;
            appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("gh-proxy 下载结果异常（zip 小于 1MB），改用直连 GitHub 重试"));
            startCoreDownload();
            return;
        }
        appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("下载结果异常（zip 小于 1MB），可能下载不完整，已删除重试入口"));
        setCoreMessage(qFromUtf8("状态：核心下载异常（详见日志）"));
        setCoreDownloadBusy(false);
        refreshCoreState();
        return;
    }
    appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("下载完成（进度 100%），正在解压..."));
    setCoreMessage(qFromUtf8("状态：正在解压安装…"));
    if (unzipProc) { delete unzipProc; unzipProc = 0; }
    unzipProc = new QProcess34(this);
    QProcess34::connectExited(unzipProc, this, SLOT(onCoreUnzipDone()));
    unzipProc->addArgument("unzip");
    unzipProc->addArgument("-o");
    unzipProc->addArgument(zipPath());
    unzipProc->addArgument("-d");
    unzipProc->addArgument(coreWorkDir());
    unzipProc->start();
}
void V2RayLogic::onCoreUnzipDone() {
    if (!unzipProc) { return; }
#ifdef QT3_BUILD
    bool unzipOk = unzipProc->normalExit();
#else
    bool unzipOk = (unzipProc->exitStatus() == QProcess::NormalExit)
        && (unzipProc->exitCode() == 0);
#endif
    delete unzipProc;
    unzipProc = 0;
    chmod(QFile::encodeName(coreBinaryPath()).data(), 0755);
    QFile::remove(zipPath());
    refreshCoreState();
    if (!unzipOk && !coreInstalled()) {
        appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("解压失败（unzip 退出异常），请确认系统已安装 unzip 后重试"));
    }
    if (coreInstalled()) {
        if (hasGeoFiles()) {
            appendLogWithPrefix(qFromUtf8("[核心下载]"), coreDisplayName() + qFromUtf8(" 安装完成（geoip.dat / geosite.dat 已就绪）"));
        } else {
            appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("v2ray-core 安装完成，但缺少 geoip.dat / geosite.dat"));
        }
        setCoreDownloadBusy(false);
    } else {
        if (dlTry == 0) {
            dlTry = 1;
            appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("解压/校验失败，改用直连 GitHub 重试"));
            startCoreDownload();
            return;
        }
        appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("v2ray-core 安装失败（未找到 v2ray 可执行文件）"));
        setCoreMessage(qFromUtf8("状态：核心安装失败（详见日志）"));
        setCoreDownloadBusy(false);
    }
}

/* ── 实时流量：stats api 轮询 + access.log 请求数 ── */
void V2RayLogic::resetTrafficCounters() {
    /* 核心重启后 counters 从 0 开始 */
    trafficUpBase = 0;
    trafficDownBase = 0;
    trafficUpLast = 0;
    trafficDownLast = 0;
    trafficUpRate = 0;
    trafficDownRate = 0;
    /* 累计流量：读上次会话保存的总值作为本次基线 */
    {
        QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
        /* 以字符串存取，避免 32 位 int 溢出 */
        totalUpBase = st.valueStr("core/totalUp", "0").toLongLong();
        totalDownBase = st.valueStr("core/totalDown", "0").toLongLong();
    }
    totalUp = totalUpBase;
    totalDown = totalDownBase;
    totalFlushTick = 0;
    /* access.log 跨运行追加：以当前行数为基线 */
    reqCountBase = v2CountLines(baseDir() + "/access.log");
    reqCountLast = 0;
    refreshTrafficLabel();
}

void V2RayLogic::startTrafficPoll() {
    if (trafficTimer) { trafficTimer->stop(); }
    if (!trafficTimer) {
        trafficTimer = new QTimer(this);
        connect(trafficTimer, SIGNAL(timeout()), this, SLOT(onTrafficPoll()));
    }
    trafficTimer->start(2000);
}

void V2RayLogic::stopTrafficPoll() {
    if (trafficTimer) { trafficTimer->stop(); }
    if (statsProc) {
        if (statsProc->isRunning()) { statsProc->tryTerminate(); }
        delete statsProc;
        statsProc = 0;
    }
    apiPort = 0;
    trafficUpRate = 0;
    trafficDownRate = 0;
    refreshTrafficLabel();
}

void V2RayLogic::onTrafficPoll() {
    if (!(coreProc && coreProc->isRunning())) { stopTrafficPoll(); return; }
    if (apiPort <= 0) { return; }
    if (statsProc && statsProc->isRunning()) { return; }
    if (statsProc) { delete statsProc; }
    statsProc = new QProcess34(this);
    QProcess34::connectExited(statsProc, this, SLOT(onStatsExited()));
    QDir wd(coreWorkDir());
    statsProc->setWorkingDirectory(wd);
    statsProc->addArgument(coreBinaryPath());
    statsProc->addArgument("api");
    statsProc->addArgument("statsquery");
    statsProc->addArgument("--server=127.0.0.1:" + QString::number(apiPort));
    statsProc->start();
}

/* 从 stats JSON 提取 inbound uplink/downlink 总和（排除 api 入站） */
static long long v2StatsSum(const QString& js, const char* suffix) {
    long long sum = 0;
    int pos = 0;
    while (true) {
        int np = sFindStr(js, "\"name\"", pos);
        if (np < 0) { break; }
        int nstart = np + 6;
        while (nstart < (int)js.length()
               && (js.at(nstart) == ':' || js.at(nstart).isSpace())) { ++nstart; }
        if (nstart >= (int)js.length() || js.at(nstart) != '"') { pos = nstart > np ? nstart : np + 6; continue; }
        ++nstart;
        int nend = sFind(js, '"', nstart);
        if (nend < 0) { break; }
        pos = nend + 1;
        QString name = js.mid(nstart, nend - nstart);
        if (!name.endsWith(suffix) || !name.startsWith("inbound>>>")
            || name.contains(">>>api>>>")) { continue; }
        /* name 后找最近的 "value": N 或 "value":"N"（Xray 数字 / v2ray 字符串） */
        int vp = sFindStr(js, "\"value\"", pos);
        int nextName = sFindStr(js, "\"name\"", pos);
        if (vp < 0 || (nextName >= 0 && vp > nextName)) { continue; }
        int vstart = vp + 7;                 /* 跳过 "value" */
        while (vstart < (int)js.length()
               && (js.at(vstart) == ':' || js.at(vstart).isSpace())) { ++vstart; }
        if (vstart >= (int)js.length()) { break; }
        int vend;
        long long v;
        if (js.at(vstart) == '"') {
            vend = sFind(js, '"', vstart + 1);
            if (vend < 0) { break; }
            v = js.mid(vstart + 1, vend - vstart - 1).toLongLong();
        } else {
            vend = vstart;
            while (vend < (int)js.length()
                   && (js.at(vend).isDigit())) { ++vend; }
            if (vend == vstart) { pos = vstart; continue; }
            v = js.mid(vstart, vend - vstart).toLongLong();
        }
        pos = vend + 1;
        sum += v;
    }
    return sum;
}

void V2RayLogic::onStatsExited() {
    if (!statsProc) { return; }
#ifdef QT3_BUILD
    QByteArray out = statsProc->readStdout();
#else
    QByteArray out = statsProc->readAllStandardOutput();
#endif
    bool running = coreProc && coreProc->isRunning();
    delete statsProc;
    statsProc = 0;
    if (!running) { return; }
    QString js = qFromUtf8(out.data(), (int)out.size());
    long long up = v2StatsSum(js, "uplink");
    long long down = v2StatsSum(js, "downlink");
    if (up <= 0 && down <= 0) { refreshTrafficLabel(); return; }
    trafficUpLast = (up - trafficUpBase > 0) ? (up - trafficUpBase) : 0;
    trafficDownLast = (down - trafficDownBase > 0) ? (down - trafficDownBase) : 0;
    /* 每 2 秒采样一次 → 速率 = 增量 / 2 */
    trafficUpRate = trafficUpLast / 2;
    trafficDownRate = trafficDownLast / 2;
    reqCountLast = v2CountLines(baseDir() + "/access.log") - reqCountBase;
    if (reqCountLast < 0) { reqCountLast = 0; }
    totalUp = totalUpBase + trafficUpLast;
    totalDown = totalDownBase + trafficDownLast;
    if (++totalFlushTick >= 15) { totalFlushTick = 0; flushTrafficTotals(); }
    refreshTrafficLabel();
}

void V2RayLogic::onStartCore() {
    startCoreAtIndex(-1);
}

void V2RayLogic::startCoreAtIndex(int idxIn) {
    if (!coreInstalled()) {
        appendLogWithPrefix(qFromUtf8("[核心]"),
            qFromUtf8("当前内核不可用（") + coreDisplayName() + qFromUtf8("），请先下载安装或切换内核"));
        setCoreMessage(qFromUtf8("状态：当前内核不可用，请下载安装或切换"));
        return;
    }
    if (coreProc && coreProc->isRunning()) { appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("核心已在运行")); return; }
    /* 启动核心前终止可能仍在运行的端口探针（监听端口将变化） */
    if (probeProc && probeProc->isRunning()) { probeProc->tryTerminate(); }
    /* D：监听地址/端口校验（SOCKS + HTTP 双端口） */
    int sport = listenSocksPort();
    int hport = listenHttpPort();
    if (sport < 0 || hport < 0) {
        appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("监听端口非法（需 1-65535），请检查“核心”页设置"));
        setCoreMessage(qFromUtf8("状态：启动失败：监听端口非法（1-65535）"));
        return;
    }
    if (sport == hport) {
        appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("SOCKS 与 HTTP 端口不能相同"));
        setCoreMessage(qFromUtf8("状态：启动失败：SOCKS/HTTP 端口不能相同"));
        return;
    }
    QString laddr = listenAddress();
#ifdef QT3_BUILD
    const char* laddrPtr = laddr.latin1();
#else
    QByteArray laddrBa = laddr.toLatin1();
    const char* laddrPtr = laddrBa.data();
#endif
    struct in_addr a4;
    struct in6_addr a6;
    if (inet_pton(AF_INET, laddrPtr, &a4) != 1
        && inet_pton(AF_INET6, laddrPtr, &a6) != 1) {
        appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("监听地址非法（需 IPv4/IPv6 字面量），请检查“核心”页设置"));
        setCoreMessage(qFromUtf8("状态：启动失败：监听地址非法（需 IPv4/IPv6）"));
        return;
    }
    if (laddr == "0.0.0.0" || laddr == "::") {
        appendLogWithPrefix(qFromUtf8("[核心]"),
            qFromUtf8("注意：监听地址 ") + laddr + qFromUtf8(" 会把 SOCKS 端口暴露给局域网/外部"));
    }
    saveSettings();
    int idx = (idxIn >= 0) ? idxIn : selectedNodeIndex();
    if (idx < 0) {
        if (nodes.count() >= 1) {
            idx = 0;
            for (int i = 0; i < (int)nodes.count(); ++i) {
                if (nodes[i].supported) { idx = i; break; }
            }
        } else {
            appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("没有可用节点，请先更新订阅"));
            setCoreMessage(qFromUtf8("状态：启动失败：没有可用节点（请先更新订阅）"));
            return;
        }
    }
    V2Node node = nodes[idx];
    if (!nodeSupportedByCore(node, coreKind)) {
        appendLogWithPrefix(qFromUtf8("[核心]"),
            qFromUtf8("该节点当前内核不支持（REALITY/XTLS-Vision 或未实现协议），请换节点"));
        setCoreMessage(qFromUtf8("状态：启动失败：该节点需要 Xray 内核（当前不支持）"));
        return;
    }
    /* 记住当前活动节点 */
    {
        QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
        st.setValue("activeNode", node.id);
        st.sync();
    }
    QString err;
    if (!generateConfig(node, err)) {
        appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("生成配置失败：") + err);
        setCoreMessage(qFromUtf8("状态：启动失败：生成配置失败（") + err + qFromUtf8("）"));
        return;
    }
    coreStopRequested = false;
    if (coreProc) { delete coreProc; coreProc = 0; }
    coreProc = new QProcess34(this);
    QProcess34::connectStdout(coreProc, this, SLOT(onCoreOutput()));
    QProcess34::connectStderr(coreProc, this, SLOT(onCoreErrOutput()));
    QProcess34::connectExited(coreProc, this, SLOT(onCoreExit()));
    QDir wd(coreWorkDir());
    coreProc->setWorkingDirectory(wd);
    /* geo 资产目录环境变量（系统内核也生效，修复“绕过中国大陆”找不到 geo 文件） */
    QString assetDir = geoAssetDir();
    if (!assetDir.isEmpty()) {
        /* Qt3 QProcess 无 setEnvironment：子进程继承父进程 env，直接 setenv */
        QByteArray assetBa = QFile::encodeName(assetDir);
#ifdef QT3_BUILD
        setenv("XRAY_LOCATION_ASSET", assetBa.data(), 1);
        setenv("V2RAY_LOCATION_ASSET", assetBa.data(), 1);
#else
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("XRAY_LOCATION_ASSET", assetDir);
        env.insert("V2RAY_LOCATION_ASSET", assetDir);
        coreProc->setProcessEnvironment(env);
#endif
    }
    coreProc->addArgument(coreBinaryPath());
    coreProc->addArgument("run");
    coreProc->addArgument("-c");
    coreProc->addArgument(coreConfigPath());
    coreProc->start();
    resetTrafficCounters();
    startTrafficPoll();
    /* 记录核心进程 PID */
    {
        QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
        st.setValue("core/lastPid", (int)coreProc->processIdentifier());
        st.sync();
    }
    /* 记录“用户期望运行”，供下次启动恢复 */
    {
        QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
        st.setValue("core/runningLast", 1);
        st.sync();
    }
    refreshCoreState();
    refreshActiveNodeLabel();
    appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("核心启动中（节点：") + node.name + qFromUtf8("）"));
}
void V2RayLogic::onCoreOutput() {
    if (!coreProc) { return; }
    QByteArray out = coreProc->readStdout();
    if ((int)out.size() <= 0) { return; }
    appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8(out.data(), (int)out.size()));
}
void V2RayLogic::onCoreErrOutput() {
    if (!coreProc) { return; }
    QByteArray out = coreProc->readStderr();
    if ((int)out.size() <= 0) { return; }
    appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("(err) ") + qFromUtf8(out.data(), (int)out.size()));
}
void V2RayLogic::onCoreExit() {
    bool stopped = coreStopRequested;
    coreStopRequested = false;
    /* 退出中保留 runningLast；其它情况（手动停止/异常退出）置 0，避免下次自动拉起重启失败的核心 */
    if (!appQuitting) {
        QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
        st.setValue("core/runningLast", 0);
        st.sync();
    }
    /* 退出码（Qt3 exitStatus()：normalExit=退出码；否则视为异常终止） */
    QString exitDesc;
    if (coreProc) {
#ifdef QT3_BUILD
        if (coreProc->normalExit()) {
            exitDesc = qFromUtf8("（退出码 ")
                + QString::number(coreProc->exitStatus()) + qFromUtf8("）");
        } else {
            int sig = -coreProc->exitStatus();
            exitDesc = (sig > 0)
                ? (qFromUtf8("（异常终止：信号 ") + QString::number(sig)
                   + qFromUtf8("）"))
                : qFromUtf8("（异常终止）");
        }
#else
        if (coreProc->exitStatus() == QProcess::NormalExit) {
            exitDesc = qFromUtf8("（退出码 ")
                + QString::number(coreProc->exitCode()) + qFromUtf8("）");
        } else {
            exitDesc = qFromUtf8("（异常终止：信号 ")
                + QString::number(coreProc->exitCode()) + qFromUtf8("）");
        }
#endif
    }
    QString stopDesc = exitDesc;
    if (stopped && stopDesc == qFromUtf8("（异常终止）")) {
        stopDesc = qFromUtf8("（已请求停止）");
    }
    appendLogWithPrefix(qFromUtf8("[核心]"),
        stopped ? (qFromUtf8("核心已停止") + stopDesc)
                : (qFromUtf8("核心已退出") + exitDesc));
    stopTrafficPoll();
    flushTrafficTotals();   /* 核心退出即落盘累计流量，避免最多 30s 尾巴丢失 */
    {
        QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
        st.setValue("core/lastPid", 0);
        st.sync();
    }
    refreshCoreState();
    refreshActiveNodeLabel();
    setCoreMessage(stopped ? qFromUtf8("状态：核心已停止")
                           : qFromUtf8("状态：核心已退出（异常，详见日志）"));
}
void V2RayLogic::onStopCore() {
    if (!coreProc || !coreProc->isRunning()) { appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("核心未在运行")); return; }
    coreStopRequested = true;
    {
        QSettings34 st(qFromUtf8("v2ray_suber"), qFromUtf8("settings"));
        st.setValue("core/runningLast", 0);
        st.sync();
    }
    coreProc->tryTerminate();
    appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("已发送停止请求"));
    refreshCoreState();
    refreshActiveNodeLabel();
}
void V2RayLogic::onCopyNodeLink() {
    int idx = selectedNodeIndex();
    if (idx < 0) { appendLogWithPrefix(qFromUtf8("[节点]"), qFromUtf8("请先选中要复制的节点")); return; }
    QApplication::clipboard()->setText(nodes[idx].raw);
    appendLogWithPrefix(qFromUtf8("[节点]"), qFromUtf8("已复制节点链接"));
}

/* ── 延迟测试（curl 实现，队列串行，不阻塞 UI） ── */
/* ── 测速：TCP（传输层）/ 真连接（经临时核心访问 204） ── */
void V2RayLogic::onTestLatency() { onTestTcpSelected(); }
void V2RayLogic::onTestTcpSelected() { startTestBatch(false, false); }
void V2RayLogic::onTestTcpAll() { startTestBatch(false, true); }
void V2RayLogic::onTestRealSelected() { startTestBatch(true, false); }
void V2RayLogic::onTestRealAll() { startTestBatch(true, true); }

void V2RayLogic::onStopTest() {
    testCancel = true;
    testQueue.clear();
    if (latProc) { latProc->tryTerminate(); }
    if (testCurlProc) { testCurlProc->tryTerminate(); }
    if (testCoreProc) { testCoreProc->tryTerminate(); }
    appendLogWithPrefix(qFromUtf8("[测速]"), qFromUtf8("已请求停止测试"));
}

void V2RayLogic::startTestBatch(bool real, bool all) {
    if (latBusy || testTotal > 0) {
        appendLogWithPrefix(qFromUtf8("[测速]"), qFromUtf8("已有测试在进行中"));
        return;
    }
    testQueue.clear();
    if (all) {
        for (int i = 0; i < (int)nodes.count(); ++i) { testQueue.append(i); }
    } else {
        int idx = selectedNodeIndex();
        if (idx < 0) {
            appendLogWithPrefix(qFromUtf8("[测速]"), qFromUtf8("请先选中要测试的节点"));
            return;
        }
        testQueue.append(idx);
    }
    if ((int)testQueue.count() <= 0) { return; }
    testIsReal = real;
    testCancel = false;
    testDone = 0;
    testTotal = (int)testQueue.count();
    v2SetTestBtns(testTcpSelBtn, testRealSelBtn, testTcpAllBtn, testRealAllBtn, testStopBtn, true);
    if (nodeStatus) {
        nodeStatus->setText(qFromUtf8("测试中：0/") + QString::number(testTotal));
    }
    appendLogWithPrefix(qFromUtf8("[测速]"),
        (real ? qFromUtf8("开始真连接批量测试，共 ") : qFromUtf8("开始 TCP 批量测试，共 "))
        + QString::number(testTotal) + qFromUtf8(" 个节点"));
    if (real) {
        appendLogWithPrefix(qFromUtf8("[测速]"),
            qFromUtf8("核心：") + coreBinaryPath());
    }
    startNextTest();
}

void V2RayLogic::startNextTest() {
    if (testCancel) { finishTest(); return; }
    if ((int)testQueue.count() <= 0) { finishTest(); return; }
    int idx = testQueue[0];
#ifdef QT3_BUILD
    testQueue.remove(testQueue.at(0));
#else
    testQueue.removeAt(0);
#endif
    if (idx < 0 || idx >= (int)nodes.count()) {
        QTimer::singleShot(0, this, SLOT(onContinueTest()));
        return;
    }
    emit testProgress(testDone, testTotal, idx);
    if (nodeStatus) {
        nodeStatus->setText(qFromUtf8("测试中：") + QString::number(testDone)
            + "/" + QString::number(testTotal));
    }
    if (!nodes[idx].supported) {
        nodes[idx].tcpMs = -1;
        nodes[idx].realMs = -1;
        nodes[idx].testedAt = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        ++testDone;
        appendLogWithPrefix(qFromUtf8("[测速]"),
            nodes[idx].name + qFromUtf8("：不支持（需 Xray/未实现），跳过"));
        refreshNodeList();
        QTimer::singleShot(0, this, SLOT(onContinueTest()));
        return;
    }
    if (testIsReal) { startRealTest(idx); return; }
    curLatIndex = idx;
    latGotConnected = false;
    latErrData.resize(0);
    if (latProc) { delete latProc; latProc = 0; }
    latProc = new QProcess34(this);
    QProcess34::connectStdout(latProc, this, SLOT(onLatOutput()));
    QProcess34::connectStderr(latProc, this, SLOT(onLatErrOutput()));
    QProcess34::connectExited(latProc, this, SLOT(onLatExited()));
    latProc->addArgument("curl");
    latProc->addArgument("-sSv");
    latProc->addArgument("--connect-timeout");
    latProc->addArgument("3");
    latProc->addArgument("--max-time");
    latProc->addArgument("4");
    latProc->addArgument("-o");
    latProc->addArgument("/dev/null");
    latProc->addArgument("telnet://" + nodes[idx].addr + ":" + nodes[idx].port);
    latTime.start();
    latBusy = true;
    latProc->start();
    appendLogWithPrefix(qFromUtf8("[测速]"),
        qFromUtf8("TCP 测试中：") + nodes[idx].addr + ":" + nodes[idx].port);
}

void V2RayLogic::onContinueTest() {
    startNextTest();
}

void V2RayLogic::onLatOutput() {
    if (!latProc) { return; }
    latProc->readStdout(); /* 丢弃，telnet 模式无有效 stdout */
}

void V2RayLogic::onLatErrOutput() {
    if (!latProc) { return; }
#ifdef QT3_BUILD
    QByteArray chunk = latProc->readStderr();
#else
    QByteArray chunk = latProc->readAllStandardError();
#endif
    baAppend(latErrData, chunk);
    if (latGotConnected) { return; }
    /* curl -v 在 TCP 建连成功后立刻输出 "Connected to xxx"，此时采样耗时并终止进程 */
    QString s = tailStr(latErrData, 4096);
    if (sFindStr(s, "Connected to", 0) >= 0) {
        latGotConnected = true;
        if (curLatIndex >= 0 && curLatIndex < (int)nodes.count()) {
            nodes[curLatIndex].tcpMs = latTime.elapsed();
            nodes[curLatIndex].testedAt =
                QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        }
        latProc->tryTerminate();
    }
}

void V2RayLogic::onLatExited() {
    if (!latProc) { return; }
#ifdef QT3_BUILD
    baAppend(latErrData, latProc->readStderr());
    int exitCode = latProc->normalExit() ? latProc->exitStatus() : -1;
#else
    baAppend(latErrData, latProc->readAllStandardError());
    int exitCode = latProc->exitStatus() == QProcess::NormalExit ? latProc->exitCode() : -1;
#endif
    delete latProc;
    latProc = 0;
    latBusy = false;
    int idx = curLatIndex;
    curLatIndex = -1;
    if (idx >= 0 && idx < (int)nodes.count()) {
        if (latGotConnected) {
            appendLogWithPrefix(qFromUtf8("[测速]"),
                nodes[idx].name + " (" + nodes[idx].addr + ":" + nodes[idx].port + ") TCP "
                + QString::number(nodes[idx].tcpMs) + "ms");
        } else {
            nodes[idx].tcpMs = -1;
            QString s = tailStr(latErrData, 4096);
            QString errLine = lastNeedleLine(s, "curl:");
            QString hint;
            if (!errLine.isEmpty()) { hint = errLine; }
            else if (exitCode == 28) { hint = qFromUtf8("连接超时"); }
            else if (exitCode == 7) { hint = qFromUtf8("连接被拒绝/不可达"); }
            else if (exitCode == 6) { hint = qFromUtf8("域名解析失败"); }
            else { hint = qFromUtf8("curl exit=") + QString::number(exitCode); }
            appendLogWithPrefix(qFromUtf8("[测速]"),
                nodes[idx].name + " (" + nodes[idx].addr + ":" + nodes[idx].port + ") "
                + qFromUtf8("TCP 失败：") + hint);
        }
    }
    ++testDone;
    refreshNodeList();
    saveNodes();
    emit testProgress(testDone, testTotal, idx);
    startNextTest();
}

int V2RayLogic::pickFreePort() {
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { return 0; }
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    a.sin_port = 0;
    if (bind(s, (struct sockaddr*)&a, sizeof(a)) != 0) { close(s); return 0; }
    socklen_t len = sizeof(a);
    getsockname(s, (struct sockaddr*)&a, &len);
    int port = ntohs(a.sin_port);
    close(s);
    return port;
}

/* 临时核心就绪探测：127.0.0.1:port 是否已可连（loopback 拒绝立即返回） */
static bool v2PortReady(int port) {
    if (port <= 0) { return false; }
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) { return false; }
    struct sockaddr_in a;
    memset(&a, 0, sizeof(a));
    a.sin_family = AF_INET;
    a.sin_port = htons((unsigned short)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    bool ok = (::connect(s, (struct sockaddr*)&a, sizeof(a)) == 0);
    close(s);
    return ok;
}

void V2RayLogic::startRealTest(int idx) {
    curLatIndex = idx;
    testPort = pickFreePort();
    QString cfg;
    if (testPort <= 0 || !buildTestConfig(nodes[idx], testPort, cfg)) {
        nodes[idx].realMs = -1;
        nodes[idx].testedAt = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        ++testDone;
        refreshNodeList();
        saveNodes();
        QTimer::singleShot(0, this, SLOT(onContinueTest()));
        return;
    }
    writeUtf8File(coreWorkDir() + "/config_test.json", cfg);
    if (testCoreProc) { delete testCoreProc; testCoreProc = 0; }
    testCoreProc = new QProcess34(this);
    QProcess34::connectStdout(testCoreProc, this, SLOT(onTestCoreOutput()));
    QProcess34::connectStderr(testCoreProc, this, SLOT(onTestCoreErrOutput()));
    QDir wd(coreWorkDir());
    testCoreProc->setWorkingDirectory(wd);
    testCoreProc->addArgument(coreBinaryPath());
    testCoreProc->addArgument("run");
    testCoreProc->addArgument("-c");
    testCoreProc->addArgument(coreWorkDir() + "/config_test.json");
    if (!testCoreProc->start()) {
        appendLogWithPrefix(qFromUtf8("[测速]"),
            qFromUtf8("临时核心启动失败：") + coreBinaryPath());
        nodes[idx].realMs = -1;
        nodes[idx].testedAt = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        ++testDone;
        refreshNodeList();
        saveNodes();
        QTimer::singleShot(0, this, SLOT(onContinueTest()));
        return;
    }
    appendLogWithPrefix(qFromUtf8("[测速]"),
        qFromUtf8("真连接测试中：") + nodes[idx].name);
    testCoreWaitTries = 0;
    QTimer::singleShot(100, this, SLOT(waitTestCoreReady()));
}

void V2RayLogic::onTestCoreOutput() {
    if (!testCoreProc) { return; }
#ifdef QT3_BUILD
    QByteArray d = testCoreProc->readStdout();
#else
    QByteArray d = testCoreProc->readAllStandardOutput();
#endif
    if ((int)d.size() > 0) {
        appendLogWithPrefix(qFromUtf8("[测速] 核心"), qFromUtf8(d.data(), (int)d.size()));
    }
}

void V2RayLogic::onTestCoreErrOutput() {
    if (!testCoreProc) { return; }
#ifdef QT3_BUILD
    QByteArray d = testCoreProc->readStderr();
#else
    QByteArray d = testCoreProc->readAllStandardError();
#endif
    if ((int)d.size() > 0) {
        appendLogWithPrefix(qFromUtf8("[测速] 核心"), qFromUtf8(d.data(), (int)d.size()));
    }
}

/* 等临时核心真正监听 testPort（最多 3s），就绪后再跑 curl */
void V2RayLogic::waitTestCoreReady() {
    if (testCancel) { finishTest(); return; }
    if (v2PortReady(testPort)) { onRealCoreReady(); return; }
    if (++testCoreWaitTries > 30) {
        appendLogWithPrefix(qFromUtf8("[测速]"),
            qFromUtf8("临时核心 3 秒内未监听 127.0.0.1:")
            + QString::number(testPort) + qFromUtf8("，跳过该节点"));
        if (testCoreProc) { testCoreProc->tryTerminate(); testCoreProc->deleteLater(); testCoreProc = 0; }
        int idx = curLatIndex;
        if (idx >= 0 && idx < (int)nodes.count()) {
            nodes[idx].realMs = -1;
            nodes[idx].testedAt = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        }
        ++testDone;
        refreshNodeList();
        saveNodes();
        QTimer::singleShot(0, this, SLOT(onContinueTest()));
        return;
    }
    QTimer::singleShot(100, this, SLOT(waitTestCoreReady()));
}

void V2RayLogic::onRealCoreReady() {
    if (testCancel) { finishTest(); return; }
    int idx = curLatIndex;
    if (idx < 0 || idx >= (int)nodes.count()) { startNextTest(); return; }
    if (testCurlProc) { delete testCurlProc; testCurlProc = 0; }
    testCurlProc = new QProcess34(this);
    QProcess34::connectExited(testCurlProc, this, SLOT(onRealCurlExited()));
    testCurlProc->addArgument("curl");
    testCurlProc->addArgument("-sS");
    testCurlProc->addArgument("-o");
    testCurlProc->addArgument("/dev/null");
    testCurlProc->addArgument("-m");
    testCurlProc->addArgument("5");
    testCurlProc->addArgument("-w");
    testCurlProc->addArgument("%{http_code} %{time_total}");
    testCurlProc->addArgument("--socks5-hostname");
    testCurlProc->addArgument(QString("127.0.0.1:") + QString::number(testPort));
    testCurlProc->addArgument(testUrl);
    testCurlProc->start();
}

void V2RayLogic::onRealCurlExited() {
    if (!testCurlProc) { return; }
#ifdef QT3_BUILD
    QByteArray out = testCurlProc->readStdout();
    QByteArray errOut = testCurlProc->readStderr();
    int exitCode = testCurlProc->normalExit() ? testCurlProc->exitStatus() : -1;
    bool ok = testCurlProc->normalExit();
#else
    QByteArray out = testCurlProc->readAllStandardOutput();
    QByteArray errOut = testCurlProc->readAllStandardError();
    int exitCode = (testCurlProc->exitStatus() == QProcess::NormalExit)
        ? testCurlProc->exitCode() : -1;
    bool ok = (testCurlProc->exitStatus() == QProcess::NormalExit)
        && (testCurlProc->exitCode() == 0);
#endif
    delete testCurlProc;
    testCurlProc = 0;
    int idx = curLatIndex;
    int ms = -1;
    QString httpCode;
    if (ok) {
        QString s = qFromUtf8(out.data(), (int)out.size());
#ifdef QT3_BUILD
        s = s.stripWhiteSpace();
#else
        s = s.trimmed();
#endif
        /* 格式："204 0.345678"（ucode time_total）；Qt3 取不到 curl exit code，用状态码判定 */
        int sp = sFind(s, ' ', 0);
        QString code = (sp > 0) ? s.left(sp) : s;
        httpCode = code;
        QString secStr = (sp > 0) ? s.mid(sp + 1) : QString();
        bool numOk = false;
        double sec = secStr.toDouble(&numOk);
        if (numOk && sec >= 0.0 && sec < 30.0
            && (code == "204" || code == "200")) {
            ms = (int)(sec * 1000.0 + 0.5);
        }
    }
    if (idx >= 0 && idx < (int)nodes.count()) {
        nodes[idx].realMs = ms;
        nodes[idx].testedAt = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        appendLogWithPrefix(qFromUtf8("[测速]"),
            nodes[idx].name + qFromUtf8("：真连接 ")
            + (ms < 0 ? qFromUtf8("—") : QString::number(ms) + "ms"));
    }
    if (ms < 0 && idx >= 0 && idx < (int)nodes.count()) {
        QString why;
        if (httpCode.isEmpty()) { why = qFromUtf8("无输出"); }
        else if (httpCode == "000") { why = qFromUtf8("未拿到 HTTP 码（连接失败）"); }
        else { why = qFromUtf8("HTTP ") + httpCode; }
        QString es = qFromUtf8(errOut.data(), (int)errOut.size());
#ifdef QT3_BUILD
        es = es.stripWhiteSpace();
#else
        es = es.trimmed();
#endif
        if (es.length() > 120) { es = es.left(120); }
        appendLogWithPrefix(qFromUtf8("[测速]"),
            nodes[idx].name + qFromUtf8("：真连接失败（curl 退出码 ")
            + QString::number(exitCode) + qFromUtf8("，") + why
            + (es.isEmpty() ? QString() : qFromUtf8("；") + es) + qFromUtf8("）"));
    }
    if (testCoreProc) { testCoreProc->tryTerminate(); testCoreProc->deleteLater(); testCoreProc = 0; }
    ++testDone;
    refreshNodeList();
    saveNodes();
    emit testProgress(testDone, testTotal, idx);
    QTimer::singleShot(120, this, SLOT(onContinueTest()));
}

void V2RayLogic::finishTest() {
    if (testCoreProc) { testCoreProc->tryTerminate(); testCoreProc->deleteLater(); testCoreProc = 0; }
    if (testCurlProc) { testCurlProc->deleteLater(); testCurlProc = 0; }
    testQueue.clear();
    testIsReal = false;
    testCancel = false;
    int done = testDone;
    int total = testTotal;
    testDone = 0;
    testTotal = 0;
    testCoreWaitTries = 0;
    curLatIndex = -1;
    latBusy = false;
    refreshNodeList();
    saveNodes();
    appendLogWithPrefix(qFromUtf8("[测速]"),
        qFromUtf8("测试结束：") + QString::number(done) + "/" + QString::number(total));
    v2SetTestBtns(testTcpSelBtn, testRealSelBtn, testTcpAllBtn, testRealAllBtn, testStopBtn, false);
    if (nodeStatus) {
        nodeStatus->setText(qFromUtf8("状态：测试结束（") + QString::number(done)
            + "/" + QString::number(total) + qFromUtf8("）"));
    }
    if (testStopBtn) { testStopBtn->setEnabled(false); }
    emit testFinished();
}

/* ── 生成 config_run.json ── */
/* 生成代理出站对象（tag=proxy）；协议/参数不支持时返回空并设置 err */
static QString v2BuildProxyOutbound(const V2Node& node, CoreKind kind, QString& err) {
    bool portOk = false;
    int portNum = node.port.toInt(&portOk);
    if (!portOk || portNum <= 0 || portNum > 65535) {
        err = qFromUtf8("端口非法");
        return QString();
    }
    if (node.proto != "vmess" && node.proto != "vless"
        && node.proto != "trojan" && node.proto != "ss") {
        err = qFromUtf8("暂不支持生成该协议配置：") + node.proto;
        return QString();
    }
    QString net = node.net;
    if (net.isEmpty()) { net = "tcp"; }
    QString outbound;
    if (node.proto == "ss") {
        outbound = "{\"protocol\":\"shadowsocks\",\"settings\":{\"servers\":[{\"address\":\""
            + node.addr + "\",\"port\":" + node.port + ",\"method\":\"" + node.method
            + "\",\"password\":\"" + node.uuid + "\"}]}";
    } else {
        QString stream;
        stream += "\"streamSettings\":{\"network\":\"" + net + "\"";
        if (node.security == "reality" && kind == CoreXray) {
            if (node.pbk.isEmpty()) {
                err = qFromUtf8("REALITY 节点缺少 pbk");
                return QString();
            }
            QString sn = node.sni.isEmpty() ? node.addr : node.sni;
            QString fpr = node.fp.isEmpty() ? QString("chrome") : node.fp;
            QString spx = node.spx.isEmpty() ? QString("/") : node.spx;
            stream += ",\"security\":\"reality\",\"realitySettings\":{"
                "\"serverName\":\"" + sn + "\","
                "\"fingerprint\":\"" + fpr + "\","
                "\"publicKey\":\"" + node.pbk + "\","
                "\"shortId\":\"" + node.sid + "\","
                "\"spiderX\":\"" + spx + "\"}";
        } else if (node.security == "tls") {
            QString sn = node.sni.isEmpty() ? node.addr : node.sni;
            stream += ",\"security\":\"tls\",\"tlsSettings\":{\"serverName\":\"" + sn + "\"";
            if (node.allowInsecure) { stream += ",\"allowInsecure\":true"; }
            if (!node.alpn.isEmpty()) { stream += ",\"alpn\":[\"" + node.alpn + "\"]"; }
            stream += "}";
        }
        if (net == "ws") {
            QString path = node.path.isEmpty() ? QString("/") : node.path;
            QString host = node.host.isEmpty() ? node.addr : node.host;
            stream += ",\"wsSettings\":{\"path\":\"" + path + "\",\"headers\":{\"Host\":\"" + host + "\"}}";
        }
        if (net == "grpc") {
            stream += ",\"grpcSettings\":{\"serviceName\":\"" + node.serviceName + "\"}";
        }
        stream += "}";
        if (node.proto == "vmess") {
            outbound = "{\"protocol\":\"vmess\",\"settings\":{\"vnext\":[{\"address\":\"" + node.addr
                + "\",\"port\":" + node.port + ",\"users\":[{\"id\":\"" + node.uuid
                + "\",\"alterId\":" + (node.aid.isEmpty() ? QString("0") : node.aid)
                + ",\"security\":\"" + (node.scy.isEmpty() ? QString("auto") : node.scy) + "\"}]}]},";
        } else if (node.proto == "vless") {
            outbound = "{\"protocol\":\"vless\",\"settings\":{\"vnext\":[{\"address\":\"" + node.addr
                + "\",\"port\":" + node.port + ",\"users\":[{\"id\":\"" + node.uuid
                + "\",\"encryption\":\"none\""
                + ((kind == CoreXray && !node.flow.isEmpty())
                    ? ",\"flow\":\"" + node.flow + "\"" : QString())
                + "}]}]},";
        } else {
            outbound = "{\"protocol\":\"trojan\",\"settings\":{\"servers\":[{\"address\":\"" + node.addr
                + "\",\"port\":" + node.port + ",\"password\":\"" + node.uuid + "\"}]},";
        }
        outbound += stream;
    }
    outbound += ",\"tag\":\"proxy\"}";
    return outbound;
}

bool V2RayLogic::generateConfig(const V2Node& node, QString& err) {
    if (!nodeSupportedByCore(node, coreKind)) {
        err = qFromUtf8("该节点需要 Xray 内核或协议未实现");
        return false;
    }
    if (coreDir().length() <= 0) { err = qFromUtf8("核心目录缺失"); return false; }
    int sport = listenSocksPort();
    int hport = listenHttpPort();
    if (sport < 0 || hport < 0 || sport == hport) {
        err = qFromUtf8("监听端口非法（SOCKS/HTTP 需 1-65535 且不同）");
        return false;
    }
    QString laddr = listenAddress();
    QString outbound = v2BuildProxyOutbound(node, coreKind, err);
    if (outbound.isEmpty()) { return false; }
    /* stats/api 端口：15490 起找空闲位（避开 SOCKS/HTTP 端口） */
    apiPort = 0;
    for (int p = 15490; p <= 15499; ++p) {
        if (p != sport && p != hport) { apiPort = p; break; }
    }
    QString accessPath = baseDir() + "/access.log";
    QString cfg = "{\"log\":{\"access\":\"" + accessPath
        + "\",\"loglevel\":\"warning\"},";
    if (apiPort > 0) {
        cfg += "\"stats\":{},"
            "\"policy\":{\"system\":{\"statsInboundUplink\":true,"
            "\"statsInboundDownlink\":true}},"
            "\"api\":{\"tag\":\"api\",\"services\":[\"StatsService\"]},";
    }
    cfg += "\"inbounds\":["
        "{\"tag\":\"socks\",\"port\":" + QString::number(sport)
        + ",\"listen\":\"" + laddr
        + "\",\"protocol\":\"socks\",\"settings\":{\"udp\":true}},"
        "{\"tag\":\"http\",\"port\":" + QString::number(hport)
        + ",\"listen\":\"" + laddr
        + "\",\"protocol\":\"http\",\"settings\":{}}"
        + (apiPort > 0
           ? (QString(",{\"tag\":\"api\",\"port\":")
              + QString::number(apiPort)
              + ",\"listen\":\"127.0.0.1\",\"protocol\":\"dokodemo-door\","
                "\"settings\":{\"address\":\"127.0.0.1\"}}")
           : QString())
        + "],"
        "\"outbounds\":[" + outbound
        + ",{\"protocol\":\"freedom\",\"tag\":\"direct\",\"settings\":{}}";
    if (apiPort > 0) {
        cfg += ",{\"protocol\":\"freedom\",\"tag\":\"api\",\"settings\":{}}]";
    } else {
        cfg += "]";
    }
    QStringList rules;
    if (apiPort > 0) {
        /* stats api 路由必须放在所有规则之前（否则会被 geoip:cn direct 截走） */
        rules.append("{\"type\":\"field\",\"inboundTag\":[\"api\"],\"outboundTag\":\"api\"}");
    }
    bool lanOn = (bypassLanCheck && bypassLanCheck->isChecked());
    if (lanOn) {
        /* A：绕过局域网——显式私有 CIDR（不依赖 geoip.dat） */
        rules.append("{\"type\":\"field\",\"outboundTag\":\"direct\",\"ip\":["
            "\"10.0.0.0/8\",\"172.16.0.0/12\",\"192.168.0.0/16\","
            "\"127.0.0.0/8\",\"169.254.0.0/16\","
            "\"::1/128\",\"fc00::/7\",\"fe80::/10\"]}");
    }
    if (bypassCnCheck && bypassCnCheck->isChecked()) {
        if (hasGeoFiles()) {
            /* 绕过中国大陆：geosite:cn / geoip:cn 直连，其余走当前节点（引用核心包自带的 geo 文件） */
            rules.append("{\"type\":\"field\",\"outboundTag\":\"direct\",\"domain\":[\"geosite:cn\"]}");
            rules.append("{\"type\":\"field\",\"outboundTag\":\"direct\",\"ip\":[\"geoip:cn\"]}");
            appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("已启用绕过中国大陆路由（geosite:cn / geoip:cn 直连，其余走代理）"));
        } else {
            appendLogWithPrefix(qFromUtf8("[核心]"),
                qFromUtf8("警告：未找到 core/geoip.dat 或 core/geosite.dat，绕过中国大陆路由未启用。请重新“下载安装”以获取完整 v2ray-core ") + qFromUtf8(V2CORE_VERSION) + qFromUtf8(" 包"));
        }
    }
    if (!rules.isEmpty()) {
        rules.append("{\"type\":\"field\",\"outboundTag\":\"proxy\",\"network\":\"tcp,udp\"}");
        cfg += ",\"routing\":{\"domainStrategy\":\"IPIfNonMatch\",\"rules\":[" + rules.join(",") + "]}";
        if (lanOn) {
            appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("已启用绕过局域网（私有地址直连）"));
        }
    }
    cfg += "}";
    writeUtf8File(coreConfigPath(), cfg);
    return true;
}

bool V2RayLogic::buildTestConfig(const V2Node& n, int port, QString& cfg) {
    if (!n.supported) { return false; }
    QString err;
    QString outbound = v2BuildProxyOutbound(n, coreKind, err);
    if (outbound.isEmpty()) { return false; }
    cfg = "{\"log\":{\"loglevel\":\"warning\"},"
        "\"inbounds\":[{\"port\":" + QString::number(port)
        + ",\"listen\":\"127.0.0.1\",\"protocol\":\"socks\",\"settings\":{\"udp\":true}}],"
        "\"outbounds\":[" + outbound + "]}";
    return true;
}

/* ── 核心状态 ── */
void V2RayLogic::refreshCoreState() {
    if (!coreState || !startBtn || !stopBtn) { return; }
    QString s;
    if (coreProc && coreProc->isRunning()) {
        int sp = listenSocksPort();
        int hp = listenHttpPort();
        s = qFromUtf8("状态：核心运行中（PID ")
            + QString::number((long)coreProc->processIdentifier())
            + qFromUtf8(" · SOCKS ")
            + listenAddress() + ":" + QString::number(sp > 0 ? sp : 10808)
            + qFromUtf8(" · HTTP ") + listenAddress() + ":"
            + QString::number(hp > 0 ? hp : 10809) + qFromUtf8("）");
    } else if (coreInstalled()) {
        s = qFromUtf8("状态：已就绪 · ") + coreDisplayName()
            + qFromUtf8(" @ ") + coreBinaryPath();
    } else {
        s = qFromUtf8("状态：") + coreDisplayName() + qFromUtf8(" 不可用，请下载安装或切换内核");
    }
    coreState->setText(s);
    bool running = coreProc && coreProc->isRunning();
    startBtn->setEnabled(coreInstalled() && !running);
    stopBtn->setEnabled(running);
    if (coreDlBtn) {
        bool needDl = !(coreKind == CoreXray && coreSourceSystem);
        coreDlBtn->setEnabled(needDl);
        coreDlBtn->setText(coreKind == CoreXray ? qFromUtf8("下载安装 Xray")
                                                : qFromUtf8("下载安装 v2ray"));
    }
    refreshActiveNodeLabel();
}
