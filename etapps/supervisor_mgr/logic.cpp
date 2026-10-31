#include "compat34.h"
#include "logic.h"
#include "../../qlcomp/cJSON.h"
#include <qfile.h>
#include <qdir.h>
#include <qtextstream.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>

static QString rmTrim(const QString& s) { return s.stripWhiteSpace(); }
#ifdef QT3_BUILD
static void rmSetText(QTextEdit* e, const QString& s) { e->setText(s); }
#else
static void rmSetText(QTextEdit* e, const QString& s) { e->setPlainText(s); }
#endif
static QByteArray rmUtf8(const QString& s) {
#ifdef QT3_BUILD
    return s.utf8();
#else
    return s.toUtf8();
#endif
}
static QString rmPathJoin(const QString& a, const QString& b) {
    if (a.endsWith("/")) { return a + b; }
    return a + "/" + b;
}

SupervisorMgrLogic::SupervisorMgrLogic(QWidget* parent)
    : QObject(parent), m_parent(parent),
      m_host(0), m_port(0), m_user(0), m_pass(0),
      m_procList(0), m_statusLabel(0),
      m_refreshBtn(0), m_startBtn(0), m_stopBtn(0), m_restartBtn(0), m_clearLogBtn(0),
      m_logEdit(0), m_selected(-1) {
}

void SupervisorMgrLogic::setWidgets(QLineEdit* host, QLineEdit* port, QLineEdit* user, QLineEdit* pass,
                                    QListBox* procList, QLabel* statusLabel,
                                    QPushButton* refreshBtn, QPushButton* startBtn, QPushButton* stopBtn, QPushButton* restartBtn, QPushButton* clearLogBtn,
                                    QTextEdit* logEdit) {
    m_host = host; m_port = port; m_user = user; m_pass = pass;
    m_procList = procList; m_statusLabel = statusLabel;
    m_refreshBtn = refreshBtn; m_startBtn = startBtn; m_stopBtn = stopBtn; m_restartBtn = restartBtn; m_clearLogBtn = clearLogBtn;
    m_logEdit = logEdit;
    loadConfig();
    if (m_startBtn) { m_startBtn->setEnabled(false); }
    if (m_stopBtn) { m_stopBtn->setEnabled(false); }
    if (m_restartBtn) { m_restartBtn->setEnabled(false); }
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

void SupervisorMgrLogic::loadConfig() {
    QFile f(configPath());
    if (!f.open(IO_ReadOnly)) { return; }
    QByteArray ba;
    char buf[4096];
    while (f.readBlock(buf, sizeof(buf)) > 0) { /* read */ }
    /* 简化：用 cJSON 读取更稳妥，这里先按行方式跳过 */
    f.close();
    /* 改用 cJSON */
    QString data;
    FILE* fp = fopen(QFile::encodeName(configPath()).data(), "rb");
    if (!fp) { return; }
    char b2[4096];
    size_t n;
    while ((n = fread(b2, 1, sizeof(b2), fp)) > 0) { data += QString::fromLocal8Bit(b2, (int)n); }
    fclose(fp);
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
        FILE* fp = fopen(QFile::encodeName(configPath()).data(), "wb");
        if (fp) {
            fwrite(out, 1, strlen(out), fp);
            fclose(fp);
        }
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

QString SupervisorMgrLogic::authArgs() {
    QString u = m_user ? m_user->text() : QString("");
    QString pw = m_pass ? m_pass->text() : QString("");
    u = rmTrim(u); pw = rmTrim(pw);
    if (!u.isEmpty() && !pw.isEmpty()) {
        QString up = u + ":" + pw;
        return " -u " + up;
    }
    if (!u.isEmpty() && pw.isEmpty()) {
        return " -u " + u + ":";
    }
    return "";
}

void SupervisorMgrLogic::appendLog(const QString& msg) {
    if (!m_logEdit) { return; }
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString line = "[" + ts + "] [Supervisor] " + msg + "\n";
    rmSetText(m_logEdit, m_logEdit->text() + line);
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
#ifdef QT3_BUILD
    connect(proc, SIGNAL(processExited()), this, SLOT(onProcDone()));
#else
    connect(proc, SIGNAL(processExited(int,QProcess::ExitStatus)), this, SLOT(onProcDone()));
#endif
    if (!tag.isEmpty()) {
        m_rpcTags.insert(proc, tag);
    }
    appendLog(qFromUtf8("RPC[") + tag + qFromUtf8("] ") + method);
    proc->start();
}

void SupervisorMgrLogic::onProcDone() {
    QProcess* proc = (QProcess*)sender();
    if (!proc) { return; }
    QString tag = m_rpcTags[proc];
    m_rpcTags.remove(proc);
    QByteArray out = proc->readStdout();
    QString raw = QString::fromLocal8Bit(out);
    /* 粗略：若含 getAllProcessInfo 结果，解析 */
    if (raw.find("getAllProcessInfo") >= 0 || raw.find("<name>name</name>") >= 0 || raw.find("statename") >= 0) {
        parseProcList(raw);
    } else {
        appendLog(qFromUtf8("响应：") + (raw.length() > 200 ? raw.left(200) + "..." : raw));
        if (m_statusLabel) {
            m_statusLabel->setText(qFromUtf8("完成"));
        }
    }
    proc->deleteLater();
}

static QString extractTag(const QString& xml, const QString& tag, int start) {
    QString open = "<" + tag + ">";
    QString close = "</" + tag + ">";
    int i = xml.find(open, start);
    if (i < 0) { return QString(""); }
    i += open.length();
    int j = xml.find(close, i);
    if (j < 0) { return QString(""); }
    return xml.mid(i, j - i);
}

void SupervisorMgrLogic::parseProcList(const QString& xml) {
    if (!m_procList) { return; }
    m_procList->clear();
    m_procs.clear();
    /* 简单遍历 <struct> 块 */
    int pos = 0;
    while (true) {
        int s = xml.find("<struct>", pos);
        if (s < 0) { break; }
        int e = xml.find("</struct>", s);
        if (e < 0) { break; }
        QString blk = xml.mid(s, e - s);
        /* 判断是否是进程信息（包含 name 和 statename） */
        if (blk.find("<name>statename</name>") < 0 && blk.find("statename") < 0) {
            pos = e + 9;
            continue;
        }
        SupProcInfo info;
        info.pid = 0; info.statename = -1;
        /* 提取成员 */
        int mp = 0;
        while (true) {
            int ms = blk.find("<member>", mp);
            if (ms < 0) { break; }
            int me = blk.find("</member>", ms);
            if (me < 0) { break; }
            QString mblk = blk.mid(ms, me - ms);
            QString mname = extractTag(mblk, "name", 0);
            /* value 可能是 string/int/i4 */
            QString vstr = extractTag(mblk, "string", 0);
            if (vstr.isEmpty()) {
                vstr = extractTag(mblk, "int", 0);
            }
            if (vstr.isEmpty()) {
                vstr = extractTag(mblk, "i4", 0);
            }
            if (mname == "name") { info.name = vstr; }
            else if (mname == "group") { info.group = vstr; }
            else if (mname == "pid") { info.pid = atoi(vstr.latin1()); }
            else if (mname == "state") { info.state = vstr; }
            else if (mname == "statename") { info.statename = atoi(vstr.latin1()); info.state = vstr == "20" ? "RUNNING" : (vstr == "0" ? "STOPPED" : vstr); }
            else if (mname == "description") { info.description = vstr; }
            mp = me + 9;
        }
        if (!info.name.isEmpty()) {
            m_procs.append(new SupProcInfo(info));
            QString disp = info.name;
            if (!info.group.isEmpty() && info.group != info.name) { disp = info.group + ":" + info.name; }
            QString st = stateText(info.statename);
            if (st.isEmpty()) { st = info.state; }
            if (info.pid > 0) {
                disp = disp + "  [" + st + "]  pid=" + QString::number(info.pid);
            } else {
                disp = disp + "  [" + st + "]";
            }
            m_procList->insertItem(disp);
        }
        pos = e + 9;
    }
    if (m_statusLabel) {
        m_statusLabel->setText(qFromUtf8("进程：") + QString::number(m_procs.count()));
    }
    appendLog(qFromUtf8("刷新完成：") + QString::number(m_procs.count()) + qFromUtf8(" 个进程"));
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
    if (!m_procList) { return; }
    m_selected = m_procList->currentItem();
    if (m_selected < 0 || m_selected >= (int)m_procs.count()) {
        if (m_startBtn) { m_startBtn->setEnabled(false); }
        if (m_stopBtn) { m_stopBtn->setEnabled(false); }
        if (m_restartBtn) { m_restartBtn->setEnabled(false); }
        return;
    }
    const SupProcInfo& p = *m_procs.at(m_selected);
    if (m_startBtn) { m_startBtn->setEnabled(p.statename != 20); }
    if (m_stopBtn) { m_stopBtn->setEnabled(p.statename == 20 || p.statename == 10 || p.statename == 40); }
    if (m_restartBtn) { m_restartBtn->setEnabled(true); }
}

void SupervisorMgrLogic::onStart() {
    if (m_selected < 0 || m_selected >= (int)m_procs.count()) { return; }
    QStringList params;
    params << m_procs.at(m_selected)->name << "true";
    sendRpc("supervisor.startProcess", params, "start");
    appendLog(qFromUtf8("启动：") + m_procs.at(m_selected)->name);
    QTimer::singleShot(1500, this, SLOT(onRefresh()));
}

void SupervisorMgrLogic::onStop() {
    if (m_selected < 0 || m_selected >= (int)m_procs.count()) { return; }
    QStringList params;
    params << m_procs.at(m_selected)->name << "true";
    sendRpc("supervisor.stopProcess", params, "stop");
    appendLog(qFromUtf8("停止：") + m_procs.at(m_selected)->name);
    QTimer::singleShot(1500, this, SLOT(onRefresh()));
}

void SupervisorMgrLogic::onRestart() {
    if (m_selected < 0 || m_selected >= (int)m_procs.count()) { return; }
    QString name = m_procs.at(m_selected)->name;
    /* 尝试直接 restartProcess */
    {
        QStringList params;
        params << name << "true";
        sendRpc("supervisor.restartProcess", params, "restart");
    }
    appendLog(qFromUtf8("重启：") + name);
    QTimer::singleShot(2000, this, SLOT(onRefresh()));
}

