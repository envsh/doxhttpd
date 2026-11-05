#include "logic.h"
#include "compat34.h"
#include "compatcore34.h"
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qtextedit.h>
#include <qcombobox.h>
#include <qspinbox.h>
#include <qcheckbox.h>
#include <qfiledialog.h>
#include <qclipboard.h>
#include <qapplication.h>
#include <qdir.h>
#include <qdatetime.h>
#include <qregexp.h>
#include <qprocess.h>
#ifdef QT3_BUILD
#include <qtable.h>
#include <qvaluelist.h>
#else
#include <qtablewidget.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"

/* ═══════════ static 工具（readUtf8File 等自持拷贝，etapps 惯例） ═══════════ */

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

static QByteArray qUtf8Bytes(const QString& s) {
#ifdef QT3_BUILD
    return s.utf8();
#else
    return s.toUtf8();
#endif
}

static QString qStrip(const QString& s) {
#ifdef QT3_BUILD
    return s.stripWhiteSpace();
#else
    return s.trimmed();
#endif
}

/* QTextEdit 取文本：Qt3 text() / Qt4 toPlainText() */
static QString qTextEditGetText(const QTextEdit* e) {
#ifdef QT3_BUILD
    return e->text();
#else
    return e->toPlainText();
#endif
}

static QStringList qSplitLines(const QString& all, bool keepEmpty) {
#ifdef QT3_BUILD
    return QStringList::split('\n', all, keepEmpty);
#else
    if (keepEmpty) { return all.split('\n'); }
    return all.split('\n', QString::SkipEmptyParts);
#endif
}

/* 命令行 token：含空格/引号/反斜杠时用单引号包裹（与 compat34 shellQuote 同口径） */
static QString cmdlineQuote(const QString& s) {
    if (s.isEmpty()) { return s; }
    if (s.contains(' ') || s.contains('\'') || s.contains('"') ||
        s.contains('\\') || s.contains('(') || s.contains(')')) {
        QString t = s;
        t.replace("'", "'\\''");
        return QString("'") + t + QString("'");
    }
    return s;
}

static QString getStr(const cJSON* obj, const char* key, const QString& fallback) {
    if (!obj) { return fallback; }
    cJSON* j = cJSON_GetObjectItem(const_cast<cJSON*>(obj), key);
    if (!j) { return fallback; }
    if (!j->valuestring) { return fallback; }
    return qFromUtf8(j->valuestring);
}

static int getInt(const cJSON* obj, const char* key, int fallback) {
    if (!obj) { return fallback; }
    cJSON* j = cJSON_GetObjectItem(const_cast<cJSON*>(obj), key);
    if (!j) { return fallback; }
    if (j->valuestring) { return atoi(j->valuestring); }
    return (int)j->valueint;
}

static bool getBool(const cJSON* obj, const char* key, bool fallback) {
    if (!obj) { return fallback; }
    cJSON* j = cJSON_GetObjectItem(const_cast<cJSON*>(obj), key);
    if (!j) { return fallback; }
    if (j->valueint != 0) { return true; }
    return fallback;
}

static void setComboByText(QComboBox* cb, const QString& text) {
    if (!cb) { return; }
    for (int i = 0; i < (int)cb->count(); i++) {
#ifdef QT3_BUILD
        if (cb->text(i) == text) { cb->setCurrentItem(i); return; }
#else
        if (cb->itemText(i) == text) { cb->setCurrentIndex(i); return; }
#endif
    }
}

static int comboIndex(QComboBox* cb) {
    if (!cb) { return -1; }
#ifdef QT3_BUILD
    return cb->currentItem();
#else
    return cb->currentIndex();
#endif
}

static QString comboCurrentTextOf(QComboBox* cb) {
    if (comboIndex(cb) < 0) { return QString(); }
    return cb->currentText();
}

static int qPos(const QString& s, const char* needle) {
    QString ns = QString(needle);       /* 仅 ASCII needle，Latin-1 构造安全 */
#ifdef QT3_BUILD
    return s.find(ns);
#else
    return s.indexOf(ns);
#endif
}

static int yTableAppendRow(YtdlpTable* t) {
#ifdef QT3_BUILD
    int r = t->numRows();
    t->setNumRows(r + 1);
    return r;
#else
    int r = t->rowCount();
    t->insertRow(r);
    return r;
#endif
}

static void yTableText(YtdlpTable* t, int row, int col, const QString& s) {
#ifdef QT3_BUILD
    t->setText(row, col, s);
#else
    QTableWidgetItem* it = t->item(row, col);
    if (!it) {
        it = new QTableWidgetItem();
        t->setItem(row, col, it);
    }
    it->setText(s);
#endif
}

static void yTableClearRows(YtdlpTable* t) {
#ifdef QT3_BUILD
    t->setNumRows(0);
#else
    t->setRowCount(0);
#endif
}

static double getDbl(const cJSON* obj, const char* key, double fallback) {    if (!obj) { return fallback; }
    cJSON* j = cJSON_GetObjectItem(const_cast<cJSON*>(obj), key);
    if (!j) { return fallback; }
    if (j->valuestring) { return atof(j->valuestring); }
    if (j->type == cJSON_NULL) { return fallback; }
    return j->valuedouble;
}

static QString joinArgs(const QStringList& args) {
    QString cmd;
    for (int i = 0; i < (int)args.count(); i++) {
        if (i > 0) { cmd += " "; }
        cmd += args[i];
    }
    return cmd;
}

/* ═══════════ 构造/注入 ═══════════ */

YtdlpUi::YtdlpUi()
    : urlsEdit(0), presetH(0), heightCap(0), fmtCustom(0),
      extractAudio(0), audioFmt(0), audioQual(0),
      dirEdit(0), dirPick(0), useArchive(0), archiveEdit(0), archivePick(0),
      itemsSpec(0), btnStart(0), btnStop(0),
      cookieEdit(0), cookiePick(0), userEdit(0), passEdit(0),
      proxyEdit(0), rateEdit(0), retriesSpin(0), concSpin(0),
      embedSubs(0), embedThumb(0), embedMeta(0), embedChapters(0),
      subLangsEdit(0), convertSubs(0), mergeFmt(0),
      sbRemove(0), sbCatsEdit(0), ppaEdit(0),
      probeUrl(0), btnProbe(0), btnFill(0), probeStatus(0), formatsTable(0),
      cmdPreview(0), taskTable(0), histLog(0), versionLabel(0)
{
}

YtdlpConfig::YtdlpConfig()
    : formatPreset(0), height(1080),
      extractAudio(false), audioQual("best"),
      useArchive(false),
      retries(10), conc(4),
      embedSubs(false), embedThumb(false), embedMeta(false), embedChapters(false),
      sbRemove(false)
{
}

YtdlpLogic::YtdlpLogic(QObject* parent)
    : QObject(parent)
{
    m_ui = 0;
    m_procParent = 0;
    m_ytdlpOk = false;
    m_loading = false;
    m_curProc = 0;
    m_curIdx = -1;
    m_cancelling = false;
    m_killPending = false;
    m_saveTimer = new QTimer(this);
    connect(m_saveTimer, SIGNAL(timeout()), this, SLOT(onSaveTick()));
    m_killTimer = new QTimer(this);
    connect(m_killTimer, SIGNAL(timeout()), this, SLOT(onKillTick()));
    m_probeProc = 0;
}

void YtdlpLogic::setWidgets(YtdlpUi* ui) {
    m_ui = ui;
    /* mainwin 以 parent 传入顶层窗口 widget（w），成为弹窗 parent */
    QObject* p = parent();
    if (p && p->isWidgetType()) {
        m_procParent = p;
    } else {
        m_procParent = 0;
    }
    m_loading = true;
    loadConfig();
    connectAll();
    m_loading = false;
    updateCmdPreview();
    probeYtdlp();
}

/* ═══════════ 配置 ═══════════ */

QString YtdlpLogic::baseDir() const {
#ifdef QT3_BUILD
    QString home = QDir::homeDirPath();
#else
    QString home = QDir::homePath();
#endif
    return home + "/.config/ytdlp";
}

void YtdlpLogic::ensureDirs() const {
    qMkdir(baseDir(), true);
}

void YtdlpLogic::loadConfig() {
    YtdlpConfig def;
    QString json = readUtf8File(baseDir() + "/config.json");
    if (json.isEmpty()) {
        applyConfig(def);
        m_ui->archiveEdit->setText(baseDir() + "/archive.txt");
        return;
    }
    cJSON* root = cJSON_Parse(qUtf8Bytes(json).data());
    if (!root) {
        applyConfig(def);
        m_ui->archiveEdit->setText(baseDir() + "/archive.txt");
        return;
    }
    YtdlpConfig c;
    c.formatPreset = getInt(root, "format_preset", def.formatPreset);
    c.height = getInt(root, "height", def.height);
    c.fmtCustom = getStr(root, "fmt_custom", def.fmtCustom);
    c.extractAudio = getBool(root, "extract_audio", def.extractAudio);
    c.audioFmt = getStr(root, "audio_fmt", "best");
    c.audioQual = getStr(root, "audio_qual", def.audioQual);
    c.dir = getStr(root, "dir", def.dir);
    c.useArchive = getBool(root, "use_archive", def.useArchive);
    c.archive = getStr(root, "archive", baseDir() + "/archive.txt");
    c.items = getStr(root, "items", def.items);
    c.cookie = getStr(root, "cookie", def.cookie);
    c.username = getStr(root, "username", def.username);
    c.password = getStr(root, "password", def.password);
    c.proxy = getStr(root, "proxy", def.proxy);
    c.rate = getStr(root, "rate", def.rate);
    c.retries = getInt(root, "retries", def.retries);
    c.conc = getInt(root, "conc", def.conc);
    c.embedSubs = getBool(root, "embed_subs", def.embedSubs);
    c.embedThumb = getBool(root, "embed_thumb", def.embedThumb);
    c.embedMeta = getBool(root, "embed_meta", def.embedMeta);
    c.embedChapters = getBool(root, "embed_chapters", def.embedChapters);
    c.subLangs = getStr(root, "sub_langs", def.subLangs);
    c.convertSubs = getStr(root, "convert_subs", def.convertSubs);
    c.mergeFmt = getStr(root, "merge_fmt", def.mergeFmt);
    c.sbRemove = getBool(root, "sb_remove", def.sbRemove);
    c.sbCats = getStr(root, "sb_cats", def.sbCats);
    c.ppa = getStr(root, "ppa", def.ppa);
    cJSON_Delete(root);
    applyConfig(c);
}

void YtdlpLogic::applyConfig(const YtdlpConfig& c) {
    if (!m_ui) { return; }
    if (c.formatPreset >= 0 && c.formatPreset < (int)m_ui->presetH->count()) {
#ifdef QT3_BUILD
        m_ui->presetH->setCurrentItem(c.formatPreset);
#else
        m_ui->presetH->setCurrentIndex(c.formatPreset);
#endif
    }
    m_ui->heightCap->setValue(c.height);
    m_ui->fmtCustom->setText(c.fmtCustom);
    m_ui->extractAudio->setChecked(c.extractAudio);
    setComboByText(m_ui->audioFmt, c.audioFmt);
    setComboByText(m_ui->audioQual, c.audioQual);
    m_ui->dirEdit->setText(c.dir);
    m_ui->useArchive->setChecked(c.useArchive);
    m_ui->archiveEdit->setText(c.archive);
    m_ui->itemsSpec->setText(c.items);
    m_ui->cookieEdit->setText(c.cookie);
    m_ui->userEdit->setText(c.username);
    m_ui->passEdit->setText(c.password);
    m_ui->proxyEdit->setText(c.proxy);
    m_ui->rateEdit->setText(c.rate);
    m_ui->retriesSpin->setValue(c.retries);
    m_ui->concSpin->setValue(c.conc);
    m_ui->embedSubs->setChecked(c.embedSubs);
    m_ui->embedThumb->setChecked(c.embedThumb);
    m_ui->embedMeta->setChecked(c.embedMeta);
    m_ui->embedChapters->setChecked(c.embedChapters);
    m_ui->subLangsEdit->setText(c.subLangs);
    setComboByText(m_ui->convertSubs,
        c.convertSubs.isEmpty() ? qFromUtf8("不转换") : c.convertSubs);
    setComboByText(m_ui->mergeFmt,
        c.mergeFmt.isEmpty() ? qFromUtf8("不指定") : c.mergeFmt);
    m_ui->sbRemove->setChecked(c.sbRemove);
    m_ui->sbCatsEdit->setText(c.sbCats.isEmpty() ?
        qFromUtf8("sponsor,intro,outro,selfpromo,interaction") : c.sbCats);
    m_ui->ppaEdit->setText(c.ppa);
}

void YtdlpLogic::collectConfig(YtdlpConfig* c) const {
    if (!m_ui) { return; }
    c->formatPreset = comboIndex(m_ui->presetH);
    c->height = m_ui->heightCap->value();
    c->fmtCustom = m_ui->fmtCustom->text();
    c->extractAudio = m_ui->extractAudio->isChecked();
    c->audioFmt = comboCurrentTextOf(m_ui->audioFmt);
    c->audioQual = comboCurrentTextOf(m_ui->audioQual);
    c->dir = m_ui->dirEdit->text();
    c->useArchive = m_ui->useArchive->isChecked();
    c->archive = m_ui->archiveEdit->text();
    c->items = m_ui->itemsSpec->text();
    c->cookie = m_ui->cookieEdit->text();
    c->username = m_ui->userEdit->text();
    c->password = m_ui->passEdit->text();
    c->proxy = m_ui->proxyEdit->text();
    c->rate = m_ui->rateEdit->text();
    c->retries = m_ui->retriesSpin->value();
    c->conc = m_ui->concSpin->value();
    c->embedSubs = m_ui->embedSubs->isChecked();
    c->embedThumb = m_ui->embedThumb->isChecked();
    c->embedMeta = m_ui->embedMeta->isChecked();
    c->embedChapters = m_ui->embedChapters->isChecked();
    c->subLangs = m_ui->subLangsEdit->text();
    c->convertSubs = comboCurrentTextOf(m_ui->convertSubs);
    c->mergeFmt = comboCurrentTextOf(m_ui->mergeFmt);
    c->sbRemove = m_ui->sbRemove->isChecked();
    c->sbCats = m_ui->sbCatsEdit->text();
    c->ppa = m_ui->ppaEdit->text();
}

static void addStr(cJSON* root, const char* key, const QString& s) {
    QByteArray u = qUtf8Bytes(s);
    cJSON_AddStringToObject(root, key, u.data());
}

void YtdlpLogic::saveConfig() {
    if (!m_ui) { return; }
    ensureDirs();
    YtdlpConfig c;
    collectConfig(&c);
    cJSON* root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "format_preset", c.formatPreset);
    cJSON_AddNumberToObject(root, "height", c.height);
    addStr(root, "fmt_custom", c.fmtCustom);
    cJSON_AddBoolToObject(root, "extract_audio", c.extractAudio ? 1 : 0);
    addStr(root, "audio_fmt", c.audioFmt);
    addStr(root, "audio_qual", c.audioQual);
    addStr(root, "dir", c.dir);
    cJSON_AddBoolToObject(root, "use_archive", c.useArchive ? 1 : 0);
    addStr(root, "archive", c.archive);
    addStr(root, "items", c.items);
    addStr(root, "cookie", c.cookie);
    addStr(root, "username", c.username);
    addStr(root, "password", c.password);
    addStr(root, "proxy", c.proxy);
    addStr(root, "rate", c.rate);
    cJSON_AddNumberToObject(root, "retries", c.retries);
    cJSON_AddNumberToObject(root, "conc", c.conc);
    cJSON_AddBoolToObject(root, "embed_subs", c.embedSubs ? 1 : 0);
    cJSON_AddBoolToObject(root, "embed_thumb", c.embedThumb ? 1 : 0);
    cJSON_AddBoolToObject(root, "embed_meta", c.embedMeta ? 1 : 0);
    cJSON_AddBoolToObject(root, "embed_chapters", c.embedChapters ? 1 : 0);
    addStr(root, "sub_langs", c.subLangs);
    addStr(root, "convert_subs", c.convertSubs);
    addStr(root, "merge_fmt", c.mergeFmt);
    cJSON_AddBoolToObject(root, "sb_remove", c.sbRemove ? 1 : 0);
    addStr(root, "sb_cats", c.sbCats);
    addStr(root, "ppa", c.ppa);
    char* out = cJSON_Print(root);
    if (out) {
        writeUtf8File(baseDir() + "/config.json", qFromUtf8(out));
        free(out);
    }
    cJSON_Delete(root);
}

/* ═══════════ 命令预览 ═══════════ */

void YtdlpLogic::updateCmdPreview() {
    if (!m_ui || m_loading) { return; }
    m_ui->cmdPreview->setText(buildCmdText());
}

QStringList YtdlpLogic::buildArgs(const QString& singleUrl) const {
    if (!m_ui) { return QStringList(); }
    QStringList args;
    args << "yt-dlp";
    args << "--newline";

    /* 格式 */
    int preset = comboIndex(m_ui->presetH);
    if (preset == 1) {
        args << "-f" << "best";
    } else if (preset == 2) {
        int h = m_ui->heightCap->value();
        args << "-f";
        args << QString("bv*[height<=%1]+ba/b").arg(h);
    } else if (preset == 3) {
        QString f = qStrip(m_ui->fmtCustom->text());
        if (!f.isEmpty()) { args << "-f" << f; }
    }
    /* preset 0 = 自动：yt-dlp 默认 bv*+ba 合流，不加 -f */

    /* 音轨 */
    if (m_ui->extractAudio->isChecked()) {
        args << "-x";
        QString af = comboCurrentTextOf(m_ui->audioFmt);
        if (!af.isEmpty() && af != "best") { args << "--audio-format" << af; }
        QString aq = comboCurrentTextOf(m_ui->audioQual);
        if (!aq.isEmpty()) {
            if (aq == "best") {
                args << "--audio-quality" << "0";
            } else {
                args << "--audio-quality" << aq;
            }
        }
    }

    /* 目标/archive/列表项 */
    {
        QString dir = qStrip(m_ui->dirEdit->text());
        if (!dir.isEmpty()) { args << "-P" << dir; }
        if (m_ui->useArchive->isChecked()) {
            QString a = qStrip(m_ui->archiveEdit->text());
            if (!a.isEmpty()) { args << "--download-archive" << a; }
        }
        QString it = qStrip(m_ui->itemsSpec->text());
        if (!it.isEmpty()) { args << "-I" << it; }
    }

    /* 网络与登录 */
    {
        QString ck = qStrip(m_ui->cookieEdit->text());
        if (!ck.isEmpty()) { args << "--cookies" << ck; }
        QString us = qStrip(m_ui->userEdit->text());
        if (!us.isEmpty()) { args << "-u" << us; }
        QString pw = m_ui->passEdit->text();
        if (!pw.isEmpty()) { args << "-p" << cmdlineQuote(pw); }
        QString px = qStrip(m_ui->proxyEdit->text());
        if (!px.isEmpty()) { args << "--proxy" << px; }
        QString rt = qStrip(m_ui->rateEdit->text());
        if (!rt.isEmpty()) { args << "--limit-rate" << rt; }
        args << "--retries" << QString::number(m_ui->retriesSpin->value());
        args << "-N" << QString::number(m_ui->concSpin->value());
    }

    /* 后处理 */
    if (m_ui->embedSubs->isChecked()) { args << "--embed-subs"; }
    if (m_ui->embedThumb->isChecked()) { args << "--embed-thumbnail"; }
    if (m_ui->embedMeta->isChecked()) { args << "--embed-metadata"; }
    if (m_ui->embedChapters->isChecked()) { args << "--embed-chapters"; }
    {
        QString sl = qStrip(m_ui->subLangsEdit->text());
        if (!sl.isEmpty()) {
            args << "--write-subs" << "--sub-langs" << sl;
        }
        QString cs = comboCurrentTextOf(m_ui->convertSubs);
        if (!cs.isEmpty() && cs != qFromUtf8("不转换")) {
            args << "--convert-subs" << cs;
        }
        QString mf = comboCurrentTextOf(m_ui->mergeFmt);
        if (!mf.isEmpty() && mf != qFromUtf8("不指定")) {
            args << "--merge-output-format" << mf;
        }
        if (m_ui->sbRemove->isChecked()) {
            QString cats = qStrip(m_ui->sbCatsEdit->text());
            if (cats.isEmpty()) { cats = "sponsor,intro,outro,selfpromo,interaction"; }
            args << "--sponsorblock-remove" << cats;
        }
        QString ppa = qStrip(m_ui->ppaEdit->text());
        if (!ppa.isEmpty()) { args << "--ppa" << cmdlineQuote(ppa); }
    }

    /* URL：一行一个；singleUrl 非空时只带该地址（任务队列按 URL 串行） */
    {
        QStringList lines;
        if (singleUrl.isEmpty()) {
            lines = qSplitLines(qTextEditGetText(m_ui->urlsEdit), true);
        } else {
            lines << singleUrl;
        }
        int urls = 0;
        for (int i = 0; i < (int)lines.count(); i++) {
            QString line = qStrip(lines[i]);
            if (line.isEmpty()) { continue; }
            args << cmdlineQuote(line);
            urls++;
        }
        if (urls == 0) { args << qFromUtf8("(未填 URL)"); }
    }
    return args;
}

QString YtdlpLogic::buildCmdText() const {
    return joinArgs(buildArgs());
}

/* ═══════════ 任务队列 ═══════════ */

static bool parseProgressLine(const QString& line, int* pct,
                              QString* speed, QString* eta) {
    QRegExp rx("\\[download\\]\\s*([0-9.]+)% of\\s*~?\\s*\\S+ at\\s*(\\S+) ETA\\s*(\\S+)");
#ifdef QT3_BUILD
    if (rx.search(line) < 0) { return false; }
#else
    if (rx.indexIn(line) < 0) { return false; }
#endif
    double d = rx.cap(1).toDouble();
    if (d < 0) { d = 0; }
    if (d > 100) { d = 100; }
    *pct = (int)d;
    *speed = rx.cap(2);
    *eta = rx.cap(3);
    return true;
}

void YtdlpLogic::consumeStream(const QByteArray& chunk, std::string* buf) {
    buf->append(chunk.data(), (size_t)chunk.size());
    while (true) {
        std::string::size_type idx = buf->find('\n');
        if (idx == std::string::npos) { break; }
        std::string lineBytes = buf->substr(0, idx);
        *buf = buf->substr(idx + 1);
        QString line = qFromUtf8(lineBytes.data(), (int)lineBytes.size());
        parseProcLine(qStrip(line));
    }
}

void YtdlpLogic::onProcReadOut() {
    if (!m_curProc) { return; }
#ifdef QT3_BUILD
    QByteArray chunk = m_curProc->readStdout();
#else
    QByteArray chunk = m_curProc->readAllStandardOutput();
#endif
    consumeStream(chunk, &m_outBuf);
}

void YtdlpLogic::onProcReadErr() {
    if (!m_curProc) { return; }
#ifdef QT3_BUILD
    QByteArray chunk = m_curProc->readStderr();
#else
    QByteArray chunk = m_curProc->readAllStandardError();
#endif
    consumeStream(chunk, &m_errBuf);
}

void YtdlpLogic::parseProcLine(const QString& line) {
    if (line.isEmpty()) { return; }
    if (m_curIdx < 0 || m_curIdx >= (int)m_queue.count()) { return; }
    YtdlpTask& t = m_queue[m_curIdx];

    int pct = 0;
    QString speed;
    QString eta;
    if (parseProgressLine(line, &pct, &speed, &eta)) {
        t.pct = pct;
        t.speed = speed;
        t.eta = eta;
        int row = t.row;
        yTableText(m_ui->taskTable, row, 2, QString::number(pct) + "%");
        yTableText(m_ui->taskTable, row, 3, speed);
        yTableText(m_ui->taskTable, row, 4, eta);
        /* ≥10% 跳变才打日志，避免刷屏 */
        if (pct / 10 > t.pctLogged / 10) {
            appendLog(qFromUtf8("进度 ") + QString::number(pct)
                + "% " + speed + " ETA " + eta);
        }
        t.pctLogged = pct;
        return;
    }

    int di = qPos(line, "Destination: ");
    if (line.startsWith("[download] ") && di >= 0) {
        t.outPath = qStrip(line.mid(di + (int)strlen("Destination: ")));
        yTableText(m_ui->taskTable, t.row, 5, t.outPath);
        appendLogWithPrefix(qFromUtf8("dest"), t.outPath);
        return;
    }

    int ai = qPos(line, "has already been downloaded");
    if (ai >= 0) {
        t.outPath = qStrip(line.left(ai)).remove(0, (int)strlen("[download]"));
        t.outPath = qStrip(t.outPath);
        yTableText(m_ui->taskTable, t.row, 5, t.outPath);
        appendLog(qFromUtf8("已存在，跳过下载"));
        return;
    }

    if (line.startsWith("ERROR:") || line.contains("ERROR:")) {
        appendLogWithPrefix(qFromUtf8("err"), line);
        return;
    }

    /* 其他信息行（[Merger]/[ExtractAudio]/[Metadata] 等） */
    if (line.startsWith("[")) {
        appendLog(line);
    }
}

void YtdlpLogic::onStartClicked() {
    if (!m_ui) { return; }
    if (!m_ytdlpOk) {
        appendLog(qFromUtf8("yt-dlp 未就位，无法开始"));
        return;
    }
    QStringList lines = qSplitLines(qTextEditGetText(m_ui->urlsEdit), true);
    int added = 0;
    for (int i = 0; i < (int)lines.count(); i++) {
        QString url = qStrip(lines[i]);
        if (url.isEmpty()) { continue; }
        YtdlpTask t;
        t.url = url;
        t.row = yTableAppendRow(m_ui->taskTable);
        QString disp = url;
        if (disp.length() > 60) { disp = disp.left(57) + "..."; }
        yTableText(m_ui->taskTable, t.row, 0, disp);
        yTableText(m_ui->taskTable, t.row, 1, qFromUtf8("排队"));
        yTableText(m_ui->taskTable, t.row, 2, "0%");
        m_queue.append(t);
        added++;
    }
    if (added == 0) {
        appendLog(qFromUtf8("URL 列表为空"));
        return;
    }
    appendLog(QString::number(added) + qFromUtf8(" 个任务入队"));
    startNext();
}

void YtdlpLogic::startNext() {
    if (m_curProc) { return; }
    int idx = -1;
    for (int i = 0; i < (int)m_queue.count(); i++) {
        if (m_queue[i].state == 0) { idx = i; break; }
    }
    if (idx < 0) {
        m_curProc = 0;
        if (m_ui) { m_ui->btnStop->setEnabled(false); }
        appendLog(qFromUtf8("队列已空"));
        return;
    }
    m_curIdx = idx;
    m_queue[idx].state = 1;
    m_cancelling = false;
    yTableText(m_ui->taskTable, m_queue[idx].row, 1, qFromUtf8("下载中"));
    spawnTask();
}

void YtdlpLogic::spawnTask() {
    if (!m_ui || m_curIdx < 0) { return; }
    if (m_curProc) { delete m_curProc; m_curProc = 0; }
    m_outBuf.clear();
    m_errBuf.clear();
    m_killPending = false;

    QString url = m_queue[m_curIdx].url;
    QStringList args = buildArgs(url);
    if (args.isEmpty()) { return; }

    QProcess* p = new QProcess(this);
    bool started = false;
#ifdef QT3_BUILD
    connect(p, SIGNAL(readyReadStdout()), this, SLOT(onProcReadOut()));
    connect(p, SIGNAL(readyReadStderr()), this, SLOT(onProcReadErr()));
    connect(p, SIGNAL(processExited()), this, SLOT(onProcDone()));
    p->setArguments(args);
    started = p->start();
#else
    connect(p, SIGNAL(readyReadStandardOutput()), this, SLOT(onProcReadOut()));
    connect(p, SIGNAL(readyReadStandardError()), this, SLOT(onProcReadErr()));
    connect(p, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(onProcDone()));
    QString prog = args[0];
    QStringList rest = args.mid(1);
    p->start(prog, rest);
    started = p->waitForStarted(3000);
#endif
    if (!started) {
        m_queue[m_curIdx].state = 3;
        yTableText(m_ui->taskTable, m_queue[m_curIdx].row, 1,
            qFromUtf8("失败(启动)"));
        appendLogWithPrefix(qFromUtf8("err"), qFromUtf8("进程启动失败"));
        p->deleteLater();
        m_curProc = 0;
        m_curIdx = -1;
        startNext();
        return;
    }
    m_curProc = p;
    m_ui->btnStop->setEnabled(true);
    appendLogWithPrefix(qFromUtf8("start"), joinArgs(args));
}

void YtdlpLogic::onStopClicked() {
    if (!m_curProc) {
        appendLog(qFromUtf8("当前无运行中的任务"));
        return;
    }
    m_cancelling = true;
    m_killPending = true;
#ifdef QT3_BUILD
    m_curProc->tryTerminate();
#else
    m_curProc->terminate();
#endif
    if (m_curIdx >= 0 && m_curIdx < (int)m_queue.count()) {
        yTableText(m_ui->taskTable, m_queue[m_curIdx].row, 1,
            qFromUtf8("取消中…"));
    }
    qTimerStart(m_killTimer, 3000, false);
}

void YtdlpLogic::onKillTick() {
    if (!m_killPending) { return; }
    m_killPending = false;
    if (m_curProc) {
        m_curProc->kill();
        appendLog(qFromUtf8("已强制结束进程"));
    }
}

void YtdlpLogic::onProcDone() {
    /* 先冲掉残留半行 */
    if (m_curProc) {
        if (!m_outBuf.empty()) {
            parseProcLine(qFromUtf8(m_outBuf.data(), (int)m_outBuf.size()));
            m_outBuf.clear();
        }
        if (!m_errBuf.empty()) {
            parseProcLine(qFromUtf8(m_errBuf.data(), (int)m_errBuf.size()));
            m_errBuf.clear();
        }
    }
    QProcess* p = m_curProc;
    m_curProc = 0;
    int code = 0;
    if (p) {
#ifdef QT3_BUILD
        code = p->normalExit() ? p->exitStatus() : -1;
#else
        code = (p->exitStatus() == QProcess::NormalExit) ?
            p->exitCode() : -1;
#endif
    }
    if (p) { p->deleteLater(); }

    if (m_curIdx >= 0 && m_curIdx < (int)m_queue.count()) {
        YtdlpTask& t = m_queue[m_curIdx];
        if (m_cancelling) {
            t.state = 4;
            yTableText(m_ui->taskTable, t.row, 1, qFromUtf8("已取消"));
            appendLog(qFromUtf8("任务已取消"));
        } else if (code == 0) {
            t.state = 2;
            yTableText(m_ui->taskTable, t.row, 1, qFromUtf8("完成"));
            yTableText(m_ui->taskTable, t.row, 2, "100%");
            appendLog(qFromUtf8("任务完成 ") + t.url);
        } else {
            t.state = 3;
            yTableText(m_ui->taskTable, t.row, 1,
                qFromUtf8("失败(") + QString::number(code) + ")");
            appendLogWithPrefix(qFromUtf8("err"),
                qFromUtf8("退出码 ") + QString::number(code) + " " + t.url);
        }
        m_ui->btnStop->setEnabled(false);
    }
    m_curIdx = -1;
    startNext();
}

/* ═══════════ 交互 ═══════════ */

void YtdlpLogic::onSaveTick() {
    saveConfig();
}

void YtdlpLogic::onFormChanged() {
    updateCmdPreview();
    if (!m_loading) {
        qTimerStart(m_saveTimer, 1500, false);
    }
}

void YtdlpLogic::onPickDir() {
    if (!m_ui) { return; }
    QString cur = qStrip(m_ui->dirEdit->text());
    if (cur.isEmpty()) { cur = qGetHomePath(); }
    QString dir;
#ifdef QT3_BUILD
    dir = QFileDialog::getExistingDirectory(cur, (QWidget*)m_procParent);
#else
    dir = QFileDialog::getExistingDirectory((QWidget*)m_procParent,
        qFromUtf8("选择保存目录"), cur);
#endif
    if (dir.isEmpty()) { return; }
    m_ui->dirEdit->setText(dir);
    onFormChanged();
}

void YtdlpLogic::onPickArchive() {
    if (!m_ui) { return; }
    QString cur = qStrip(m_ui->archiveEdit->text());
    QString seed = cur.isEmpty() ? baseDir() : cur;
    QString f;
#ifdef QT3_BUILD
    f = QFileDialog::getOpenFileName(seed, QString::null, (QWidget*)m_procParent);
#else
    f = QFileDialog::getOpenFileName((QWidget*)m_procParent,
        qFromUtf8("选择去重档案文件"), seed, QString::null);
#endif
    if (f.isEmpty()) { return; }
    m_ui->archiveEdit->setText(f);
    m_ui->useArchive->setChecked(true);
    onFormChanged();
}

void YtdlpLogic::onPickCookie() {
    if (!m_ui) { return; }
    QString cur = qStrip(m_ui->cookieEdit->text());
    QString seed = cur.isEmpty() ? qGetHomePath() : cur;
    QString f;
#ifdef QT3_BUILD
    f = QFileDialog::getOpenFileName(seed, QString::null, (QWidget*)m_procParent);
#else
    f = QFileDialog::getOpenFileName((QWidget*)m_procParent,
        qFromUtf8("选择 cookies.txt"), seed, QString::null);
#endif
    if (f.isEmpty()) { return; }
    m_ui->cookieEdit->setText(f);
    onFormChanged();
}

void YtdlpLogic::onCopyCommand() {
    if (!m_ui) { return; }
    QApplication::clipboard()->setText(m_ui->cmdPreview->text());
    appendLogWithPrefix(qFromUtf8("cmd"), m_ui->cmdPreview->text());
}

void YtdlpLogic::onProbeFormats() {
    if (!m_ui || !m_ytdlpOk) { return; }
    if (m_probeProc) {
        m_ui->probeStatus->setText(qFromUtf8("探测仍在进行中…"));
        return;
    }
    QString url = qStrip(m_ui->probeUrl->text());
    if (url.isEmpty()) {
        m_ui->probeStatus->setText(qFromUtf8("请先输入 URL"));
        return;
    }
    m_probeBuf.clear();
    QStringList args;
    args << "yt-dlp" << "-J" << "--no-warnings";
    args << cmdlineQuote(url);

    QProcess* p = new QProcess(this);
    bool started = false;
#ifdef QT3_BUILD
    connect(p, SIGNAL(readyReadStdout()), this, SLOT(onProbeReadOut()));
    connect(p, SIGNAL(readyReadStderr()), this, SLOT(onProbeReadErr()));
    connect(p, SIGNAL(processExited()), this, SLOT(onProbeDone()));
    p->setArguments(args);
    started = p->start();
#else
    connect(p, SIGNAL(readyReadStandardOutput()), this, SLOT(onProbeReadOut()));
    connect(p, SIGNAL(readyReadStandardError()), this, SLOT(onProbeReadErr()));
    connect(p, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(onProbeDone()));
    p->start(args[0], args.mid(1));
    started = p->waitForStarted(3000);
#endif
    if (!started) {
        p->deleteLater();
        m_ui->probeStatus->setText(qFromUtf8("探测进程启动失败"));
        return;
    }
    m_probeProc = p;
    m_ui->probeStatus->setText(qFromUtf8("探测中…（大播放列表可能很慢）"));
    appendLogWithPrefix(qFromUtf8("probe"), url);
}

void YtdlpLogic::onProbeReadOut() {
    if (!m_probeProc) { return; }
#ifdef QT3_BUILD
    QByteArray chunk = m_probeProc->readStdout();
#else
    QByteArray chunk = m_probeProc->readAllStandardOutput();
#endif
    m_probeBuf.append(chunk.data(), (size_t)chunk.size());
}

void YtdlpLogic::onProbeReadErr() {
    if (!m_probeProc) { return; }
#ifdef QT3_BUILD
    m_probeProc->readStderr(); /* 丢弃 stderr，仅回显残错于状态 */
#else
    m_probeProc->readAllStandardError();
#endif
}

void YtdlpLogic::onProbeDone() {
    int code = 0;
    QProcess* p = m_probeProc;
    m_probeProc = 0;
    if (p) {
#ifdef QT3_BUILD
        code = p->normalExit() ? p->exitStatus() : -1;
#else
        code = (p->exitStatus() == QProcess::NormalExit) ? p->exitCode() : -1;
#endif
        p->deleteLater();
    }
    if (!m_ui) { return; }
    QString json = m_probeBuf.size() > 0 ?
        qFromUtf8(m_probeBuf.data(), (int)m_probeBuf.size()) : QString();
    m_probeBuf.clear();
    if (code != 0) {
        m_ui->probeStatus->setText(
            qFromUtf8("yt-dlp 探测失败（退出码 ") + QString::number(code) + "）");
        return;
    }
    cJSON* root = cJSON_Parse(qUtf8Bytes(json).data());
    if (!root) {
        m_ui->probeStatus->setText(
            qFromUtf8("JSON 解析失败（播放列表建议填到主下载页，或改填单视频）"));
        return;
    }
    yTableClearRows(m_ui->formatsTable);

    QString title = getStr(root, "title", QString());
    QString uploader = getStr(root, "uploader", QString());
    cJSON* formats = cJSON_GetObjectItem(root, "formats");
    int nfmt = 0;
    if (formats && formats->type == cJSON_Array) {
        nfmt = cJSON_GetArraySize(formats);
        for (int i = 0; i < nfmt; i++) {
            cJSON* fr = cJSON_GetArrayItem(formats, i);
            int row = yTableAppendRow(m_ui->formatsTable);
            QString id = getStr(fr, "format_id", QString());
            QString ext = getStr(fr, "ext", QString());
            QString res = getStr(fr, "resolution", QString());
            double fps = getDbl(fr, "fps", 0);
            QString fpsStr = fps > 0 ? QString::number(fps, 'f', 0) : QString();
            double tbr = getDbl(fr, "tbr", 0);
            QString tbrStr = tbr > 0 ?
                QString::number(tbr, 'f', 0) + "k" : QString();
            QString vc = getStr(fr, "vcodec", QString());
            QString ac = getStr(fr, "acodec", QString());
            QString codecs;
            if (!vc.isEmpty() && vc != "none") { codecs += vc; }
            if (!ac.isEmpty() && ac != "none") {
                if (!codecs.isEmpty()) { codecs += "/"; }
                codecs += ac;
            }
            double fs = getDbl(fr, "filesize", 0);
            if (fs <= 0) { fs = getDbl(fr, "filesize_approx", 0); }
            QString sizeStr;
            if (fs > 0) {
                if (fs > 1048576.0) {
                    sizeStr = QString::number(fs / 1048576.0, 'f', 1) + "MiB";
                } else {
                    sizeStr = QString::number(fs / 1024.0, 'f', 1) + "KiB";
                }
            }
            yTableText(m_ui->formatsTable, row, 0, id);
            yTableText(m_ui->formatsTable, row, 1, ext);
            yTableText(m_ui->formatsTable, row, 2, res);
            yTableText(m_ui->formatsTable, row, 3, fpsStr);
            yTableText(m_ui->formatsTable, row, 4, tbrStr);
            yTableText(m_ui->formatsTable, row, 5, codecs);
            yTableText(m_ui->formatsTable, row, 6, sizeStr);
        }
    }

    /* 字幕语言（subtitles 对象的 key） */
    QString subs;
    int nsub = 0;
    cJSON* subObj = cJSON_GetObjectItem(root, "subtitles");
    if (subObj && subObj->type == cJSON_Object) {
        cJSON* it = subObj->child;
        while (it) {
            if (it->string) {
                if (nsub > 0) { subs += ","; }
                if (nsub < 6) { subs += qFromUtf8(it->string); }
                nsub++;
            }
            it = it->next;
        }
        if (nsub > 6) { subs += qFromUtf8("…等"); }
    }
    QString status = qFromUtf8("共 ") + QString::number(nfmt) +
        qFromUtf8(" 个格式");
    if (!title.isEmpty()) {
        status = "《" + title + "》 " + status;
    }
    if (!uploader.isEmpty()) {
        status += qFromUtf8("  [") + uploader + "]";
    }
    if (nsub > 0) {
        status += qFromUtf8(" · 字幕: ") + subs;
    } else {
        status += qFromUtf8(" · 无字幕");
    }
    m_ui->probeStatus->setText(status);
    cJSON_Delete(root);
}

void YtdlpLogic::onFillFormat() {
    if (!m_ui) { return; }
#ifdef QT3_BUILD
    int row = m_ui->formatsTable->currentRow();
#else
    int row = m_ui->formatsTable->currentRow();
#endif
    if (row < 0) {
        m_ui->probeStatus->setText(qFromUtf8("请先点选一行格式"));
        return;
    }
    QString id;
#ifdef QT3_BUILD
    id = m_ui->formatsTable->text(row, 0);
#else
    QTableWidgetItem* it = m_ui->formatsTable->item(row, 0);
    if (it) { id = it->text(); }
#endif
    if (id.isEmpty()) {
        m_ui->probeStatus->setText(qFromUtf8("该行无格式 ID"));
        return;
    }
    m_ui->fmtCustom->setText(id);
#ifdef QT3_BUILD
    m_ui->presetH->setCurrentItem(3);
#else
    m_ui->presetH->setCurrentIndex(3);
#endif
    onFormChanged();
}

/* ═══════════ connect ═══════════ */

void YtdlpLogic::connectAll() {
    if (!m_ui) { return; }
    YtdlpUi* ui = m_ui;

    connect(ui->urlsEdit, SIGNAL(textChanged()), this, SLOT(onFormChanged()));
    connect(ui->presetH, SIGNAL(activated(int)), this, SLOT(onFormChanged()));
    connect(ui->heightCap, SIGNAL(valueChanged(int)), this, SLOT(onFormChanged()));
    connect(ui->fmtCustom, SIGNAL(textChanged(const QString&)), this, SLOT(onFormChanged()));
    connect(ui->extractAudio, SIGNAL(toggled(bool)), this, SLOT(onFormChanged()));
    connect(ui->audioFmt, SIGNAL(activated(int)), this, SLOT(onFormChanged()));
    connect(ui->audioQual, SIGNAL(activated(int)), this, SLOT(onFormChanged()));
    connect(ui->dirEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onFormChanged()));
    connect(ui->useArchive, SIGNAL(toggled(bool)), this, SLOT(onFormChanged()));
    connect(ui->archiveEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onFormChanged()));
    connect(ui->itemsSpec, SIGNAL(textChanged(const QString&)), this, SLOT(onFormChanged()));
    connect(ui->cookieEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onFormChanged()));
    connect(ui->userEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onFormChanged()));
    connect(ui->passEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onFormChanged()));
    connect(ui->proxyEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onFormChanged()));
    connect(ui->rateEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onFormChanged()));
    connect(ui->retriesSpin, SIGNAL(valueChanged(int)), this, SLOT(onFormChanged()));
    connect(ui->concSpin, SIGNAL(valueChanged(int)), this, SLOT(onFormChanged()));
    connect(ui->embedSubs, SIGNAL(toggled(bool)), this, SLOT(onFormChanged()));
    connect(ui->embedThumb, SIGNAL(toggled(bool)), this, SLOT(onFormChanged()));
    connect(ui->embedMeta, SIGNAL(toggled(bool)), this, SLOT(onFormChanged()));
    connect(ui->embedChapters, SIGNAL(toggled(bool)), this, SLOT(onFormChanged()));
    connect(ui->subLangsEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onFormChanged()));
    connect(ui->convertSubs, SIGNAL(activated(int)), this, SLOT(onFormChanged()));
    connect(ui->mergeFmt, SIGNAL(activated(int)), this, SLOT(onFormChanged()));
    connect(ui->sbRemove, SIGNAL(toggled(bool)), this, SLOT(onFormChanged()));
    connect(ui->sbCatsEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onFormChanged()));
    connect(ui->ppaEdit, SIGNAL(textChanged(const QString&)), this, SLOT(onFormChanged()));

    connect(ui->dirPick, SIGNAL(clicked()), this, SLOT(onPickDir()));
    connect(ui->archivePick, SIGNAL(clicked()), this, SLOT(onPickArchive()));
    connect(ui->cookiePick, SIGNAL(clicked()), this, SLOT(onPickCookie()));
    connect(ui->btnProbe, SIGNAL(clicked()), this, SLOT(onProbeFormats()));
    connect(ui->btnFill, SIGNAL(clicked()), this, SLOT(onFillFormat()));
    connect(ui->btnStart, SIGNAL(clicked()), this, SLOT(onStartClicked()));
    connect(ui->btnStop, SIGNAL(clicked()), this, SLOT(onStopClicked()));
}

/* ═══════════ yt-dlp 探测 ═══════════ */

void YtdlpLogic::probeYtdlp() {
    if (!m_ui) { return; }
    QString outErr;
    int rc = qRuncmdCaptureOuterr("yt-dlp",
        QStringList() << "--version", &outErr);
    if (rc == 0 && !outErr.isEmpty()) {
        QString ver = qStrip(outErr);
        m_ui->versionLabel->setText(qFromUtf8("yt-dlp 已就位 ") + ver);
        m_ytdlpOk = true;
    } else {
        m_ytdlpOk = false;
    }
    setEnabledStates(m_ytdlpOk);
}

void YtdlpLogic::setEnabledStates(bool ytdlpOk) {
    if (!m_ui) { return; }
    if (!ytdlpOk) {
        m_ui->versionLabel->setText(
            qFromUtf8("PATH 中未找到 yt-dlp（请自行安装 pip install yt-dlp）"));
    }
    m_ui->btnProbe->setEnabled(ytdlpOk);
    m_ui->btnStart->setEnabled(ytdlpOk);
    m_ui->btnStop->setEnabled(false);
}

/* ═══════════ 日志 ═══════════ */

void YtdlpLogic::appendLog(const QString& msg) {
    if (!m_ui) { return; }
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString line = "[" + ts + "] " + msg;
    m_ui->histLog->append(line);
    m_ui->histLog->append("");
}

void YtdlpLogic::appendLogWithPrefix(const QString& prefix, const QString& msg) {
    if (!m_ui) { return; }
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString line;
    if (prefix.isEmpty()) {
        line = "[" + ts + "] " + msg;
    } else {
        line = "[" + ts + "] " + prefix + " " + msg;
    }
    m_ui->histLog->append(line);
    m_ui->histLog->append("");
}
