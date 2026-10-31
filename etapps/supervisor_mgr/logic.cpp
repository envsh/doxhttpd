#include "compat34.h"
#include "logic.h"
#include "../../qlcomp/cJSON.h"
#include <qfile.h>
#include <qdir.h>
#include <qtextstream.h>
#include <qpainter.h>
#include <qstringlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

static QString rmTrim(const QString& s) {
#ifdef QT3_BUILD
    return s.stripWhiteSpace();
#else
    return s.trimmed();
#endif
}
#ifdef QT3_BUILD
static void rmSetText(QTextEdit* e, const QString& s) { e->setText(s); }
static QString rmGetText(QTextEdit* e) { return e->text(); }
static int rmComboCount(QComboBox* c) { return (int)c->count(); }
static QString rmComboText(QComboBox* c, int i) { return c->text(i); }
#else
static void rmSetText(QTextEdit* e, const QString& s) { e->setPlainText(s); }
static QString rmGetText(QTextEdit* e) { return e->toPlainText(); }
static int rmComboCount(QComboBox* c) { return c->count(); }
static QString rmComboText(QComboBox* c, int i) { return c->itemText(i); }
#endif
static QByteArray rmUtf8(const QString& s) {
#ifdef QT3_BUILD
    return s.utf8();
#else
    return s.toUtf8();
#endif
}
static int sFind(const QString& s, const char* sub, int from) {
#ifdef QT3_BUILD
    return s.find(sub, from);
#else
    return s.indexOf(QString::fromLatin1(sub), from);
#endif
}
static int sFindQ(const QString& s, const QString& sub, int from) {
#ifdef QT3_BUILD
    return s.find(sub, from);
#else
    return s.indexOf(sub, from);
#endif
}
static const char* rmCs(const QString& s) {
#ifdef QT3_BUILD
    return s.latin1();
#else
    return s.toLatin1().constData();
#endif
}
static QString rmPathJoin(const QString& a, const QString& b) {
    if (a.endsWith("/")) { return a + b; }
    return a + "/" + b;
}

/* 前置声明：在整段 XML 中取 <tag>…</tag> 内容 */
static QString extractTag(const QString& xml, const QString& tag, int start);

/* ── 状态颜色（官方状态码映射） ── */
static QColor stateColor(int code) {
    if (code == 20) { return QColor(0, 150, 0); }        /* RUNNING 绿 */
    if (code == 10) { return QColor(200, 120, 0); }      /* STARTING 橙 */
    if (code == 30) { return QColor(200, 120, 0); }      /* BACKOFF 橙 */
    if (code == 40) { return QColor(220, 140, 0); }      /* STOPPING 亮橙 */
    if (code == 100) { return QColor(190, 70, 0); }      /* EXITED 橙红 */
    if (code == 200) { return QColor(210, 0, 0); }       /* FATAL 红 */
    if (code == 0) { return QColor(120, 120, 120); }     /* STOPPED 灰 */
    return QColor(150, 150, 150);                        /* UNKNOWN/其它 */
}

#ifdef QT3_BUILD
/* 单元格文字着色：重写 QTableItem::paint（Qt3 标准做法） */
class ColorTableItem : public QTableItem {
public:
    ColorTableItem(QTable* t, const QString& txt, const QColor& c)
        : QTableItem(t, QTableItem::Never, txt), m_color(c) {}
    void paint(QPainter* p, const QColorGroup& cg, const QRect& cr, bool selected) {
        QColorGroup g(cg);
        g.setColor(QColorGroup::Text, m_color);
        QTableItem::paint(p, g, cr, selected);
    }
private:
    QColor m_color;
};
#endif

SupervisorMgrLogic::SupervisorMgrLogic(QWidget* parent)
    : QObject(parent), m_parent(parent),
      m_host(0), m_port(0), m_user(0), m_pass(0),
      m_procTable(0), m_statusLabel(0),
      m_refreshBtn(0), m_startBtn(0), m_stopBtn(0), m_restartBtn(0), m_clearLogBtn(0),
      m_logEdit(0), procCount(0), m_selected(-1) {
}

void SupervisorMgrLogic::setWidgets(QLineEdit* host, QLineEdit* port, QLineEdit* user, QLineEdit* pass,
                                    RmProcTable* procTable, QLabel* statusLabel,
                                    QPushButton* refreshBtn, QPushButton* startBtn, QPushButton* stopBtn, QPushButton* restartBtn, QPushButton* clearLogBtn,
                                    QTextEdit* logEdit) {
    m_host = host; m_port = port; m_user = user; m_pass = pass;
    m_procTable = procTable; m_statusLabel = statusLabel;
    m_refreshBtn = refreshBtn; m_startBtn = startBtn; m_stopBtn = stopBtn; m_restartBtn = restartBtn; m_clearLogBtn = clearLogBtn;
    m_logEdit = logEdit;
    loadConfig();
    disableOps();
    clearProcTable();
}

void SupervisorMgrLogic::disableOps() {
    if (m_startBtn) { m_startBtn->setEnabled(false); }
    if (m_stopBtn) { m_stopBtn->setEnabled(false); }
    if (m_restartBtn) { m_restartBtn->setEnabled(false); }
}

void SupervisorMgrLogic::setStatusColor(const QColor& c) {
    if (!m_statusLabel) { return; }
#ifdef QT3_BUILD
    m_statusLabel->setPaletteForegroundColor(c);
#else
    m_statusLabel->setStyleSheet(QString("color:%1").arg(c.name()));
#endif
}

QString SupervisorMgrLogic::configPath() {
    const char* home = getenv("HOME");
    QString base = home ? QString(home) : QString(".");
    base = rmPathJoin(base, ".config");
    mkdir(rmUtf8(base).data(), 0755);
    base = rmPathJoin(base, "supervisor_mgr");
    mkdir(rmUtf8(base).data(), 0755);
    return rmPathJoin(base, "config.json");
}

static QString readUtf8File(const QString& path) {
    QString data;
    FILE* fp = fopen(QFile::encodeName(path).data(), "rb");
    if (!fp) { return data; }
    char b2[4096];
    size_t n;
    while ((n = fread(b2, 1, sizeof(b2), fp)) > 0) { data += QString::fromLocal8Bit(b2, (int)n); }
    fclose(fp);
    return data;
}

static void writeUtf8File(const QString& path, const char* data, int len) {
    FILE* fp = fopen(QFile::encodeName(path).data(), "wb");
    if (!fp || !data || len <= 0) { if (fp) { fclose(fp); } return; }
    fwrite(data, 1, (size_t)len, fp);
    fclose(fp);
}

void SupervisorMgrLogic::loadConfig() {
    QString data = readUtf8File(configPath());
    if (data.isEmpty()) { return; }
    QByteArray bd = rmUtf8(data);
    cJSON* root = cJSON_Parse(bd.data());
    if (!root) { return; }
    cJSON* it;
    if ((it = cJSON_GetObjectItem(root, "host")) && it->valuestring) {
        if (m_host) { m_host->setText(qFromUtf8(it->valuestring)); }
    }
    if ((it = cJSON_GetObjectItem(root, "port")) && it->valuestring) {
        if (m_port) { m_port->setText(qFromUtf8(it->valuestring)); }
    }
    if ((it = cJSON_GetObjectItem(root, "user")) && it->valuestring) {
        if (m_user) { m_user->setText(qFromUtf8(it->valuestring)); }
    }
    if ((it = cJSON_GetObjectItem(root, "pass")) && it->valuestring) {
        if (m_pass) { m_pass->setText(qFromUtf8(it->valuestring)); }
    }
    cJSON_Delete(root);
}

void SupervisorMgrLogic::saveConfig() {
    cJSON* root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "host", rmUtf8(m_host ? m_host->text() : QString("127.0.0.1")).data());
    cJSON_AddStringToObject(root, "port", rmUtf8(m_port ? m_port->text() : QString("9001")).data());
    cJSON_AddStringToObject(root, "user", rmUtf8(m_user ? m_user->text() : QString("")).data());
    cJSON_AddStringToObject(root, "pass", rmUtf8(m_pass ? m_pass->text() : QString("")).data());
    char* out = cJSON_Print(root);
    if (out) {
        writeUtf8File(configPath(), out, (int)strlen(out));
        free(out);
    }
    cJSON_Delete(root);
}

QString SupervisorMgrLogic::rpcUrl() {
    QString h = m_host ? m_host->text() : QString("127.0.0.1");
    QString p = m_port ? m_port->text() : QString("9001");
    h = rmTrim(h); p = rmTrim(p);
    if (h.isEmpty()) { h = "127.0.0.1"; }
    if (p.isEmpty()) { p = "9001"; }
    return "http://" + h + ":" + p + "/RPC2";
}

void SupervisorMgrLogic::appendLog(const QString& msg) {
    if (!m_logEdit) { return; }
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString line = "[" + ts + "] [Supervisor] " + msg + "\n";
    rmSetText(m_logEdit, rmGetText(m_logEdit) + line);
}

void SupervisorMgrLogic::onConnect() {
    saveConfig();
    appendLog(qFromUtf8("连接：") + rpcUrl());
    onRefresh();
}

void SupervisorMgrLogic::onClearLog() {
    if (m_logEdit) { rmSetText(m_logEdit, ""); }
}

static QString xmlEscape(const QString& s) {
    QString t = s;
    t = t.replace("&", "&amp;");
    t = t.replace("<", "&lt;");
    t = t.replace(">", "&gt;");
    t = t.replace("\"", "&quot;");
    return t;
}

void SupervisorMgrLogic::sendRpc(const QString& method, const QStringList& params, const QString& tag) {
    QString body = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<methodCall>\n<methodName>" + xmlEscape(method) + "</methodName>\n<params>\n";
    for (QStringList::ConstIterator it = params.begin(); it != params.end(); ++it) {
        body += "<param><value><string>" + xmlEscape(*it) + "</string></value></param>\n";
    }
    body += "</params>\n</methodCall>\n";
    QProcess* proc = new QProcess(this);
#ifdef QT3_BUILD
    proc->addArgument("curl");
    proc->addArgument("-sSk");
    proc->addArgument("-i");
    proc->addArgument("--connect-timeout");
    proc->addArgument("10");
    proc->addArgument("--max-time");
    proc->addArgument("30");
    proc->addArgument("-H");
    proc->addArgument("Content-Type: text/xml");
    QString u = m_user ? rmTrim(m_user->text()) : QString("");
    QString pw = m_pass ? rmTrim(m_pass->text()) : QString("");
    if (!u.isEmpty() && !pw.isEmpty()) {
        proc->addArgument("--user");
        proc->addArgument(u + ":" + pw);
    } else if (!u.isEmpty() && pw.isEmpty()) {
        proc->addArgument("--user");
        proc->addArgument(u + ":");
    }
    proc->addArgument("-X");
    proc->addArgument("POST");
    proc->addArgument("-d");
    proc->addArgument(body);
    proc->addArgument(rpcUrl());
    proc->start();
#else
    QStringList args;
    args << "curl" << "-sSk" << "-i" << "--connect-timeout" << "10" << "--max-time" << "30"
         << "-H" << "Content-Type: text/xml";
    QString u2 = m_user ? rmTrim(m_user->text()) : QString("");
    QString pw2 = m_pass ? rmTrim(m_pass->text()) : QString("");
    if (!u2.isEmpty() && !pw2.isEmpty()) { args << "--user" << (u2 + ":" + pw2); }
    else if (!u2.isEmpty() && pw2.isEmpty()) { args << "--user" << (u2 + ":"); }
    args << "-X" << "POST" << "-d" << body << rpcUrl();
    proc->start("curl", args);
#endif
#ifdef QT3_BUILD
    connect(proc, SIGNAL(processExited()), this, SLOT(onProcDone()));
#else
    /* Qt 官方规则：slot 可比 signal 签名短（忽略多余参数），无参槽两端通用 */
    connect(proc, SIGNAL(finished(int,QProcess::ExitStatus)), this, SLOT(onProcDone()));
#endif
    if (!tag.isEmpty()) {
        m_rpcTags.insert(proc, tag);
    }
    appendLog(qFromUtf8("RPC[") + tag + qFromUtf8("] ") + method);
}

void SupervisorMgrLogic::onProcDone() {
    QProcess* proc = (QProcess*)sender();
    if (!proc) { return; }
    QString tag = m_rpcTags[proc];
    m_rpcTags.remove(proc);
#ifdef QT3_BUILD
    QString raw = QString::fromLocal8Bit(proc->readStdout());
    raw += QString::fromLocal8Bit(proc->readStderr());
#else
    QString raw = QString::fromLocal8Bit(proc->readAllStandardOutput());
    raw += QString::fromLocal8Bit(proc->readAllStandardError());
#endif
    /* 1) 连接错误检测（curl 错误走 stderr） */
    static const char* conErrs[] = {
        "Connection refused", "Failed to connect", "Couldn't connect",
        "Could not connect", "Connection timed out",
        "Name or service not known", "Could not resolve host",
        "Empty reply from server", "Timeout was reached",
        "Connection reset by peer", "Unknown protocol", 0 };
    bool isConErr = false;
    const char* errHit = 0;
    for (int i = 0; conErrs[i]; ++i) {
        if (sFind(raw, conErrs[i], 0) >= 0) {
            isConErr = true;
            errHit = conErrs[i];
            break;
        }
    }
    if (isConErr) {
        appendLog(qFromUtf8("[连接失败] ") + errHit);
        setStatusColor(QColor(200, 0, 0));
        if (m_statusLabel) { m_statusLabel->setText(qFromUtf8("连接失败：") + errHit); }
        clearProcTable();
        procCount = 0;
        disableOps();
        proc->deleteLater();
        return;
    }
    /* 2) HTTP 状态错误 */
    if (sFind(raw, "HTTP/1.0 401", 0) >= 0 || sFind(raw, "HTTP/1.1 401", 0) >= 0) {
        appendLog(qFromUtf8("[认证失败] HTTP 401"));
        setStatusColor(QColor(200, 0, 0));
        if (m_statusLabel) { m_statusLabel->setText(qFromUtf8("认证失败：用户名/密码错误")); }
        clearProcTable();
        procCount = 0;
        disableOps();
        proc->deleteLater();
        return;
    }
    if (sFind(raw, "HTTP/1.0 404", 0) >= 0 || sFind(raw, "HTTP/1.1 404", 0) >= 0) {
        appendLog(qFromUtf8("[RPC路径错误] HTTP 404"));
        setStatusColor(QColor(200, 0, 0));
        if (m_statusLabel) { m_statusLabel->setText(qFromUtf8("RPC 路径错误：请确认使用 /RPC2")); }
        clearProcTable();
        procCount = 0;
        disableOps();
        proc->deleteLater();
        return;
    }
    /* 3) XML-RPC fault 显式提示 */
    if (sFind(raw, "<fault>", 0) >= 0) {
        QString fs = extractTag(raw, "string", 0);
        appendLog(qFromUtf8("[RPC Fault] ") + fs);
        setStatusColor(QColor(200, 120, 0));
        if (m_statusLabel) { m_statusLabel->setText(qFromUtf8("RPC Fault：") + fs); }
        proc->deleteLater();
        return;
    }
    /* 4) 正常解析 */
    if (sFind(raw, "<methodResponse>", 0) >= 0 || sFind(raw, "<struct>", 0) >= 0) {
        parseProcList(raw);
    } else {
        appendLog(qFromUtf8("[未知响应] ") + (raw.length() > 180 ? raw.left(180) + "..." : raw));
        setStatusColor(QColor(200, 120, 0));
        if (m_statusLabel) { m_statusLabel->setText(qFromUtf8("无有效 RPC 响应")); }
    }
    proc->deleteLater();
}

/* 在整段 XML 中取 <tag>…</tag> 内容 */
static QString extractTag(const QString& xml, const QString& tag, int start) {
    QString open = "<" + tag + ">";
    QString close = "</" + tag + ">";
    int i = sFindQ(xml, open, start);
    if (i < 0) { return QString(""); }
    i += open.length();
    int j = sFindQ(xml, close, i);
    if (j < 0) { return QString(""); }
    return xml.mid(i, j - i);
}

/* 从单个 <member>…</member> 块的 <value>…</value> 内取标量（int/i4 优先，再 string） */
static QString extractMemberValue(const QString& mblk) {
    int vs = sFindQ(mblk, "<value>", 0);
    if (vs < 0) { return QString(""); }
    vs += 7;
    int ve = sFindQ(mblk, "</value>", vs);
    if (ve < 0) { return QString(""); }
    QString inner = mblk.mid(vs, ve - vs);
    QString t = extractTag(inner, "int", 0);
    if (!t.isEmpty()) { return t; }
    t = extractTag(inner, "i4", 0);
    if (!t.isEmpty()) { return t; }
    t = extractTag(inner, "string", 0);
    if (!t.isEmpty()) { return t; }
    return rmTrim(inner);
}

void SupervisorMgrLogic::parseProcList(const QString& xml) {
    if (!m_procTable) { return; }
    clearProcTable();
    procCount = 0;
    int pos = 0;
    while (true) {
        int s = sFindQ(xml, "<struct>", pos);
        if (s < 0) { break; }
        int e = sFindQ(xml, "</struct>", s);
        if (e < 0) { break; }
        QString blk = xml.mid(s, e - s);
        /* 遍历成员，收集 key=value */
        QMap<QString, QString> kv;
        bool hasName = false;
        int mp = 0;
        while (true) {
            int ms = sFindQ(blk, "<member>", mp);
            if (ms < 0) { break; }
            int me = sFindQ(blk, "</member>", ms);
            if (me < 0) { break; }
            QString mblk = blk.mid(ms, me - ms);
            QString mname = extractTag(mblk, "name", 0);
            if (!mname.isEmpty()) {
                if (mname == "name") { hasName = true; }
                if (!kv.contains(mname)) { kv.insert(mname, extractMemberValue(mblk)); }
            }
            mp = me + 9;
        }
        if (!hasName) { pos = e + 9; continue; }
        /* 判定为进程信息：有 name 且含 state/statename/pid 字段 */
        if (!kv.contains("state") && !kv.contains("statename") && !kv.contains("pid")) {
            pos = e + 9;
            continue;
        }
        if (procCount >= 512) { break; }
        SupProcInfo* info = &procs[procCount];
        info->name = kv["name"];
        info->group = kv["group"];
        info->pid = kv.contains("pid") ? atoi(rmCs(kv["pid"])) : 0;
        info->state = kv.contains("state") ? atoi(rmCs(kv["state"])) : -1;
        info->statename = kv.contains("statename") ? kv["statename"] : QString("");
        info->description = kv["description"];
        info->start = kv.contains("start") ? atol(rmCs(kv["start"])) : 0;
        info->stop = kv.contains("stop") ? atol(rmCs(kv["stop"])) : 0;
        info->now = kv.contains("now") ? atol(rmCs(kv["now"])) : 0;
        procCount++;
        addProcRow(info, procCount - 1);
        pos = e + 9;
    }
    int n = procCount;
    if (n > 0) {
        setStatusColor(QColor(0, 128, 0));
        if (m_statusLabel) { m_statusLabel->setText(qFromUtf8("已连接　进程数：") + QString::number(n)); }
    } else {
        setStatusColor(QColor(100, 100, 100));
        if (m_statusLabel) { m_statusLabel->setText(qFromUtf8("已连接（无进程）")); }
    }
    appendLog(qFromUtf8("刷新完成：") + QString::number(n) + qFromUtf8(" 个进程"));
}

void SupervisorMgrLogic::clearProcTable() {
    if (!m_procTable) { return; }
#ifdef QT3_BUILD
    /* QTable 不拥有 item，需手动释放自建 item */
    for (int r = (int)m_procTable->numRows() - 1; r >= 0; --r) {
        for (int c = (int)m_procTable->numCols() - 1; c >= 0; --c) {
            QTableItem* it = m_procTable->item(r, c);
            if (it) {
                delete it;
                m_procTable->setItem(r, c, 0);
            }
        }
    }
    m_procTable->setNumRows(0);
    m_procTable->setNumCols(4);
    m_procTable->horizontalHeader()->setLabel(0, qFromUtf8("进程名"));
    m_procTable->horizontalHeader()->setLabel(1, qFromUtf8("状态"));
    m_procTable->horizontalHeader()->setLabel(2, qFromUtf8("PID"));
    m_procTable->horizontalHeader()->setLabel(3, qFromUtf8("描述"));
#else
    m_procTable->setRowCount(0);
    m_procTable->setColumnCount(4);
    QStringList labels;
    labels << qFromUtf8("进程名") << qFromUtf8("状态") << qFromUtf8("PID") << qFromUtf8("描述");
    m_procTable->setHorizontalHeaderLabels(labels);
#endif
}

void SupervisorMgrLogic::addProcRow(const SupProcInfo* info, int row) {
    if (!m_procTable || !info) { return; }
    QString disp = info->name;
    if (!info->group.isEmpty() && info->group != info->name) { disp = info->group + ":" + info->name; }
    QString st = info->statename.isEmpty() ? stateText(info->state) : info->statename;
    QString pidStr = (info->pid > 0) ? QString::number(info->pid) : QString("-");
    QColor c = stateColor(info->state);
#ifdef QT3_BUILD
    if ((int)m_procTable->numRows() <= row) { m_procTable->setNumRows(row + 1); }
    m_procTable->setItem(row, 0, new ColorTableItem(m_procTable, disp, QColor(60, 60, 60)));
    m_procTable->setItem(row, 1, new ColorTableItem(m_procTable, st, c));
    m_procTable->setItem(row, 2, new ColorTableItem(m_procTable, pidStr, QColor(60, 60, 60)));
    m_procTable->setItem(row, 3, new ColorTableItem(m_procTable, info->description, QColor(100, 100, 255)));
#else
    if (m_procTable->rowCount() <= row) { m_procTable->setRowCount(row + 1); }
    QTableWidgetItem* i0 = new QTableWidgetItem(disp);
    QTableWidgetItem* i1 = new QTableWidgetItem(st);
    QTableWidgetItem* i2 = new QTableWidgetItem(pidStr);
    QTableWidgetItem* i3 = new QTableWidgetItem(info->description);
    i1->setForeground(QBrush(c));
    m_procTable->setItem(row, 0, i0);
    m_procTable->setItem(row, 1, i1);
    m_procTable->setItem(row, 2, i2);
    m_procTable->setItem(row, 3, i3);
#endif
}

QString SupervisorMgrLogic::stateText(int code) {
    if (code == 0) { return "STOPPED"; }
    if (code == 10) { return "STARTING"; }
    if (code == 20) { return "RUNNING"; }
    if (code == 30) { return "BACKOFF"; }
    if (code == 40) { return "STOPPING"; }
    if (code == 100) { return "EXITED"; }
    if (code == 200) { return "FATAL"; }
    if (code == 1000) { return "UNKNOWN"; }
    return "";
}

void SupervisorMgrLogic::onRefresh() {
    QStringList params;
    sendRpc("supervisor.getAllProcessInfo", params, "refresh");
}

void SupervisorMgrLogic::onProcSelected() {
    if (!m_procTable) { return; }
    m_selected = m_procTable->currentRow();
    if (m_selected < 0 || m_selected >= procCount) {
        disableOps();
        return;
    }
    const SupProcInfo* p = &procs[m_selected];
    if (m_startBtn) { m_startBtn->setEnabled(p->state != 20); }
    if (m_stopBtn) { m_stopBtn->setEnabled(p->state == 20 || p->state == 10 || p->state == 40); }
    if (m_restartBtn) { m_restartBtn->setEnabled(true); }
}

void SupervisorMgrLogic::onStart() {
    if (m_selected < 0 || m_selected >= procCount) { return; }
    QStringList params;
    params << procs[m_selected].name << "true";
    sendRpc("supervisor.startProcess", params, "start");
    appendLog(qFromUtf8("启动：") + procs[m_selected].name);
    QTimer::singleShot(1500, this, SLOT(onRefresh()));
}

void SupervisorMgrLogic::onStop() {
    if (m_selected < 0 || m_selected >= procCount) { return; }
    QStringList params;
    params << procs[m_selected].name << "true";
    sendRpc("supervisor.stopProcess", params, "stop");
    appendLog(qFromUtf8("停止：") + procs[m_selected].name);
    QTimer::singleShot(1500, this, SLOT(onRefresh()));
}

void SupervisorMgrLogic::onRestart() {
    if (m_selected < 0 || m_selected >= procCount) { return; }
    /* supervisord 无单进程 restartProcess API；restart() 是全量重启。
       两阶段：stop → 延时 → start */
    QString name = procs[m_selected].name;
    QStringList params;
    params << name << "true";
    sendRpc("supervisor.stopProcess", params, "restart-stop");
    appendLog(qFromUtf8("重启第1阶段：") + name);
    QTimer::singleShot(2000, this, SLOT(onRestartPhase2()));
    QTimer::singleShot(4500, this, SLOT(onRefresh()));
}

void SupervisorMgrLogic::onRestartPhase2() {
    if (m_selected < 0 || m_selected >= procCount) { return; }
    QString name = procs[m_selected].name;
    QStringList params;
    params << name << "true";
    sendRpc("supervisor.startProcess", params, "restart-start");
    appendLog(qFromUtf8("重启第2阶段：") + name);
}
