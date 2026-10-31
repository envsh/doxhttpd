#include "compat34.h"
#include "compatcore34.h"
#include "logic.h"
#include "cJSON.h"

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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>

/* ── 固定版本号（锁定，不可升级） ── */
#define V2CORE_VERSION      "v5.35.0"
#define V2CORE_GH_PREFIX    "https://gh-proxy.com/https://github.com/v2fly/v2ray-core/releases/download/v5.35.0/"

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
static void lbAdd(V2ListBox* lb, const QString& s) {
#ifdef QT3_BUILD
    lb->insertItem(s);
#else
    lb->addItem(s);
#endif
}
static int lbCurrent(V2ListBox* lb) {
#ifdef QT3_BUILD
    return lb->currentItem();
#else
    return lb->currentRow();
#endif
}
static void lbClear(V2ListBox* lb) {
    lb->clear();
}
static int lbCount(V2ListBox* lb) {
    return (int)lb->count();
}

static void writeUtf8File(const QString& path, const QString& content) {
#ifdef QT3_BUILD
    QByteArray u = content.utf8();
#else
    QByteArray u = content.toUtf8();
#endif
    FILE* fp = fopen(QFile::encodeName(path).data(), "wb");
    if (!fp) { return; }
    fwrite(u.data(), 1, (size_t)u.size(), fp);
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
            char c = s[j].latin1();
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
    nodeCount = 0;
    for (int i = 0; i < 512; ++i) { nodeLatencyMs[i] = -1; nodeLatencyKnown[i] = false; }
    subProc = 0;
    dlProc = 0;
    unzipProc = 0;
    coreProc = 0;
    latProc = 0;
    coreDlBusy = false;
    latBusy = false;
    latGotConnected = false;
    curLatIndex = -1;
    latQueueCount = 0;
    subEdit = 0;
    subList = 0;
    nodeList = 0;
    coreState = 0;
    log = 0;
    startBtn = 0;
    stopBtn = 0;
    bypassCnCheck = 0;
    autoUpdateIntervalMin = 60;
    pendingSubIndex = 0;
    updatingAll = false;
    dlProgressPct = -1;
    ensureDirs();
    loadSubscriptions();
    if (subList) { refreshSubList(); }
    /* 订阅定时自动更新（默认 60 分钟，串行执行） */
    autoUpdateTimer = new QTimer(this);
    connect(autoUpdateTimer, SIGNAL(timeout()), this, SLOT(onAutoUpdateTick()));
    autoUpdateTimer->start(autoUpdateIntervalMin * 60 * 1000);
}

void V2RayLogic::setWidgets(QLineEdit* subEdit_, V2ListBox* subList_, V2ListBox* nodeList_,
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
QString V2RayLogic::coreBinaryPath() const {
    return coreDir() + "/v2ray";
}
QString V2RayLogic::zipPath() const {
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
    mkdir(QString(home + "/.config").utf8().data(), 0755);
    mkdir(baseDir().utf8().data(), 0755);
    mkdir(coreDir().utf8().data(), 0755);
}

bool V2RayLogic::coreInstalled() const {
    if (!fileExists(coreBinaryPath())) { return false; }
    if (access(QFile::encodeName(coreBinaryPath()).data(), X_OK) != 0) { return false; }
    return true;
}

bool V2RayLogic::hasGeoFiles() const {
    return fileExists(coreDir() + "/geoip.dat") && fileExists(coreDir() + "/geosite.dat");
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
    lbClear(subList);
    for (int i = 0; i < (int)subs.count(); ++i) {
        lbAdd(subList, subs[i]);
    }
}
void V2RayLogic::refreshNodeList() {
    if (!nodeList) { return; }
    lbClear(nodeList);
    for (int i = 0; i < nodeCount; ++i) {
        QString show = nodes[i].name;
        if (show.isEmpty()) { show = qFromUtf8("节点") + QString::number(i + 1); }
        show += " [" + nodes[i].proto + "] " + nodes[i].addr + ":" + nodes[i].port;
        if (nodeLatencyKnown[i]) {
            if (nodeLatencyMs[i] >= 0) {
                show += " [" + QString::number(nodeLatencyMs[i]) + "ms]";
            } else {
                show += " [" + qFromUtf8("超时") + "]";
            }
        }
        lbAdd(nodeList, show);
    }
}

void V2RayLogic::setRouteWidgets(QCheckBox* bypassCnCheck_) {
    bypassCnCheck = bypassCnCheck_;
    if (bypassCnCheck && !hasGeoFiles()) {
        appendLogWithPrefix(qFromUtf8("[核心]"),
            qFromUtf8("提示：核心目录缺少 geoip.dat / geosite.dat，勾选“绕过中国大陆”将不生效（重新下载安装核心即可）"));
    }
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
    int idx = lbCurrent(subList);
    if (idx < 0 || idx >= (int)subs.count()) { return -1; }
    return idx;
}
int V2RayLogic::selectedNodeIndex() {
    if (!nodeList) { return -1; }
    int idx = lbCurrent(nodeList);
    if (idx < 0 || idx >= nodeCount) { return -1; }
    return idx;
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
    subs.remove(subs.at(idx));
    saveSubscriptions();
    refreshSubList();
}
void V2RayLogic::startSubFetch(const QString& url) {
    if (subProc) { delete subProc; subProc = 0; }
    subData.resize(0);
    subProc = new QProcess(this);
    connect(subProc, SIGNAL(readyReadStdout()), this, SLOT(onSubOutput()));
    connect(subProc, SIGNAL(readyReadStderr()), this, SLOT(onSubErrOutput()));
    connect(subProc, SIGNAL(processExited()), this, SLOT(onSubFetched()));
    subProc->addArgument("curl");
    subProc->addArgument("-SsL");
    subProc->addArgument("--max-time");
    subProc->addArgument("30");
    subProc->addArgument(url);
    subProc->start();
    appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("正在更新：") + url);
}

void V2RayLogic::onUpdateSubscription() {
    if (subProc && subProc->isRunning()) { appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("上一次订阅更新仍在进行中")); return; }
    if (updatingAll) { appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("“更新全部订阅”进行中，请稍候")); return; }
    int idx = selectedSubIndex();
    if (idx < 0) {
        if ((int)subs.count() >= 1) { idx = 0; } else { appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("没有可用订阅，请先添加")); return; }
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
        appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("全部订阅更新完成"));
        return;
    }
    QString url = pendingSubUrls[pendingSubIndex];
    ++pendingSubIndex;
    startSubFetch(url);
}

void V2RayLogic::onAutoUpdateTick() {
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
    baAppend(subData, subProc->readStdout());
    delete subProc;
    subProc = 0;
    QString content = qFromUtf8(subData.data(), (int)subData.size());
    if (content.isEmpty()) {
        appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("订阅下载为空，请检查网络或订阅地址"));
        if (updatingAll) { startNextPendingSub(); }
        return;
    }
    QString plain = maybeDecodeBase64(content);
    saveSubLast(plain);
    clearNodes();
    parseSubscription(plain);
    refreshNodeList();
    appendLogWithPrefix(qFromUtf8("[订阅]"), qFromUtf8("更新完成，共解析 ") + QString::number(nodeCount) + qFromUtf8(" 个节点"));
    if (updatingAll) { startNextPendingSub(); }
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
    nodeCount = 0;
    for (int i = 0; i < 512; ++i) { nodeLatencyMs[i] = -1; nodeLatencyKnown[i] = false; }
}
void V2RayLogic::parseSubscription(const QString& content) {
    QStringList lines = splitLines(content);
    for (int i = 0; i < (int)lines.count(); ++i) {
#ifdef QT3_BUILD
        QString line = lines[i].stripWhiteSpace();
#else
        QString line = lines[i].trimmed();
#endif
        if (line.isEmpty()) { continue; }
        bool ok = false;
        QString err;
        if (line.left(8) == "vmess://") {
            V2Node node;
            node.proto = "vmess";
            parseVmess(line, node, ok, err);
            if (ok) {
                if (nodeCount < 512) { nodes[nodeCount] = node; ++nodeCount; }
                continue;
            }
            appendLog(qFromUtf8("vmess 解析失败：") + err);
        } else if (line.left(8) == "vless://") {
            V2Node node;
            node.proto = "vless";
            parseVlessTrojan(line, node, ok, err);
            if (ok) {
                if (nodeCount < 512) { nodes[nodeCount] = node; ++nodeCount; }
                continue;
            }
            appendLog(qFromUtf8("vless 解析失败：") + err);
        } else if (line.left(9) == "trojan://") {
            V2Node node;
            node.proto = "trojan";
            parseVlessTrojan(line, node, ok, err);
            if (ok) {
                if (nodeCount < 512) { nodes[nodeCount] = node; ++nodeCount; }
                continue;
            }
            appendLog(qFromUtf8("trojan 解析失败：") + err);
        } else if (line.left(5) == "ss://") {
            V2Node node;
            node.proto = "ss";
            node.raw = line;
            node.name = line;
            if (nodeCount < 512) { nodes[nodeCount] = node; ++nodeCount; }
            appendLog(qFromUtf8("提示：ss 节点仅展示，不支持一键生成配置"));
        } else {
            /* 其它行忽略 */
        }
    }
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
    j = cJSON_GetObjectItem(root, "net");
    if (j && j->valuestring) { node.net = j->valuestring; }
    j = cJSON_GetObjectItem(root, "path");
    if (j && j->valuestring) { node.path = j->valuestring; }
    j = cJSON_GetObjectItem(root, "host");
    if (j && j->valuestring) { node.host = j->valuestring; }
    j = cJSON_GetObjectItem(root, "tls");
    if (j && j->valuestring) { node.tls = j->valuestring; }
    j = cJSON_GetObjectItem(root, "ps");
    if (j && j->valuestring) { node.name = j->valuestring; }
    cJSON_Delete(root);
    node.raw = line;
    if (node.addr.isEmpty()) { err = qFromUtf8("缺少 add"); return; }
    if (node.port.isEmpty()) { err = qFromUtf8("缺少 port"); return; }
    if (node.uuid.isEmpty()) { err = qFromUtf8("缺少 id"); return; }
    if (node.name.isEmpty()) { node.name = node.addr + ":" + node.port; }
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
    node.uuid = line.mid(schemeEnd + 1, at - schemeEnd - 1);
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
            else if (key == "security") { node.tls = val; }
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
    ok = true;
}

/* ── 核心 ── */
void V2RayLogic::onDownloadCore() {
    if (coreDlBusy) { appendLog(qFromUtf8("正在下载核心，请稍候")); return; }
    QString zipName = "v2ray-linux-64.zip";
#ifdef __aarch64__
    zipName = "v2ray-linux-arm64-v8a.zip";
#endif
    QString url = qFromUtf8(V2CORE_GH_PREFIX) + zipName;
    ensureDirs();
    coreDlBusy = true;
    dlProgressPct = -1;
    dlErrData.resize(0);
    if (dlProc) { delete dlProc; dlProc = 0; }
    dlProc = new QProcess(this);
    connect(dlProc, SIGNAL(readyReadStdout()), this, SLOT(onCoreDlOutput()));
    connect(dlProc, SIGNAL(readyReadStderr()), this, SLOT(onCoreDlErrOutput()));
    connect(dlProc, SIGNAL(processExited()), this, SLOT(onCoreDlDone()));
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
    appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("正在下载 v2ray-core ") + qFromUtf8(V2CORE_VERSION) + qFromUtf8("（gh-proxy.com 加速）"));
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
        }
    }
}
void V2RayLogic::onCoreDlDone() {
    if (!dlProc) { return; }
    delete dlProc;
    dlProc = 0;
    coreDlBusy = false;
    if (!fileExists(zipPath())) {
        appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("下载失败（未生成 zip 文件），请检查网络或 gh-proxy.com 可用性"));
        refreshCoreState();
        return;
    }
    /* 文件大小过小视为下载不完整 */
    QFileInfo fi(zipPath());
    if ((long)fi.size() < 1024 * 1024) {
        appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("下载结果异常（zip 小于 1MB），可能下载不完整，已删除重试入口"));
        QFile::remove(zipPath());
        refreshCoreState();
        return;
    }
    appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("下载完成（进度 100%），正在解压..."));
    if (unzipProc) { delete unzipProc; unzipProc = 0; }
    unzipProc = new QProcess(this);
    connect(unzipProc, SIGNAL(processExited()), this, SLOT(onCoreUnzipDone()));
    unzipProc->addArgument("unzip");
    unzipProc->addArgument("-o");
    unzipProc->addArgument(zipPath());
    unzipProc->addArgument("-d");
    unzipProc->addArgument(coreDir());
    unzipProc->start();
}
void V2RayLogic::onCoreUnzipDone() {
    if (!unzipProc) { return; }
    delete unzipProc;
    unzipProc = 0;
    chmod(QFile::encodeName(coreBinaryPath()).data(), 0755);
    QFile::remove(zipPath());
    refreshCoreState();
    if (coreInstalled()) {
        if (hasGeoFiles()) {
            appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("v2ray-core ") + qFromUtf8(V2CORE_VERSION) + qFromUtf8(" 安装完成（geoip.dat / geosite.dat 已就绪）"));
        } else {
            appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("v2ray-core 安装完成，但缺少 geoip.dat / geosite.dat"));
        }
    } else {
        appendLogWithPrefix(qFromUtf8("[核心下载]"), qFromUtf8("v2ray-core 安装失败（未找到 v2ray 可执行文件）"));
    }
}

void V2RayLogic::onStartCore() {
    if (!coreInstalled()) { appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("核心未安装，请先在“核心”页下载安装")); return; }
    if (coreProc && coreProc->isRunning()) { appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("核心已在运行")); return; }
    int idx = selectedNodeIndex();
    if (idx < 0) {
        if (nodeCount >= 1) { idx = 0; } else { appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("没有可用节点，请先更新订阅")); return; }
    }
    V2Node node = nodes[idx];
    QString err;
    if (!generateConfig(node, err)) { appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("生成配置失败：") + err); return; }
    if (coreProc) { delete coreProc; coreProc = 0; }
    coreProc = new QProcess(this);
    connect(coreProc, SIGNAL(readyReadStdout()), this, SLOT(onCoreOutput()));
    connect(coreProc, SIGNAL(readyReadStderr()), this, SLOT(onCoreErrOutput()));
    connect(coreProc, SIGNAL(processExited()), this, SLOT(onCoreExit()));
    QDir wd(coreDir());
    coreProc->setWorkingDirectory(wd);
    coreProc->addArgument("./v2ray");
    coreProc->addArgument("run");
    coreProc->addArgument("-c");
    coreProc->addArgument("config_run.json");
    coreProc->start();
    refreshCoreState();
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
    appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("核心已退出"));
    refreshCoreState();
}
void V2RayLogic::onStopCore() {
    if (!coreProc || !coreProc->isRunning()) { appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("核心未在运行")); return; }
    coreProc->tryTerminate();
    appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("已发送停止请求"));
    refreshCoreState();
}
void V2RayLogic::onCopyNodeLink() {
    int idx = selectedNodeIndex();
    if (idx < 0) { appendLogWithPrefix(qFromUtf8("[节点]"), qFromUtf8("请先选中要复制的节点")); return; }
    QApplication::clipboard()->setText(nodes[idx].raw);
    appendLogWithPrefix(qFromUtf8("[节点]"), qFromUtf8("已复制节点链接"));
}

/* ── 延迟测试（curl 实现，队列串行，不阻塞 UI） ── */
void V2RayLogic::onTestLatency() {
    int idx = selectedNodeIndex();
    if (idx < 0) { appendLogWithPrefix(qFromUtf8("[延迟测试]"), qFromUtf8("请先选中要测试的节点")); return; }
    V2Node n = nodes[idx];
    bool portOk = false;
    n.port.toInt(&portOk);
    if (n.addr.isEmpty() || !portOk) {
        appendLogWithPrefix(qFromUtf8("[延迟测试]"), qFromUtf8("节点地址/端口无效，跳过"));
        return;
    }
    if (latBusy && curLatIndex == idx) {
        appendLogWithPrefix(qFromUtf8("[延迟测试]"), qFromUtf8("该节点正在测试中"));
        return;
    }
    for (int i = 0; i < latQueueCount; ++i) {
        if (latQueueArr[i] == idx) {
            appendLogWithPrefix(qFromUtf8("[延迟测试]"), qFromUtf8("该节点已在测试队列中"));
            return;
        }
    }
    if (latQueueCount < 512) {
        latQueueArr[latQueueCount] = idx;
        ++latQueueCount;
    } else {
        appendLogWithPrefix(qFromUtf8("[延迟测试]"), qFromUtf8("测试队列已满"));
        return;
    }
    appendLogWithPrefix(qFromUtf8("[延迟测试]"), qFromUtf8("已入队：") + n.name + " (" + n.addr + ":" + n.port + ")");
    if (!latBusy) { startNextLatency(); }
}

void V2RayLogic::startNextLatency() {
    if (latBusy) { return; }
    if (latQueueCount <= 0) { return; }
    int idx = latQueueArr[0];
    for (int i = 1; i < latQueueCount; ++i) { latQueueArr[i - 1] = latQueueArr[i]; }
    --latQueueCount;
    V2Node n = nodes[idx];
    curLatIndex = idx;
    latGotConnected = false;
    latErrData.resize(0);
    if (latProc) { delete latProc; latProc = 0; }
    latProc = new QProcess(this);
    connect(latProc, SIGNAL(readyReadStdout()), this, SLOT(onLatOutput()));
    connect(latProc, SIGNAL(readyReadStderr()), this, SLOT(onLatErrOutput()));
    connect(latProc, SIGNAL(processExited()), this, SLOT(onLatExited()));
    latProc->addArgument("curl");
    latProc->addArgument("-sSv");
    latProc->addArgument("--connect-timeout");
    latProc->addArgument("3");
    latProc->addArgument("--max-time");
    latProc->addArgument("4");
    latProc->addArgument("-o");
    latProc->addArgument("/dev/null");
    latProc->addArgument("telnet://" + n.addr + ":" + n.port);
    latTime.start();
    latBusy = true;
    latProc->start();
    appendLogWithPrefix(qFromUtf8("[延迟测试]"), qFromUtf8("测试中（TCP，最长 3s）：") + n.addr + ":" + n.port);
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
        nodeLatencyKnown[curLatIndex] = true;
        nodeLatencyMs[curLatIndex] = latTime.elapsed();
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
    if (idx < 0) { return; }
    if (latGotConnected) {
        appendLogWithPrefix(qFromUtf8("[延迟测试]"),
            nodes[idx].name + " (" + nodes[idx].addr + ":" + nodes[idx].port + ") "
            + QString::number(nodeLatencyMs[idx]) + "ms");
    } else {
        nodeLatencyKnown[idx] = true;
        nodeLatencyMs[idx] = -1;
        QString s = tailStr(latErrData, 4096);
        QString errLine = lastNeedleLine(s, "curl:");
        QString hint;
        if (!errLine.isEmpty()) { hint = errLine; }
        else if (exitCode == 28) { hint = qFromUtf8("连接超时"); }
        else if (exitCode == 7) { hint = qFromUtf8("连接被拒绝/不可达"); }
        else if (exitCode == 6) { hint = qFromUtf8("域名解析失败"); }
        else { hint = qFromUtf8("curl exit=") + QString::number(exitCode); }
        appendLogWithPrefix(qFromUtf8("[延迟测试]"),
            nodes[idx].name + " (" + nodes[idx].addr + ":" + nodes[idx].port + ") "
            + qFromUtf8("失败：") + hint);
    }
    refreshNodeList();
    startNextLatency();
}

/* ── 生成 config_run.json ── */
bool V2RayLogic::generateConfig(const V2Node& node, QString& err) {
    bool portOk = false;
    int portNum = node.port.toInt(&portOk);
    if (!portOk || portNum <= 0 || portNum > 65535) { err = qFromUtf8("端口非法"); return false; }
    if (node.proto != "vmess" && node.proto != "vless" && node.proto != "trojan") {
        err = qFromUtf8("暂不支持生成该协议配置：") + node.proto;
        return false;
    }
    if (coreDir().length() <= 0) { err = qFromUtf8("核心目录缺失"); return false; }
    QString net = node.net;
    if (net.isEmpty()) { net = "tcp"; }
    QString stream;
    stream += "\"streamSettings\":{\"network\":\"" + net + "\"";
    if (!node.tls.isEmpty() && node.tls != "none") {
        stream += ",\"security\":\"tls\"";
    }
    if (net == "ws") {
        QString path = node.path;
        if (path.isEmpty()) { path = "/"; }
        QString host = node.host;
        if (host.isEmpty()) { host = node.addr; }
        stream += ",\"wsSettings\":{\"path\":\"" + path + "\",\"headers\":{\"Host\":\"" + host + "\"}}";
    }
    stream += "}";
    QString outbound;
    if (node.proto == "vmess") {
        outbound += "{\"protocol\":\"vmess\",\"settings\":{\"vnext\":[{\"address\":\"" + node.addr
            + "\",\"port\":" + node.port
            + ",\"users\":[{\"id\":\"" + node.uuid
            + "\",\"alterId\":0,\"security\":\"auto\"}]}]},";
    } else if (node.proto == "vless") {
        outbound += "{\"protocol\":\"vless\",\"settings\":{\"vnext\":[{\"address\":\"" + node.addr
            + "\",\"port\":" + node.port
            + ",\"users\":[{\"id\":\"" + node.uuid
            + "\",\"encryption\":\"none\"}]}]},";
    } else {
        outbound += "{\"protocol\":\"trojan\",\"settings\":{\"servers\":[{\"address\":\"" + node.addr
            + "\",\"port\":" + node.port
            + ",\"password\":\"" + node.uuid + "\"}]},";
    }
    outbound += stream;
    QString cfg = "{\"log\":{\"loglevel\":\"warning\"},"
        "\"inbounds\":[{\"port\":10808,\"listen\":\"127.0.0.1\",\"protocol\":\"socks\",\"settings\":{\"udp\":true}}],"
        "\"outbounds\":[" + outbound + "]";
    if (bypassCnCheck && bypassCnCheck->isChecked()) {
        if (hasGeoFiles()) {
            /* 绕过中国大陆：geosite:cn / geoip:cn 直连，其余走当前节点（引用核心包自带的 geo 文件） */
            cfg += ",\"routing\":{\"domainStrategy\":\"IPIfNonMatch\",\"rules\":["
                "{\"type\":\"field\",\"outboundTag\":\"direct\",\"domain\":[\"geosite:cn\"]},"
                "{\"type\":\"field\",\"outboundTag\":\"direct\",\"ip\":[\"geoip:cn\",\"geoip:private\"]},"
                "{\"type\":\"field\",\"outboundTag\":\"proxy\",\"network\":\"tcp,udp\"}"
                "]}";
            appendLogWithPrefix(qFromUtf8("[核心]"), qFromUtf8("已启用绕过中国大陆路由（geosite:cn / geoip:cn 直连，其余走代理）"));
        } else {
            appendLogWithPrefix(qFromUtf8("[核心]"),
                qFromUtf8("警告：未找到 core/geoip.dat 或 core/geosite.dat，绕过中国大陆路由未启用。请重新“下载安装”以获取完整 v2ray-core ") + qFromUtf8(V2CORE_VERSION) + qFromUtf8(" 包"));
        }
    }
    cfg += "}";
    writeUtf8File(coreDir() + "/config_run.json", cfg);
    return true;
}

/* ── 核心状态 ── */
void V2RayLogic::refreshCoreState() {
    if (!coreState || !startBtn || !stopBtn) { return; }
    QString s;
    if (coreProc && coreProc->isRunning()) {
        s = qFromUtf8("状态：核心运行中（本地 SOCKS 127.0.0.1:10808）");
    } else if (coreInstalled()) {
        s = qFromUtf8("状态：已安装 v2ray-core（固定 ") + qFromUtf8(V2CORE_VERSION) + qFromUtf8("）");
    } else {
        s = qFromUtf8("状态：未安装 v2ray-core，请点击“下载安装”获取 ") + qFromUtf8(V2CORE_VERSION);
    }
    coreState->setText(s);
    startBtn->setEnabled(coreInstalled());
    stopBtn->setEnabled(coreProc && coreProc->isRunning());
}
