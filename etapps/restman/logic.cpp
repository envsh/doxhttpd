#include "compat34.h"
#include "logic.h"
#include "cJSON.h"

#include <qdir.h>
#include <qfile.h>
#include <qlineedit.h>
#include <qlabel.h>
#include <qtextedit.h>
#include <qcombobox.h>
#include <qpushbutton.h>
#include <qapplication.h>
#include <qprocess.h>

#ifdef QT3_BUILD
#include <qlistbox.h>
#else
#include <qlistwidget.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

/* ── Qt3/Qt4 小工具 ── */
#ifdef QT3_BUILD
static QString rmTrim(const QString& s) { return s.stripWhiteSpace(); }
#else
static QString rmTrim(const QString& s) { return s.trimmed(); }
#endif
static void lbAdd(RmListBox* lb, const QString& s) {
#ifdef QT3_BUILD
    lb->insertItem(s);
#else
    lb->addItem(s);
#endif
}
static void lbClear(RmListBox* lb) { lb->clear(); }
static int lbCurrent(RmListBox* lb) {
#ifdef QT3_BUILD
    return lb->currentItem();
#else
    return lb->currentRow();
#endif
}
#ifdef QT3_BUILD
static QString rmText(const QTextEdit* e) { return e->text(); }
#else
static QString rmText(const QTextEdit* e) { return e->toPlainText(); }
#endif
#ifdef QT3_BUILD
static void rmSetText(QTextEdit* e, const QString& s) { e->setText(s); }
#else
static void rmSetText(QTextEdit* e, const QString& s) { e->setPlainText(s); }
#endif
#ifdef QT3_BUILD
static QString rmComboText(const QComboBox* b, int i) { return b->text(i); }
static int rmComboCount(const QComboBox* b) { return (int)b->count(); }
static void rmComboSetCurrent(QComboBox* b, int i) { b->setCurrentItem(i); }
#else
static QString rmComboText(const QComboBox* b, int i) { return b->itemText(i); }
static int rmComboCount(const QComboBox* b) { return (int)b->count(); }
static void rmComboSetCurrent(QComboBox* b, int i) { b->setCurrentIndex(i); }
#endif
static bool rmComboSelect(QComboBox* b, const QString& t) {
    if (!b) { return false; }
    for (int i = 0; i < rmComboCount(b); ++i) {
        if (rmComboText(b, i) == t) {
            rmComboSetCurrent(b, i);
            return true;
        }
    }
    return false;
}


/* Qt3/Qt4 utf8 传出 */
static QByteArray rmUtf8(const QString& s) {
#ifdef QT3_BUILD
    return s.utf8();
#else
    return s.toUtf8();
#endif
}
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
    return s.indexOf(QString::fromLatin1(sub), from);
#endif
}

static void writeUtf8File(const QString& path, const QString& content) {
    QByteArray u = rmUtf8(content);
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
static void baAppend(QByteArray& dst, const QByteArray& src) {
    if (src.size() <= 0) { return; }
    int old = (int)dst.size();
    dst.resize(old + (int)src.size());
    memcpy(dst.data() + old, src.data(), (size_t)src.size());
}
/* ── curl -i 响应解析：首段为 HTTP 状态行 + 响应头，其后为 body ── */
static void splitResponse(const QString& raw, QString& status, QString& hdrs, QString& body) {
    int sep = -1;
    int len = (int)raw.length();
    /* 查找首个空行位置（\r\n\r\n 或 \n\n） */
    for (int i = 0; i < len - 1; ++i) {
        char c1 = raw[i].latin1();
        char c2 = raw[i + 1].latin1();
        if (c1 == '\n' && c2 == '\n') { sep = i; break; }
        if (c1 == '\r' && c2 == '\n' && i + 3 < len
            && raw[i + 2].latin1() == '\r' && raw[i + 3].latin1() == '\n') { sep = i; break; }
    }
    if (sep < 0) {
        hdrs = raw;
        return;
    }
    int skip = 2;
    if (sep + 3 < len && raw[sep].latin1() == '\r') { skip = 4; }
    hdrs = raw.left(sep);
    int nlPos = sep + skip;
    if (nlPos < len) { body = raw.mid(nlPos); }
    int firstNl = -1;
    for (int i = 0; i < (int)hdrs.length(); ++i) {
        if (hdrs[i].latin1() == '\n') { firstNl = i; break; }
    }
    if (firstNl >= 0) {
        status = hdrs.left(firstNl);
#ifdef QT3_BUILD
        status = status.stripWhiteSpace();
#else
        status = status.trimmed();
#endif
        hdrs = hdrs.mid(firstNl + 1);
    } else {
        status = hdrs;
        hdrs = QString();
    }
}
/* JSON 工具 */
static QString jStr(const cJSON* root, const char* key) {
    if (!root) { return QString(); }
    cJSON* j = cJSON_GetObjectItem(root, key);
    if (!j || !j->valuestring) { return QString(); }
    return qFromUtf8(j->valuestring);
}

/* ───────────────────────────────────────────── */

RestmanLogic::RestmanLogic(QObject* parent)
    : QObject(parent) {
    histCount = 0;
    bmCount = 0;
    reqProc = 0;
    curHistIdx = -1;
    curBmIdx = -1;
    methodBox = 0;
    urlEdit = 0;
    queryEdit = 0;
    headerEdit = 0;
    bodyTypeBox = 0;
    bodyEdit = 0;
    authTypeBox = 0;
    authUserEdit = 0;
    authPassEdit = 0;
    histList = 0;
    bmList = 0;
    respState = 0;
    respBody = 0;
    respHeaders = 0;
    respRaw = 0;
    sendBtn = 0;
    ensureDirs();
    loadHistory();
    loadBookmarks();
}

void RestmanLogic::setWidgets(QComboBox* methodBox_, QLineEdit* urlEdit_,
                              QTextEdit* queryEdit_, QTextEdit* headerEdit_,
                              QComboBox* bodyTypeBox_, QTextEdit* bodyEdit_,
                              QComboBox* authTypeBox_, QLineEdit* authUserEdit_, QLineEdit* authPassEdit_,
                              RmListBox* histList_, RmListBox* bmList_,
                              QLabel* respState_, QTextEdit* respBody_,
                              QTextEdit* respHeaders_, QTextEdit* respRaw_,
                              QPushButton* sendBtn_) {
    methodBox = methodBox_;
    urlEdit = urlEdit_;
    queryEdit = queryEdit_;
    headerEdit = headerEdit_;
    bodyTypeBox = bodyTypeBox_;
    bodyEdit = bodyEdit_;
    authTypeBox = authTypeBox_;
    authUserEdit = authUserEdit_;
    authPassEdit = authPassEdit_;
    histList = histList_;
    bmList = bmList_;
    respState = respState_;
    respBody = respBody_;
    respHeaders = respHeaders_;
    respRaw = respRaw_;
    sendBtn = sendBtn_;
    refreshHistList();
    refreshBmList();
    appendLogWithPrefix(qFromUtf8("[Restman]"), qFromUtf8("REST 请求管理器已就绪（历史保存最近 30 条，收藏保留完整参数）"));
}

QString RestmanLogic::baseDir() const {
#ifdef QT3_BUILD
    QString home = QDir::homeDirPath();
#else
    QString home = QDir::homePath();
#endif
    return home + "/.config/" RM_BASE_DIR_NAME;
}
void RestmanLogic::ensureDirs() {
    QString home;
#ifdef QT3_BUILD
    home = QDir::homeDirPath();
#else
    home = QDir::homePath();
#endif
    mkdir(rmUtf8(home + "/.config").data(), 0755);
    mkdir(rmUtf8(baseDir()).data(), 0755);
}

void RestmanLogic::appendLogWithPrefix(const QString& prefix, const QString& msg) {
    if (!respRaw) { return; }
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString line;
    if (prefix.isEmpty()) {
        line = "[" + ts + "] " + msg;
    } else {
        line = "[" + ts + "] " + prefix + " " + msg;
    }
    respRaw->append(line);
}

/* ── 持久化 ── */
void RestmanLogic::loadHistory() {
    histCount = 0;
    QString data = readUtf8File(baseDir() + "/history.json");
    if (data.isEmpty()) { return; }
    QByteArray b = rmUtf8(data);
    cJSON* root = cJSON_Parse(b.data());
    if (!root) { return; }
    cJSON* list = cJSON_GetObjectItem(root, "history");
    if (list && list->type == cJSON_Array) {
        int n = cJSON_GetArraySize(list);
        for (int i = n - 1; i >= 0; --i) {
            cJSON* o = cJSON_GetArrayItem(list, i);
            if (!o) { continue; }
            RestManSnap s;
            s.method = jStr(o, "method");
            s.url = jStr(o, "url");
            s.query = jStr(o, "query");
            s.headers = jStr(o, "headers");
            s.bodyType = jStr(o, "bodyType");
            s.bodyText = jStr(o, "bodyText");
            s.authType = jStr(o, "authType");
            s.authUser = jStr(o, "authUser");
            s.authPass = jStr(o, "authPass");
            s.createdAt = jStr(o, "createdAt");
            if (s.url.isEmpty()) { continue; }
            if (histCount < RM_HIST_CAP) {
                histArr[histCount] = s;
                ++histCount;
            }
        }
    }
    cJSON_Delete(root);
}
void RestmanLogic::loadBookmarks() {
    bmCount = 0;
    QString data = readUtf8File(baseDir() + "/bookmarks.json");
    if (data.isEmpty()) { return; }
    QByteArray b = rmUtf8(data);
    cJSON* root = cJSON_Parse(b.data());
    if (!root) { return; }
    cJSON* list = cJSON_GetObjectItem(root, "bookmarks");
    if (list && list->type == cJSON_Array) {
        int n = cJSON_GetArraySize(list);
        for (int i = 0; i < n; ++i) {
            cJSON* o = cJSON_GetArrayItem(list, i);
            if (!o) { continue; }
            RestManSnap s;
            s.method = jStr(o, "method");
            s.url = jStr(o, "url");
            s.query = jStr(o, "query");
            s.headers = jStr(o, "headers");
            s.bodyType = jStr(o, "bodyType");
            s.bodyText = jStr(o, "bodyText");
            s.authType = jStr(o, "authType");
            s.authUser = jStr(o, "authUser");
            s.authPass = jStr(o, "authPass");
            s.createdAt = jStr(o, "createdAt");
            if (s.url.isEmpty()) { continue; }
            if (bmCount < RM_BOOKMARK_CAP) {
                bmArr[bmCount] = s;
                ++bmCount;
            }
        }
    }
    cJSON_Delete(root);
}
static void snapToJson(const RestManSnap& s, cJSON* o) {
    cJSON_AddStringToObject(o, "method", rmUtf8(s.method).data());
    cJSON_AddStringToObject(o, "url", rmUtf8(s.url).data());
    cJSON_AddStringToObject(o, "query", rmUtf8(s.query).data());
    cJSON_AddStringToObject(o, "headers", rmUtf8(s.headers).data());
    cJSON_AddStringToObject(o, "bodyType", rmUtf8(s.bodyType).data());
    cJSON_AddStringToObject(o, "bodyText", rmUtf8(s.bodyText).data());
    cJSON_AddStringToObject(o, "authType", rmUtf8(s.authType).data());
    cJSON_AddStringToObject(o, "authUser", rmUtf8(s.authUser).data());
    cJSON_AddStringToObject(o, "authPass", rmUtf8(s.authPass).data());
    cJSON_AddStringToObject(o, "createdAt", rmUtf8(s.createdAt).data());
}
void RestmanLogic::saveHistory() {
    cJSON* root = cJSON_CreateObject();
    cJSON* list = cJSON_CreateArray();
    for (int i = 0; i < histCount; ++i) {
        cJSON* o = cJSON_CreateObject();
        snapToJson(histArr[i], o);
        cJSON_AddItemToArray(list, o);
    }
    cJSON_AddItemToObject(root, "history", list);
    char* out = cJSON_Print(root);
    if (out) {
        writeUtf8File(baseDir() + "/history.json", qFromUtf8(out));
        free(out);
    }
    cJSON_Delete(root);
}
void RestmanLogic::saveBookmarks() {
    cJSON* root = cJSON_CreateObject();
    cJSON* list = cJSON_CreateArray();
    for (int i = 0; i < bmCount; ++i) {
        cJSON* o = cJSON_CreateObject();
        snapToJson(bmArr[i], o);
        cJSON_AddItemToArray(list, o);
    }
    cJSON_AddItemToObject(root, "bookmarks", list);
    char* out = cJSON_Print(root);
    if (out) {
        writeUtf8File(baseDir() + "/bookmarks.json", qFromUtf8(out));
        free(out);
    }
    cJSON_Delete(root);
}

/* ── 列表刷新 ── */
static QString snapLine(const RestManSnap& s) {
    QString t = s.createdAt;
    if (t.length() > 11) { t = t.mid(11); } /* 只留 hh:mm:ss 段 */
    return s.method + " " + s.url + "  " + t;
}
void RestmanLogic::refreshHistList() {
    if (!histList) { return; }
    lbClear(histList);
    for (int i = 0; i < histCount; ++i) {
        lbAdd(histList, snapLine(histArr[i]));
    }
}
void RestmanLogic::refreshBmList() {
    if (!bmList) { return; }
    lbClear(bmList);
    for (int i = 0; i < bmCount; ++i) {
        lbAdd(bmList, snapLine(bmArr[i]));
    }
}

/* ── 快照 ── */
void RestmanLogic::captureSnapshot(RestManSnap& s) {
    s.method = methodBox ? methodBox->currentText() : QString("GET");
    s.url = urlEdit ? rmTrim(urlEdit->text()) : QString();
    s.query = queryEdit ? rmText(queryEdit) : QString();
    s.headers = headerEdit ? rmText(headerEdit) : QString();
    s.bodyType = bodyTypeBox ? bodyTypeBox->currentText() : QString("none");
    if (s.bodyType == qFromUtf8("无")) { s.bodyType = "none"; }
    else if (s.bodyType == qFromUtf8("原始(raw)")) { s.bodyType = "raw"; }
    else if (s.bodyType == qFromUtf8("表单(urlencoded)")) { s.bodyType = "urlencoded"; }
    s.bodyText = bodyEdit ? rmText(bodyEdit) : QString();
    s.authType = authTypeBox ? authTypeBox->currentText() : QString("none");
    if (s.authType == qFromUtf8("无")) { s.authType = "none"; }
    else if (s.authType == qFromUtf8("Basic")) { s.authType = "basic"; }
    s.authUser = authUserEdit ? authUserEdit->text() : QString();
    s.authPass = authPassEdit ? authPassEdit->text() : QString();
    s.createdAt = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
}
void RestmanLogic::applySnapshot(const RestManSnap& s) {
    rmComboSelect(methodBox, s.method);
    if (urlEdit) { urlEdit->setText(s.url); }
    if (queryEdit) { rmSetText(queryEdit, s.query); }
    if (headerEdit) { rmSetText(headerEdit, s.headers); }
    if (bodyTypeBox) {
        QString t = s.bodyType;
        if (t == "none") { t = qFromUtf8("无"); }
        else if (t == "raw") { t = qFromUtf8("原始(raw)"); }
        else if (t == "urlencoded") { t = qFromUtf8("表单(urlencoded)"); }
        rmComboSelect(bodyTypeBox, t);
    }
    if (bodyEdit) { rmSetText(bodyEdit, s.bodyText); }
    if (authTypeBox) {
        QString t = s.authType;
        if (t == "none") { t = qFromUtf8("无"); }
        else if (t == "basic") { t = qFromUtf8("Basic"); }
        rmComboSelect(authTypeBox, t);
    }
    if (authUserEdit) { authUserEdit->setText(s.authUser); }
    if (authPassEdit) { authPassEdit->setText(s.authPass); }
}

/* ── 历史加入（上限 30，保留最新） ── */
void RestmanLogic::addHistory(const RestManSnap& s) {
    /* 与最近一条完全相同则跳过 */
    if (histCount >= 1) {
        const RestManSnap& last = histArr[histCount - 1];
        if (last.method == s.method && last.url == s.url && last.query == s.query
            && last.headers == s.headers && last.bodyText == s.bodyText
            && last.authUser == s.authUser && last.authPass == s.authPass) {
            return;
        }
    }
    if (histCount < RM_HIST_CAP) {
        for (int i = histCount; i > 0; --i) { histArr[i] = histArr[i - 1]; }
        histArr[0] = s;
        ++histCount;
    } else {
        for (int i = histCount - 1; i > 0; --i) { histArr[i] = histArr[i - 1]; }
        histArr[0] = s;
    }
    saveHistory();
    refreshHistList();
}

/* ── URL + Query 拼接 ── */
QString RestmanLogic::buildFullUrl(const RestManSnap& s, QString& err) {
    QString url = s.url;
    if (url.isEmpty()) { err = qFromUtf8("URL 为空"); return QString(); }
    QString q = s.query;
    if (!q.isEmpty()) {
        /* 逐行 key=value 拼接 */
        QStringList lines;
#ifdef QT3_BUILD
        lines = QStringList::split('\n', q);
#else
        lines = q.split(QChar('\n'));
#endif
        QString qs;
        for (int i = 0; i < (int)lines.count(); ++i) {
            QString l = rmTrim(lines[i]);
            if (l.isEmpty()) { continue; }
            if (!qs.isEmpty()) { qs += "&"; }
            qs += l;
        }
        if (!qs.isEmpty()) {
            url += (sFind(url, '?', 0) >= 0 ? "&" : "?");
            url += qs;
        }
    }
    return url;
}

/* ── curl 参数构造 ── */
bool RestmanLogic::buildCurlArgs(const RestManSnap& s) {
    QString err;
    QString url = buildFullUrl(s, err);
    if (url.isEmpty()) {
        appendLogWithPrefix(qFromUtf8("[发送]"), qFromUtf8("失败：") + err);
        return false;
    }
    /* 默认携带 -k（测试/内网自签证书场景），超时 10/30s 且 -i 取回响应头 */
    reqProc->addArgument("-sSk");
    reqProc->addArgument("-i");
    reqProc->addArgument("--connect-timeout");
    reqProc->addArgument("10");
    reqProc->addArgument("--max-time");
    reqProc->addArgument("30");
    reqProc->addArgument("-X");
    reqProc->addArgument(s.method);
    /* Headers（每行 Key: Value） */
    QString h = s.headers;
    if (!h.isEmpty()) {
        QStringList lines;
#ifdef QT3_BUILD
        lines = QStringList::split('\n', h);
#else
        lines = h.split(QChar('\n'));
#endif
        for (int i = 0; i < (int)lines.count(); ++i) {
            QString l = rmTrim(lines[i]);
            if (l.isEmpty()) { continue; }
            /* 需要包含冒号才有效 */
            if (sFind(l, ':', 0) < 0) { continue; }
            reqProc->addArgument("-H");
            reqProc->addArgument(l);
        }
    }
    /* Body */
    if (s.bodyType == "raw" && !s.bodyText.isEmpty()) {
        reqProc->addArgument("--data-binary");
        reqProc->addArgument(s.bodyText);
        /* 未显式设置 Content-Type 时补 JSON 默认 */
        if (s.headers.isEmpty() || (sFindStr(s.headers, "ontent-Type", 0) < 0 && sFindStr(s.headers, "ontent-type", 0) < 0)) {
            reqProc->addArgument("-H");
            reqProc->addArgument("Content-Type: application/json");
        }
    } else if (s.bodyType == "urlencoded" && !s.bodyText.isEmpty()) {
        reqProc->addArgument("--data");
        reqProc->addArgument(s.bodyText);
        reqProc->addArgument("-H");
        reqProc->addArgument("Content-Type: application/x-www-form-urlencoded");
    }
    /* Auth */
    if (s.authType == "basic" && !s.authUser.isEmpty()) {
        reqProc->addArgument("--user");
        reqProc->addArgument(s.authUser + ":" + s.authPass);
    }
    reqProc->addArgument(url);
    return true;
}

/* ── 发送（异步，不阻塞 UI） ── */
void RestmanLogic::onSend() {
    if (reqProc && reqProc->isRunning()) {
        appendLogWithPrefix(qFromUtf8("[发送]"), qFromUtf8("上一次请求仍在进行中"));
        return;
    }
    RestManSnap s;
    captureSnapshot(s);
    if (s.url.isEmpty()) {
        appendLogWithPrefix(qFromUtf8("[发送]"), qFromUtf8("URL 为空"));
        return;
    }
    if (reqProc) { delete reqProc; reqProc = 0; }
    reqStdout.resize(0);
    reqStderr.resize(0);
    reqProc = new QProcess(this);
    connect(reqProc, SIGNAL(readyReadStdout()), this, SLOT(onReqOutput()));
    connect(reqProc, SIGNAL(readyReadStderr()), this, SLOT(onReqErrOutput()));
    connect(reqProc, SIGNAL(processExited()), this, SLOT(onReqDone()));
#ifdef QT3_BUILD
    reqProc->clearArguments();
#endif
    reqProc->addArgument("curl");
    if (!buildCurlArgs(s)) {
        delete reqProc;
        reqProc = 0;
        return;
    }
    reqTime.start();
    if (sendBtn) { sendBtn->setEnabled(false); }
    if (respState) { respState->setText(qFromUtf8("状态：请求中...")); }
    reqProc->start();
    appendLogWithPrefix(qFromUtf8("[发送]"), s.method + " " + s.url);
    /* 成功发起即入历史 */
    addHistory(s);
}

void RestmanLogic::onReqOutput() {
    if (!reqProc) { return; }
#ifdef QT3_BUILD
    baAppend(reqStdout, reqProc->readStdout());
#else
    baAppend(reqStdout, reqProc->readAllStandardOutput());
#endif
}
void RestmanLogic::onReqErrOutput() {
    if (!reqProc) { return; }
#ifdef QT3_BUILD
    baAppend(reqStderr, reqProc->readStderr());
#else
    baAppend(reqStderr, reqProc->readAllStandardError());
#endif
}
void RestmanLogic::onReqDone() {
    if (!reqProc) { return; }
#ifdef QT3_BUILD
    baAppend(reqStdout, reqProc->readStdout());
    baAppend(reqStderr, reqProc->readStderr());
    bool normalExit = reqProc->normalExit();
    int exitCode = normalExit ? reqProc->exitStatus() : -1;
#else
    baAppend(reqStdout, reqProc->readAllStandardOutput());
    baAppend(reqStderr, reqProc->readAllStandardError());
    int exitCode = reqProc->exitStatus() == QProcess::NormalExit ? reqProc->exitCode() : -1;
#endif
    delete reqProc;
    reqProc = 0;
    if (sendBtn) { sendBtn->setEnabled(true); }
    int ms = reqTime.elapsed();
    /* stderr 错误提示 */
    if (exitCode != 0) {
        QString es;
        if ((int)reqStderr.size() > 0) {
            es = qFromUtf8(reqStderr.data(), (int)reqStderr.size());
        }
        QString hint;
        if (exitCode == 28) { hint = qFromUtf8("请求超时（max-time 30s）"); }
        else if (exitCode == 7) { hint = qFromUtf8("连接被拒绝/不可达"); }
        else if (exitCode == 6) { hint = qFromUtf8("域名解析失败"); }
        else { hint = qFromUtf8("curl exit=") + QString::number(exitCode); }
        if (respState) { respState->setText(qFromUtf8("状态：失败（") + hint + "）"); }
        appendLogWithPrefix(qFromUtf8("[响应]"), qFromUtf8("请求失败：") + hint);
        if (!es.isEmpty()) {
            appendLogWithPrefix(qFromUtf8("[响应]"), qFromUtf8("curl 输出：") + rmTrim(es));
        }
        return;
    }
    QString raw;
    if ((int)reqStdout.size() > 0) {
        raw = qFromUtf8(reqStdout.data(), (int)reqStdout.size());
    }
    QString status, hdrs, body;
    splitResponse(raw, status, hdrs, body);
    if (respState) { respState->setText(qFromUtf8("状态：") + (status.isEmpty() ? qFromUtf8("(无状态行)") : status) + "  " + QString::number(ms) + "ms"); }
    if (respBody) { rmSetText(respBody, body); }
    if (respHeaders) { rmSetText(respHeaders, hdrs); }
    appendLogWithPrefix(qFromUtf8("[响应]"),
        (status.isEmpty() ? qFromUtf8("(无状态行)") : status) + "  " + QString::number(ms) + "ms  "
        + qFromUtf8("body ") + QString::number((int)body.length()) + qFromUtf8(" 字符"));
}

/* ── 收藏 ── */
void RestmanLogic::onSaveBookmark() {
    if (!urlEdit) { return; }
    RestManSnap s;
    captureSnapshot(s);
    if (s.url.isEmpty()) {
        appendLogWithPrefix(qFromUtf8("[收藏]"), qFromUtf8("URL 为空，未保存"));
        return;
    }
    if (bmCount < RM_BOOKMARK_CAP) {
        for (int i = bmCount; i > 0; --i) { bmArr[i] = bmArr[i - 1]; }
        bmArr[0] = s;
        ++bmCount;
    } else {
        for (int i = bmCount - 1; i > 0; --i) { bmArr[i] = bmArr[i - 1]; }
        bmArr[0] = s;
        appendLogWithPrefix(qFromUtf8("[收藏]"), qFromUtf8("收藏已达上限 ") + QString::number(RM_BOOKMARK_CAP) + qFromUtf8("，最旧条目被替换"));
    }
    saveBookmarks();
    refreshBmList();
    appendLogWithPrefix(qFromUtf8("[收藏]"), qFromUtf8("已保存（含完整参数）：") + s.method + " " + s.url);
}
void RestmanLogic::onDelBookmark() {
    if (!bmList) { return; }
    int idx = lbCurrent(bmList);
    if (idx < 0 || idx >= bmCount) { appendLogWithPrefix(qFromUtf8("[收藏]"), qFromUtf8("请先选中要删除的收藏")); return; }
    appendLogWithPrefix(qFromUtf8("[收藏]"), qFromUtf8("已删除：") + bmArr[idx].method + " " + bmArr[idx].url);
    for (int i = idx; i < bmCount - 1; ++i) { bmArr[i] = bmArr[i + 1]; }
    --bmCount;
    saveBookmarks();
    refreshBmList();
}
void RestmanLogic::onLoadBookmark() {
    if (!bmList) { return; }
    int idx = lbCurrent(bmList);
    if (idx < 0 || idx >= bmCount) { appendLogWithPrefix(qFromUtf8("[收藏]"), qFromUtf8("请先选中要加载的收藏")); return; }
    applySnapshot(bmArr[idx]);
    appendLogWithPrefix(qFromUtf8("[收藏]"), qFromUtf8("已加载到编辑区（含完整参数）：") + bmArr[idx].method + " " + bmArr[idx].url);
}

/* ── 历史 ── */
void RestmanLogic::onLoadHistory() {
    if (!histList) { return; }
    int idx = lbCurrent(histList);
    if (idx < 0 || idx >= histCount) { appendLogWithPrefix(qFromUtf8("[历史]"), qFromUtf8("请先选中要加载的历史")); return; }
    applySnapshot(histArr[idx]);
    appendLogWithPrefix(qFromUtf8("[历史]"), qFromUtf8("已加载到编辑区：") + histArr[idx].method + " " + histArr[idx].url);
}
void RestmanLogic::onDelHistory() {
    if (!histList) { return; }
    int idx = lbCurrent(histList);
    if (idx < 0 || idx >= histCount) { appendLogWithPrefix(qFromUtf8("[历史]"), qFromUtf8("请先选中要删除的历史")); return; }
    for (int i = idx; i < histCount - 1; ++i) { histArr[i] = histArr[i + 1]; }
    --histCount;
    saveHistory();
    refreshHistList();
}
void RestmanLogic::onClearHistory() {
    histCount = 0;
    saveHistory();
    refreshHistList();
    appendLogWithPrefix(qFromUtf8("[历史]"), qFromUtf8("已清空历史"));
}
