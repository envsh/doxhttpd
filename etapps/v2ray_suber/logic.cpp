#include "compat34.h"
#include "logic.h"
#include "cJSON.h"

#include <qdir.h>
#include <qfile.h>
#include <qlineedit.h>
#include <qlabel.h>
#include <qtextedit.h>
#include <qpushbutton.h>
#include <qapplication.h>
#include <qclipboard.h>
#include <qprocess.h>

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

/* ── base64 解码 ── */
static int b64Val(char c) {
    if (c >= 'A' && c <= 'Z') { return c - 'A'; }
    if (c >= 'a' && c <= 'z') { return c - 'a' + 26; }
    if (c >= '0' && c <= '9') { return c - '0' + 52; }
    if (c == '+') { return 62; }
    if (c == '/') { return 63; }
    return -1;
}
void V2RayLogic::b64Decode(const QString& in, QByteArray& out) {
    out.resize(0);
    int acc = 0;
    int bits = 0;
    for (int i = 0; i < (int)in.length(); ++i) {
        int v = b64Val(in[i].latin1());
        if (v < 0) { continue; }
        acc = (acc << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.resize(out.size() + 1);
            out[(int)out.size() - 1] = (char)((acc >> bits) & 0xFF);
        }
    }
}

/* ───────────────────────────────────────────── */

V2RayLogic::V2RayLogic(QObject* parent)
    : QObject(parent) {
    nodeCount = 0;
    subProc = 0;
    dlProc = 0;
    unzipProc = 0;
    coreProc = 0;
    coreDlBusy = false;
    subEdit = 0;
    subList = 0;
    nodeList = 0;
    coreState = 0;
    log = 0;
    startBtn = 0;
    stopBtn = 0;
    ensureDirs();
    loadSubscriptions();
    if (subList) { refreshSubList(); }
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
        lbAdd(nodeList, show);
    }
}

void V2RayLogic::appendLog(const QString& msg) {
    if (!log) { return; }
    log->append(msg);
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
    if (url.isEmpty()) { appendLog(qFromUtf8("订阅地址为空")); return; }
    if (sFind(url, ':', 0) <= 0
        || (url.left(7) != "http://" && url.left(8) != "https://")) {
        appendLog(qFromUtf8("订阅地址需以 http:// 或 https:// 开头"));
        return;
    }
    for (int i = 0; i < (int)subs.count(); ++i) {
        if (subs[i] == url) { appendLog(qFromUtf8("该订阅已存在")); return; }
    }
    subs.append(url);
    saveSubscriptions();
    refreshSubList();
    appendLog(qFromUtf8("已添加订阅：") + url);
}
void V2RayLogic::onDelSubscription() {
    int idx = selectedSubIndex();
    if (idx < 0) { appendLog(qFromUtf8("请先选中要删除的订阅")); return; }
    appendLog(qFromUtf8("已删除订阅：") + subs[idx]);
    subs.remove(subs.at(idx));
    saveSubscriptions();
    refreshSubList();
}
void V2RayLogic::onUpdateSubscription() {
    if (subProc && subProc->isRunning()) { appendLog(qFromUtf8("上一次订阅更新仍在进行中")); return; }
    int idx = selectedSubIndex();
    if (idx < 0) {
        if ((int)subs.count() >= 1) { idx = 0; } else { appendLog(qFromUtf8("没有可用订阅，请先添加")); return; }
    }
    QString url = subs[idx];
    if (subProc) { delete subProc; subProc = 0; }
    subData.resize(0);
    subProc = new QProcess(this);
    connect(subProc, SIGNAL(readyReadStdout()), this, SLOT(onSubOutput()));
    connect(subProc, SIGNAL(processExited()), this, SLOT(onSubFetched()));
    subProc->addArgument("curl");
    subProc->addArgument("-sL");
    subProc->addArgument("--max-time");
    subProc->addArgument("30");
    subProc->addArgument(url);
    subProc->start();
    appendLog(qFromUtf8("正在更新订阅：") + url);
}
void V2RayLogic::onSubOutput() {
    if (!subProc) { return; }
    baAppend(subData, subProc->readStdout());
}
void V2RayLogic::onSubFetched() {
    if (!subProc) { return; }
    baAppend(subData, subProc->readStdout());
    delete subProc;
    subProc = 0;
    QString content = qFromUtf8(subData.data(), (int)subData.size());
    if (content.isEmpty()) { appendLog(qFromUtf8("订阅下载为空，请检查网络")); return; }
    QString plain = maybeDecodeBase64(content);
    saveSubLast(plain);
    clearNodes();
    parseSubscription(plain);
    refreshNodeList();
    appendLog(qFromUtf8("订阅更新完成，共解析 ") + QString::number(nodeCount) + qFromUtf8(" 个节点"));
}

QString V2RayLogic::maybeDecodeBase64(const QString& in) {
    return in;
}

void V2RayLogic::clearNodes() {
    nodeCount = 0;
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
    QByteArray raw;
    b64Decode(line.mid(8), raw);
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
    if (dlProc) { delete dlProc; dlProc = 0; }
    dlProc = new QProcess(this);
    connect(dlProc, SIGNAL(readyReadStdout()), this, SLOT(onCoreDlOutput()));
    connect(dlProc, SIGNAL(processExited()), this, SLOT(onCoreDlDone()));
    dlProc->addArgument("curl");
    dlProc->addArgument("-L");
    dlProc->addArgument("-o");
    dlProc->addArgument(zipPath());
    dlProc->addArgument("--max-time");
    dlProc->addArgument("120");
    dlProc->addArgument(url);
    dlProc->start();
    appendLog(qFromUtf8("正在下载 v2ray-core ") + qFromUtf8(V2CORE_VERSION) + qFromUtf8("（gh-proxy.com 加速）"));
    appendLog(qFromUtf8("URL：") + url);
}
void V2RayLogic::onCoreDlOutput() {
    /* 输出仅做日志展示，不解析 */
}
void V2RayLogic::onCoreDlDone() {
    if (!dlProc) { return; }
    delete dlProc;
    dlProc = 0;
    coreDlBusy = false;
    if (!fileExists(zipPath())) {
        appendLog(qFromUtf8("核心下载失败（未生成 zip 文件），请检查网络或 gh-proxy.com 可用性"));
        refreshCoreState();
        return;
    }
    appendLog(qFromUtf8("下载完成，正在解压..."));
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
        appendLog(qFromUtf8("v2ray-core ") + qFromUtf8(V2CORE_VERSION) + qFromUtf8(" 安装完成"));
    } else {
        appendLog(qFromUtf8("v2ray-core 安装失败（未找到 v2ray 可执行文件）"));
    }
}

void V2RayLogic::onStartCore() {
    if (!coreInstalled()) { appendLog(qFromUtf8("核心未安装，请先在“核心”页下载安装")); return; }
    if (coreProc && coreProc->isRunning()) { appendLog(qFromUtf8("核心已在运行")); return; }
    int idx = selectedNodeIndex();
    if (idx < 0) {
        if (nodeCount >= 1) { idx = 0; } else { appendLog(qFromUtf8("没有可用节点，请先更新订阅")); return; }
    }
    V2Node node = nodes[idx];
    QString err;
    if (!generateConfig(node, err)) { appendLog(qFromUtf8("生成配置失败：") + err); return; }
    if (coreProc) { delete coreProc; coreProc = 0; }
    coreProc = new QProcess(this);
    connect(coreProc, SIGNAL(readyReadStdout()), this, SLOT(onCoreOutput()));
    connect(coreProc, SIGNAL(processExited()), this, SLOT(onCoreExit()));
    QDir wd(coreDir());
    coreProc->setWorkingDirectory(wd);
    coreProc->addArgument("./v2ray");
    coreProc->addArgument("run");
    coreProc->addArgument("-c");
    coreProc->addArgument("config_run.json");
    coreProc->start();
    refreshCoreState();
    appendLog(qFromUtf8("核心启动中（节点：" ) + node.name + qFromUtf8("）"));
}
void V2RayLogic::onCoreOutput() {
    if (!coreProc) { return; }
    QByteArray out = coreProc->readStdout();
    if ((int)out.size() <= 0) { return; }
    appendLog(qFromUtf8(out.data(), (int)out.size()));
}
void V2RayLogic::onCoreExit() {
    appendLog(qFromUtf8("核心已退出"));
    refreshCoreState();
}
void V2RayLogic::onStopCore() {
    if (!coreProc || !coreProc->isRunning()) { appendLog(qFromUtf8("核心未在运行")); return; }
    coreProc->tryTerminate();
    appendLog(qFromUtf8("已发送停止请求"));
    refreshCoreState();
}
void V2RayLogic::onCopyNodeLink() {
    int idx = selectedNodeIndex();
    if (idx < 0) { appendLog(qFromUtf8("请先选中要复制的节点")); return; }
    QApplication::clipboard()->setText(nodes[idx].raw);
    appendLog(qFromUtf8("已复制节点链接"));
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
        "\"outbounds\":[" + outbound + "]}";
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
