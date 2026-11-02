#include "compat34.h"
#include "dblogic.h"
#include "cJSON.h"

#include <qfile.h>
#include <qdir.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qtextedit.h>
#include <qmessagebox.h>
#include <qtabwidget.h>
#include <qdatetime.h>
#include <qfiledialog.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

/* ══════════════════════════════════════════════════
 * 通用兼容工具
 * ══════════════════════════════════════════════════ */

QString sbTrim(const QString& s) {
#ifdef QT3_BUILD
    return s.stripWhiteSpace();
#else
    return s.trimmed();
#endif
}

QByteArray sbUtf8(const QString& s) {
#ifdef QT3_BUILD
    return s.utf8();
#else
    return s.toUtf8();
#endif
}

void sbAppendBytes(QByteArray& dst, const QByteArray& src) {
    if (src.size() <= 0) { return; }
    int old = (int)dst.size();
    dst.resize(old + (int)src.size());
    memcpy(dst.data() + old, src.data(), src.size());
}

bool sbWriteUtf8File(const QString& path, const QString& content) {
    QByteArray u = sbUtf8(content);
    FILE* fp = fopen(QFile::encodeName(path).data(), "wb");
    if (!fp) { return false; }
    size_t n = fwrite(u.data(), 1, (size_t)u.size(), fp);
    fclose(fp);
    return n == (size_t)u.size();
}

QString sbReadUtf8File(const QString& path) {
    QString s;
    FILE* fp = fopen(QFile::encodeName(path).data(), "rb");
    if (!fp) { return s; }
    char buf[65536];
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

QString sbFileDialogOpen(QWidget* parent, const QString& filter) {
#ifdef QT3_BUILD
    return QFileDialog::getOpenFileName(QString(), filter, parent);
#else
    return QFileDialog::getOpenFileName(parent, qFromUtf8("打开"),
                                        QString(), filter);
#endif
}

QString sbFileDialogSave(QWidget* parent, const QString& filter) {
#ifdef QT3_BUILD
    return QFileDialog::getSaveFileName(QString(), filter, parent);
#else
    return QFileDialog::getSaveFileName(parent, qFromUtf8("保存"),
                                        QString(), filter);
#endif
}

QString sbTextEdit(const QTextEdit* e) {
#ifdef QT3_BUILD
    return e->text();
#else
    return e->toPlainText();
#endif
}

void sbTextEditSet(QTextEdit* e, const QString& s) {
#ifdef QT3_BUILD
    e->setText(s);
#else
    e->setPlainText(s);
#endif
}

void sbTextEditSetPlain(QTextEdit* e) {
#ifdef QT3_BUILD
    e->setTextFormat(Qt::PlainText);
#else
    e->setAcceptRichText(false);
#endif
}

/* ── QComboBox ── */
void sbComboAdd(QComboBox* b, const QString& s) {
#ifdef QT3_BUILD
    b->insertItem(s);
#else
    b->addItem(s);
#endif
}
void sbComboClear(QComboBox* b) { b->clear(); }
int sbComboCount(const QComboBox* b) { return (int)b->count(); }
QString sbComboText(const QComboBox* b, int i) {
#ifdef QT3_BUILD
    return b->text(i);
#else
    return b->itemText(i);
#endif
}
void sbComboSetCur(QComboBox* b, int i) {
#ifdef QT3_BUILD
    b->setCurrentItem(i);
#else
    b->setCurrentIndex(i);
#endif
}
int sbComboCur(const QComboBox* b) {
#ifdef QT3_BUILD
    return b->currentItem();
#else
    return b->currentIndex();
#endif
}
bool sbComboSelect(QComboBox* b, const QString& t) {
    if (!b) { return false; }
    for (int i = 0; i < sbComboCount(b); ++i) {
        if (sbComboText(b, i) == t) {
            sbComboSetCur(b, i);
            return true;
        }
    }
    return false;
}

/* ── SbListBox ── */
void sbLbAdd(SbListBox* lb, const QString& s) {
#ifdef QT3_BUILD
    lb->insertItem(s);
#else
    lb->addItem(s);
#endif
}
void sbLbClear(SbListBox* lb) { lb->clear(); }
int sbLbCount(const SbListBox* lb) { return (int)lb->count(); }
int sbLbCurrent(const SbListBox* lb) {
#ifdef QT3_BUILD
    return lb->currentItem();
#else
    return lb->currentRow();
#endif
}
QString sbLbText(const SbListBox* lb, int i) {
#ifdef QT3_BUILD
    return lb->text(i) ? lb->text(i) : QString();
#else
    QListWidgetItem* it = lb->item(i);
    return it ? it->text() : QString();
#endif
}

/* ── 表格 ── */
void sbTableSetSize(SbTable* t, int rows, int cols, const QStringList& headers) {
#ifdef QT3_BUILD
    t->setNumRows(rows);
    t->setNumCols(cols);
    for (int c = 0; c < cols; ++c) {
        t->horizontalHeader()->setLabel(c, c < (int)headers.count()
            ? headers[c] : QString());
    }
#else
    t->setRowCount(rows);
    t->setColumnCount(cols);
    for (int c = 0; c < cols; ++c) {
        t->setHorizontalHeaderItem(c, new QTableWidgetItem(
            c < (int)headers.count() ? headers[c] : QString()));
    }
#endif
}

void sbTableSetText(SbTable* t, int r, int c, const QString& s) {
#ifdef QT3_BUILD
    t->setText(r, c, s);
#else
    QTableWidgetItem* it = t->item(r, c);
    if (it) {
        it->setText(s);
    } else {
        t->setItem(r, c, new QTableWidgetItem(s));
    }
#endif
}

QString sbTableText(const SbTable* t, int r, int c) {
#ifdef QT3_BUILD
    return t->text(r, c) ? t->text(r, c) : QString();
#else
    QTableWidgetItem* it = t->item(r, c);
    return it ? it->text() : QString();
#endif
}

void sbTableSetReadOnly(SbTable* t, bool redOn) {
#ifdef QT3_BUILD
    t->setReadOnly(redOn);
#else
    if (redOn) {
        t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    } else {
        t->setEditTriggers(QAbstractItemView::DoubleClicked |
                           QAbstractItemView::EditKeyPressed |
                           QAbstractItemView::AnyKeyPressed);
    }
#endif
}

void sbTableFitCols(SbTable* t) {
#ifdef QT3_BUILD
    for (int c = 0; c < t->numCols(); ++c) {
        t->adjustColumn(c);
    }
#else
    t->resizeColumnsToContents();
#endif
}

/* ── QTabWidget 兼容 ── */
void sbTabAdd(QTabWidget* w, QWidget* page, const QString& label) {
    w->addTab(page, label);
}

int sbTabCount(const QTabWidget* w) { return (int)w->count(); }

int sbTabIndex(const QTabWidget* w) {
#ifdef QT3_BUILD
    return w->currentPageIndex();
#else
    return w->currentIndex();
#endif
}

void sbTabSetCurrent(QTabWidget* w, int i) {
#ifdef QT3_BUILD
    w->setCurrentPage(i);
#else
    w->setCurrentIndex(i);
#endif
}

QWidget* sbTabPage(QTabWidget* w, int i) {
#ifdef QT3_BUILD
    return w->page(i);
#else
    return w->widget(i);
#endif
}

void sbTabRemove(QTabWidget* w, int i) {
#ifdef QT3_BUILD
    w->removePage(w->page(i));
#else
    w->removeTab(i);
#endif
}

/* ── SQL 转义 ── */
QString sbSqlEscapeText(const QString& s) {
    QString out;
    for (int i = 0; i < (int)s.length(); ++i) {
        char c = sbCharAt(s, i);
        if (c == '\'') {
            out += QString("''");
        } else {
            out += s[i];
        }
    }
    return "'" + out + "'";
}

QString sbSqlBlobLiteralHex(const unsigned char* data, int len) {
    static const char* HEXD = "0123456789ABCDEF";
    QString hex;
    for (int i = 0; i < len; ++i) {
        hex += QChar(HEXD[(data[i] >> 4) & 0xF]);
        hex += QChar(HEXD[data[i] & 0xF]);
    }
    return "X'" + hex + "'";
}

/* 标识符包裹 */
static void sbQuoteIdentInto(const QString& name, QString& out) {
    out += '"';
    for (int i = 0; i < (int)name.length(); ++i) {
        if (sbCharAt(name, i) == '"') {
            out += '"';
            out += '"';
        } else {
            out += name[i];
        }
    }
    out += '"';
}
QString sbSqlIdent(const QString& name) {
    QString out;
    sbQuoteIdentInto(name, out);
    return out;
}

/* X'..' blob 字面量检测并解码 */
static bool sbParseHexBlobLiteral(const QString& s, QByteArray& bytes) {
    if ((int)s.length() < 3) { return false; }
    if (!s.startsWith("X'") ) { return false; }
    int end = (int)s.length() - 1;
    if (s[end] != '\'') { return false; }
    QString hex = s.mid(2, end - 2);
    int hn = (int)hex.length();
    if (hn % 2 != 0) { return false; }
    for (int i = 0; i < hn; ++i) {
        char c = sbCharAt(hex, i);
        bool okd = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
                   (c >= 'A' && c <= 'F');
        if (!okd) { return false; }
    }
    bytes.resize(hn / 2);
    for (int i = 0; i < hn / 2; ++i) {
        int hi, lo;
        char c1 = sbCharAt(hex, i * 2);
        char c2 = sbCharAt(hex, i * 2 + 1);
        hi = (c1 >= '0' && c1 <= '9') ? c1 - '0' :
             (c1 < 'a' ? c1 - 'A' + 10 : c1 - 'a' + 10);
        lo = (c2 >= '0' && c2 <= '9') ? c2 - '0' :
             (c2 < 'a' ? c2 - 'A' + 10 : c2 - 'a' + 10);
        bytes[i] = (char)((hi << 4) | lo);
    }
    return true;
}

/* 路径 */
QString sbBaseDir() {
#ifdef QT3_BUILD
    QString home = QDir::homeDirPath();
#else
    QString home = QDir::homePath();
#endif
    return home + "/.config/" SB_BASE_DIR_NAME;
}

void sbEnsureDirs() {
#ifdef QT3_BUILD
    QString home = QDir::homeDirPath();
#else
    QString home = QDir::homePath();
#endif
    mkdir(QFile::encodeName(home + "/.config").data(), 0755);
    mkdir(QFile::encodeName(sbBaseDir()).data(), 0755);
}

/* ══════════════════════════════════════════════════
 * DbSession
 * ══════════════════════════════════════════════════ */

DbSession::DbSession() : db(0), ro(false) {}

DbSession::~DbSession() { close(); }

bool DbSession::open(const QString& p, bool readOnly, QString& err) {
    close();
    ro = readOnly;
    path = p;
    int flags = ro ? SQLITE_OPEN_READONLY : SQLITE_OPEN_READWRITE;
    if (p == ":memory:") {
        ro = false;
        flags = SQLITE_OPEN_READWRITE;
    }
    int rc = sqlite3_open_v2(QFile::encodeName(p).data(), &db, flags, 0);
    if (rc != SQLITE_OK || !db) {
        err = db ? qFromUtf8(sqlite3_errmsg(db))
                 : qFromUtf8("无法创建数据库连接");
        if (db) { close(); }
        return false;
    }
    return true;
}

bool DbSession::createEmptyFile(const QString& p, QString& err) {
    if (QFile::exists(p)) {
        err = qFromUtf8("文件已存在：") + p;
        return false;
    }
    sqlite3* ndb = 0;
    int rc = sqlite3_open_v2(QFile::encodeName(p).data(), &ndb,
                             SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, 0);
    if (rc != SQLITE_OK || !ndb) {
        err = qFromUtf8("创建失败：") +
              (ndb ? qFromUtf8(sqlite3_errmsg(ndb)) : QString());
        if (ndb) { sqlite3_close(ndb); }
        return false;
    }
    sqlite3_close(ndb);
    return true;
}

void DbSession::close() {
    if (db) {
        sqlite3_close(db);
        db = 0;
    }
    ro = false;
    path = QString();
}

bool DbSession::vacuum(QString& err) {
    return runSql("VACUUM", err, 0);
}

bool DbSession::execScript(const QString& sql, QString& err) {
    if (!db || ro) {
        err = qFromUtf8("数据库未打开或为只读");
        return false;
    }
    QByteArray s = sbUtf8(sql);
    char* msg = 0;
    int rc = sqlite3_exec(db, s.data(), 0, 0, &msg);
    if (rc != SQLITE_OK) {
        err = msg ? qFromUtf8(msg) : qFromUtf8("SQL 执行失败");
        if (msg) { sqlite3_free(msg); }
        return false;
    }
    return true;
}

bool DbSession::runSql(const QString& sql, QString& err, sb_int64* changes) {
    if (changes) { *changes = 0; }
    if (!db || ro) {
        err = qFromUtf8("数据库未打开或为只读");
        return false;
    }
    QByteArray s = sbUtf8(sql);
    char* msg = 0;
    sqlite3_stmt* stmt = 0;
    const char* tail = s.data();
    bool anyResult = false;
    while (tail && *tail) {
        int rc = sqlite3_prepare_v2(db, tail, -1, &stmt, &tail);
        if (rc != SQLITE_OK) {
            err = qFromUtf8(sqlite3_errmsg(db));
            if (stmt) { sqlite3_finalize(stmt); }
            return false;
        }
        if (!stmt) { continue; }
        rc = sqlite3_step(stmt);
        if (rc == SQLITE_DONE) {
            /* 非 SELECT 语句 */
            if (changes) { *changes += sqlite3_changes(db); }
        } else if (rc == SQLITE_ROW) {
            anyResult = true;
        } else {
            err = qFromUtf8(sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            return false;
        }
        sqlite3_finalize(stmt);
        stmt = 0;
    }
    if (anyResult && changes) { *changes = 0; }
    return true;
}

void DbSession::gridFromStmt(sqlite3_stmt* stmt, SbGrid& grid) {
    int cols = sqlite3_column_count(stmt);
    for (int c = 0; c < cols; ++c) {
        const char* nm = sqlite3_column_name(stmt, c);
        grid.headers.append(nm ? qFromUtf8(nm) : QString());
    }
    SbCellRow cells;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cells.clear();
        SbCell cp;
        for (int c = 0; c < cols; ++c) {
            int ty = sqlite3_column_type(stmt, c);
            if (ty == SQLITE_BLOB) {
                const void* bl = sqlite3_column_blob(stmt, c);
                int nb = sqlite3_column_bytes(stmt, c);
                if (nb <= 0 || !bl) {
                    cp.text = qFromUtf8("X'' (0字节)");
                    cp.kind = 1;
                } else {
                    cp.text = sbSqlBlobLiteralHex((const unsigned char*)bl, nb);
                    QString shown = cp.text;
                    if ((int)shown.length() > 120) {
                        shown = shown.left(114) + "...";
                    }
                    shown += QString::fromLatin1(" (") +
                             QString::number(nb) +
                             QString::fromLatin1("字节)");
                    cp.text = shown;
                    cp.kind = 1;
                }
            } else if (ty == SQLITE_NULL) {
                cp.text = QString();
                cp.kind = 2;
            } else {
                const char* tx = (const char*)sqlite3_column_text(stmt, c);
                cp.text = tx ? qFromUtf8(tx) : QString();
                cp.kind = 0;
            }
            cells.append(cp);
        }
        grid.rows.append(cells);
        if ((int)grid.rows.size() >= SB_GRID_MAX_ROWS) { break; }
    }
}

bool DbSession::execQuery(const QString& sql, SbGrid& grid, QString& err,
                          sb_int64* changes) {
    grid.clear();
    if (changes) { *changes = 0; }
    if (!db) {
        err = qFromUtf8("数据库未打开");
        return false;
    }
    QByteArray s = sbUtf8(sql);
    const char* tail = s.data();
    sqlite3_stmt* stmt = 0;
    bool sawErr = false;
    while (tail && *tail) {
        int rc = sqlite3_prepare_v2(db, tail, -1, &stmt, &tail);
        if (rc != SQLITE_OK) {
            err = qFromUtf8(sqlite3_errmsg(db));
            if (stmt) { sqlite3_finalize(stmt); }
            return false;
        }
        if (!stmt) { continue; }
        int cols = sqlite3_column_count(stmt);
        if (cols == 0) {
            rc = sqlite3_step(stmt);
            if (rc == SQLITE_DONE) {
                if (changes) { *changes += sqlite3_changes(db); }
            } else if (rc != SQLITE_OK && rc != SQLITE_ROW) {
                err = qFromUtf8(sqlite3_errmsg(db));
                sawErr = true;
            }
            sqlite3_finalize(stmt);
            stmt = 0;
            if (sawErr) { return false; }
            continue;
        }
        /* 有结果集：覆盖式捕获（保留最后一条结果集语句） */
        SbGrid pg;
        gridFromStmt(stmt, pg);
        grid = pg;
        sqlite3_finalize(stmt);
        stmt = 0;
    }
    return !sawErr;
}

/* ── schema 元数据 ── */

static bool stmtSingleText(sqlite3* db, const char* sql, QString& out) {
    sqlite3_stmt* stmt = 0;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK || !stmt) {
        if (stmt) { sqlite3_finalize(stmt); }
        return false;
    }
    bool got = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* tx = (const char*)sqlite3_column_text(stmt, 0);
        out = tx ? qFromUtf8(tx) : QString();
        got = true;
    }
    sqlite3_finalize(stmt);
    return got;
}

QStringList DbSession::listObjects(const char* type) {
    QStringList out;
    if (!db) { return out; }
    QString sql = "SELECT name FROM sqlite_master WHERE name NOT LIKE 'sqlite\\_%' ESCAPE '\\'";
    if (type && type[0]) {
        sql += " AND type = '";
        QString t = QString::fromLatin1(type);
        for (int i = 0; i < (int)t.length(); ++i) { sql += t[i]; }
        sql += "'";
    }
    sql += " ORDER BY name";
    sqlite3_stmt* stmt = 0;
    if (sqlite3_prepare_v2(db, sbUtf8(sql).data(), -1, &stmt, 0) == SQLITE_OK &&
        stmt) {
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* nm = (const char*)sqlite3_column_text(stmt, 0);
            out.append(nm ? qFromUtf8(nm) : QString());
        }
    }
    if (stmt) { sqlite3_finalize(stmt); }
    return out;
}

QString DbSession::objectDdl(const QString& name) {
    if (!db) { return QString(); }
    QString out;
    QString sql = "SELECT sql FROM sqlite_master WHERE name = " +
                  sbSqlEscapeText(name);
    stmtSingleText(db, sbUtf8(sql).data(), out);
    return out;
}

QString DbSession::objectType(const QString& name) {
    if (!db) { return QString(); }
    QString out;
    QString sql = "SELECT type FROM sqlite_master WHERE name = " +
                  sbSqlEscapeText(name);
    stmtSingleText(db, sbUtf8(sql).data(), out);
    return out;
}

bool DbSession::tableExists(const QString& name) {
    if (!db) { return false; }
    QString out;
    QString sql = "SELECT count(*) FROM sqlite_master WHERE type='table'" +
                  QString(" AND name = ") + sbSqlEscapeText(name);
    return stmtSingleText(db, sbUtf8(sql).data(), out) && out == "1";
}

SbColumns DbSession::tableColumns(const QString& table, QString& err) {
    SbColumns out;
    if (!db) {
        err = qFromUtf8("数据库未打开");
        return out;
    }
    QString sql = "PRAGMA table_info(" + sbSqlIdent(table) + ")";
    sqlite3_stmt* stmt = 0;
    if (sqlite3_prepare_v2(db, sbUtf8(sql).data(), -1, &stmt, 0) != SQLITE_OK) {
        err = qFromUtf8(sqlite3_errmsg(db));
        if (stmt) { sqlite3_finalize(stmt); }
        return out;
    }
    if (!stmt) { return out; }
    /* cid,name,type,notnull,dflt_value,pk */
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        SbColumn col;
        const char* nm = (const char*)sqlite3_column_text(stmt, 1);
        const char* ty = (const char*)sqlite3_column_text(stmt, 2);
        const char* df = (const char*)sqlite3_column_text(stmt, 4);
        col.name = nm ? qFromUtf8(nm) : QString();
        col.type = ty ? qFromUtf8(ty) : QString();
        col.notnull = sqlite3_column_int(stmt, 3) != 0;
        col.pk = sqlite3_column_int(stmt, 5) != 0;
        col.dflt = df ? qFromUtf8(df) : QString();
        out.append(col);
    }
    sqlite3_finalize(stmt);
    return out;
}

bool DbSession::checkRowidSupport(const QString& table, bool& hasRowid,
                                  QString&) {
    hasRowid = false;
    if (!db) { return false; }
    QString sql = "SELECT rowid FROM " + sbSqlIdent(table) + " LIMIT 1";
    sqlite3_stmt* stmt = 0;
    if (sqlite3_prepare_v2(db, sbUtf8(sql).data(), -1, &stmt, 0)
        != SQLITE_OK) {
        if (stmt) { sqlite3_finalize(stmt); }
        return false;
    }
    hasRowid = true;
    sqlite3_finalize(stmt);
    return true;
}

/* ── 浏览数据 ── */

int DbSession::tableRows(const QString& table, const QString& where,
                         QString& err) {
    if (!db) {
        err = qFromUtf8("数据库未打开");
        return -1;
    }
    QString sql = "SELECT count(*) FROM " + sbSqlIdent(table) +
                  " WHERE " + (where.isEmpty() ? "1=1" : where);
    sqlite3_stmt* stmt = 0;
    if (sqlite3_prepare_v2(db, sbUtf8(sql).data(), -1, &stmt, 0)
        != SQLITE_OK || !stmt) {
        err = qFromUtf8(sqlite3_errmsg(db));
        if (stmt) { sqlite3_finalize(stmt); }
        return -1;
    }
    int n = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        n = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return n;
}

bool DbSession::fetchPage(const QString& table, sb_int64 offset, int limit,
                          const QString& orderBySql, const QString& where,
                          SbBrowsePage& page, QString& err) {
    page.clear();
    if (!db) {
        err = qFromUtf8("数据库未打开");
        return false;
    }
    bool useRowid = page.hasRowid;
    QString colList;
    if (useRowid) {
        colList = "\"rowid\",*";
    } else {
        colList = "*";
    }
    QString sql = "SELECT " + colList + " FROM " + sbSqlIdent(table);
    if (!where.isEmpty()) { sql += " WHERE " + where; }
    if (!orderBySql.isEmpty()) { sql += " ORDER BY " + orderBySql; }
    sql += " LIMIT " + QString::number(limit) +
           " OFFSET " + QString::number(offset);

    sqlite3_stmt* stmt = 0;
    if (sqlite3_prepare_v2(db, sbUtf8(sql).data(), -1, &stmt, 0)
        != SQLITE_OK || !stmt) {
        err = qFromUtf8(sqlite3_errmsg(db));
        if (stmt) { sqlite3_finalize(stmt); }
        return false;
    }
    int cols = sqlite3_column_count(stmt);
    int dataOff = useRowid ? 1 : 0;
    for (int c = dataOff; c < cols; ++c) {
        const char* nm = sqlite3_column_name(stmt, c);
        page.headers.append(nm ? qFromUtf8(nm) : QString());
    }
    SbStrRow row;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        row.clear();
        for (int c = dataOff; c < cols; ++c) {
            int ty = sqlite3_column_type(stmt, c);
            QString tv;
            if (ty == SQLITE_BLOB) {
                const void* bl = sqlite3_column_blob(stmt, c);
                int nb = sqlite3_column_bytes(stmt, c);
                tv = (nb > 0 && bl)
                    ? sbSqlBlobLiteralHex((const unsigned char*)bl, nb)
                    : QString::fromLatin1("X''");
                QString shown = tv;
                if ((int)shown.length() > 100) { shown = shown.left(94) + "..."; }
                shown += QString::fromLatin1(" (");
                shown += QString::number(nb);
                shown += QString::fromLatin1("字节)");
                tv = shown;
            } else if (ty != SQLITE_NULL) {
                const char* tx = (const char*)sqlite3_column_text(stmt, c);
                tv = tx ? qFromUtf8(tx) : QString();
            }
            /* NULL 与空文本区分交给 updateCell 判断（kind 解析时再判定） */
            if (ty == SQLITE_NULL) {
                tv = QString::fromLatin1("(NULL)");
            }
            row.append(tv);
        }
        page.cells.append(row);
        page.hasRowid = useRowid;
        if (useRowid) {
            page.rowids.append(sqlite3_column_int64(stmt, 0));
        } else {
            page.rowids.append(-1);
        }
    }
    sqlite3_finalize(stmt);
    page.hasRowid = useRowid;
    return true;
}

bool DbSession::runWhereRowid(const QString& table, sb_int64 rowid,
                              const QString& setSql, QString& err,
                              sb_int64* changes) {
    if (!db) {
        err = qFromUtf8("数据库未打开");
        return false;
    }
    if (ro) {
        err = qFromUtf8("数据库为只读");
        return false;
    }
    QString sql = setSql + " WHERE \"rowid\" = " + QString::number(rowid);
    return runSql(sql, err, changes);
}

bool DbSession::updateCell(const QString& table, sb_int64 rowid,
                           const QString& colName, const QString& valueLiteral,
                           QString& err) {
    QString sql = "UPDATE " + sbSqlIdent(table) + " SET " +
                  sbSqlIdent(colName) + " = " + valueLiteral;
    sb_int64 ch = 0;
    return runWhereRowid(table, rowid, sql, err, &ch);
}

bool DbSession::insertDefaultRow(const QString& table, QString& err) {
    QString sql = "INSERT INTO " + sbSqlIdent(table) + " DEFAULT VALUES";
    return runSql(sql, err, 0);
}

bool DbSession::deleteRowByRowid(const QString& table, sb_int64 rowid,
                                 QString& err) {
    QString sql = "DELETE FROM " + sbSqlIdent(table);
    sb_int64 ch = 0;
    return runWhereRowid(table, rowid, sql, err, &ch);
}

/* ── pragma ── */
bool DbSession::pragmaText(const QString& name, QString& out) {
    if (!db) { return false; }
    QString sql = "PRAGMA " + name;
    return stmtSingleText(db, sbUtf8(sql).data(), out);
}

bool DbSession::pragmaSet(const QString& name, const QString& value,
                          QString& err) {
    if (!db || ro) {
        err = qFromUtf8("数据库未打开或为只读");
        return false;
    }
    QString sql = "PRAGMA " + name + " = " + value;
    return runSql(sql, err, 0);
}

bool DbSession::cellLiteral(const QString& table, sb_int64 rowid,
                            const QString& colName, QString& outLiteral,
                            QString& err) {
    outLiteral = QString();
    if (!db) {
        err = qFromUtf8("数据库未打开");
        return false;
    }
    QString sql = "SELECT quote(" + sbSqlIdent(colName) + ") FROM " +
                  sbSqlIdent(table) + " WHERE rowid = " +
                  QString::number((long long)rowid);
    sqlite3_stmt* stmt = 0;
    if (sqlite3_prepare_v2(db, sbUtf8(sql).data(), -1, &stmt, 0)
        != SQLITE_OK || !stmt) {
        err = qFromUtf8(sqlite3_errmsg(db));
        if (stmt) { sqlite3_finalize(stmt); }
        return false;
    }
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char* tx = (const char*)sqlite3_column_text(stmt, 0);
        outLiteral = tx ? qFromUtf8(tx) : QString();
    }
    sqlite3_finalize(stmt);
    return true;
}

bool DbSession::parseHexLiteral(const QString& s, QByteArray& bytes) {
    return sbParseHexBlobLiteral(s, bytes);
}

/* ══════════════════════════════════════════════════
 * CSV 解析
 * ══════════════════════════════════════════════════ */

static int csvGetC(FILE* fp) { return fgetc(fp); }

bool DbSession::csvNextRecord(FILE* fp, QStringList& out, bool& eof,
                              QString& err) {
    out.clear();
    QString field;
    bool inQuotes = false;
    bool sawAny = false;
    int c;
    for (;;) {
        c = csvGetC(fp);
        if (c == EOF) {
            if (sawAny) {
                out.append(field);
                eof = true;
                return true;
            }
            eof = true;
            return false;
        }
        sawAny = true;
        if (inQuotes) {
            if (c == '"') {
                int nc = csvGetC(fp);
                if (nc == '"') {
                    field += '"';
                } else {
                    inQuotes = false;
                    if (nc != EOF) { ungetc(nc, fp); }
                }
            } else {
                field += (char)c;
            }
            continue;
        }
        if (c == '"') {
            inQuotes = true;
            continue;
        }
        if (c == ',') {
            out.append(field);
            field = QString();
            continue;
        }
        if (c == '\n') {
            out.append(field);
            eof = false;
            return true;
        }
        if (c == '\r') {
            int nc = csvGetC(fp);
            if (nc != '\n' && nc != EOF) { ungetc(nc, fp); }
            out.append(field);
            eof = false;
            return true;
        }
        /* 多字节 UTF-8 直通 */
        field += (char)c;
    }
    err = QString();  /* never; swallow warning */
}

/* 写一个 CSV 字段（自动加引号） */
static void csvWriteField(FILE* fp, const QString& v) {
    QByteArray u = sbUtf8(v);
    fprintf(fp, "\"");
    for (int i = 0; i < (int)u.size(); ++i) {
        char c = u.data()[i];
        if (c == '"') { fprintf(fp, "\"\""); }
        else if (c == '\r') { }
        else { fprintf(fp, "%c", c); }
    }
    fprintf(fp, "\"");
}

bool DbSession::exportTableCsv(const QString& table, const QString& file,
                               QString& err, bool includeHeader) {
    if (!db) {
        err = qFromUtf8("数据库未打开");
        return false;
    }
    SbColumns cols = tableColumns(table, err);
    if ((int)cols.size() <= 0) {
        err = qFromUtf8("读取表结构失败：") + err;
        return false;
    }
    FILE* fp = fopen(QFile::encodeName(file).data(), "wb");
    if (!fp) {
        err = qFromUtf8("无法创建 CSV 文件：") + file;
        return false;
    }
    if (includeHeader) {
        for (int i = 0; i < (int)cols.size(); ++i) {
            if (i > 0) { fprintf(fp, ","); }
            csvWriteField(fp, cols[i].name);
        }
        fprintf(fp, "\n");
    }
    QString sql = "SELECT * FROM " + sbSqlIdent(table);
    sqlite3_stmt* stmt = 0;
    if (sqlite3_prepare_v2(db, sbUtf8(sql).data(), -1, &stmt, 0)
        != SQLITE_OK || !stmt) {
        err = qFromUtf8(sqlite3_errmsg(db));
        if (stmt) { sqlite3_finalize(stmt); }
        fclose(fp);
        return false;
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        for (int c = 0; c < (int)cols.size(); ++c) {
            if (c > 0) { fprintf(fp, ","); }
            int ty = sqlite3_column_type(stmt, c);
            if (ty == SQLITE_BLOB) {
                const unsigned char* bl =
                    (const unsigned char*)sqlite3_column_blob(stmt, c);
                int nb = sqlite3_column_bytes(stmt, c);
                csvWriteField(fp, sbSqlBlobLiteralHex(bl, nb));
            } else if (ty == SQLITE_NULL) {
                /* NULL 与空文本同形，CSV 语义有关全表结构，导空 */
                fprintf(fp, "\"\"");
            } else {
                const char* tx = (const char*)sqlite3_column_text(stmt, c);
                csvWriteField(fp, tx ? qFromUtf8(tx) : QString());
            }
        }
        fprintf(fp, "\n");
    }
    sqlite3_finalize(stmt);
    fclose(fp);
    return true;
}

bool DbSession::importCsvInto(const QString& table, const QString& file,
                              QString& err, bool firstRowHeader,
                              sb_int64* inserted) {
    if (!db || ro) {
        err = qFromUtf8("数据库未打开或为只读");
        return false;
    }
    SbColumns cols = tableColumns(table, err);
    if ((int)cols.size() <= 0) {
        err = qFromUtf8("读取表结构失败：") + err;
        return false;
    }
    FILE* fp = fopen(QFile::encodeName(file).data(), "rb");
    if (!fp) {
        err = qFromUtf8("无法打开 CSV 文件：") + file;
        return false;
    }
    /* UTF-8 BOM 跳过 */
    { int b1 = fgetc(fp);
      int b2 = fgetc(fp);
      int b3 = fgetc(fp);
      if (b1 != 0xEF || b2 != 0xBB || b3 != 0xBF) {
          if (b1 != EOF) { ungetc(b1, fp); }
          if (b2 != EOF) { ungetc(b2, fp); }
          if (b3 != EOF) { ungetc(b3, fp); }
      } }
    QStringList hdr;
    if (firstRowHeader) {
        hdr.clear();
        bool eofh = false;
        csvNextRecord(fp, hdr, eofh, err);
        if ((int)hdr.count() != (int)cols.size()) {
            err = qFromUtf8("表头列数与表字段数不符：CSV 找到 ") +
                  QString::number(hdr.count()) + "，表中是 " +
                  QString::number((int)cols.size());
            fclose(fp);
            return false;
        }
    }
    runSql("BEGIN", err, 0);
    err = QString();
    sb_int64 ins = 0;
    for (;;) {
        QStringList rec;
        bool e = false;
        if (!csvNextRecord(fp, rec, e, err)) {
            if (e) { break; }
        }
        if ((int)rec.count() != (int)cols.size()) {
            err = (qFromUtf8("数据行列数不符（") +
                   QString::number((int)rec.count()) + "，表中是 " +
                   QString::number((int)cols.size()) + "）") ;
            break;
        }
        QString sql = "INSERT INTO " + sbSqlIdent(table) + " VALUES (";
        for (int i = 0; i < (int)rec.count(); ++i) {
            if (i > 0) { sql += ","; }
            QByteArray blob;
            if (sbParseHexBlobLiteral(rec[i], blob)) {
                sql += sbSqlBlobLiteralHex((const unsigned char*)blob.data(),
                                           (int)blob.size());
            } else {
                sql += sbSqlEscapeText(rec[i]);
            }
        }
        sql += ")";
        bool ok = runSql(sql, err, 0);
        if (!ok) {
            break;
        }
        ++ins;
    }
    bool okAll = err.isEmpty();
    if (okAll) {
        runSql("COMMIT", err, 0);
    } else {
        QString rollErr;
        runSql("ROLLBACK", rollErr, 0);
        err = qFromUtf8("导入中止（已回滚）：") + err;
    }
    fclose(fp);
    if (inserted) { *inserted = ins; }
    return okAll;
}

/* ══════════════════════════════════════════════════
 * SQL dump 导出 / 导入
 * ══════════════════════════════════════════════════ */

static void dumpWriteValue(FILE* fp, sqlite3_stmt* stmt, int c) {
    int ty = sqlite3_column_type(stmt, c);
    if (ty == SQLITE_NULL) {
        fprintf(fp, "NULL");
        return;
    }
    if (ty == SQLITE_INTEGER) {
        fprintf(fp, "%lld", (long long)sqlite3_column_int64(stmt, c));
        return;
    }
    if (ty == SQLITE_FLOAT) {
        fprintf(fp, "%s", (const char*)sqlite3_column_text(stmt, c));
        return;
    }
    if (ty == SQLITE_BLOB) {
        const unsigned char* bl =
            (const unsigned char*)sqlite3_column_blob(stmt, c);
        int nb = sqlite3_column_bytes(stmt, c);
        fprintf(fp, "X'");
        for (int i = 0; i < nb; ++i) { fprintf(fp, "%02X", bl[i]); }
        fprintf(fp, "'");
        return;
    }
    /* TEXT */
    const char* tx = (const char*)sqlite3_column_text(stmt, c);
    fprintf(fp, "'");
    if (tx) {
        for (const char* p = tx; *p; ++p) {
            if (*p == '\'') { fprintf(fp, "''"); }
            else { fprintf(fp, "%c", *p); }
        }
    }
    fprintf(fp, "'");
}

bool DbSession::exportDump(const QString& file, QString& err) {
    if (!db) {
        err = qFromUtf8("数据库未打开");
        return false;
    }
    FILE* fp = fopen(QFile::encodeName(file).data(), "wb");
    if (!fp) {
        err = qFromUtf8("无法创建 SQL 文件：") + file;
        return false;
    }
    fprintf(fp, "PRAGMA foreign_keys=OFF;\nBEGIN TRANSACTION;\n");
    QStringList tables = listObjects("table");
    for (int t = 0; t < (int)tables.count(); ++t) {
        const QString& tb = tables[t];
        QString ddl = objectDdl(tb);
        fprintf(fp, "DROP TABLE IF EXISTS %s;\n", sbUtf8(sbSqlIdent(tb)).data());
        if (!ddl.isEmpty()) {
            fprintf(fp, "%s;\n", sbUtf8(ddl).data());
        }
        sqlite3_stmt* stmt = 0;
        QString sql = "SELECT * FROM " + sbSqlIdent(tb);
        if (sqlite3_prepare_v2(db, sbUtf8(sql).data(), -1, &stmt, 0)
            != SQLITE_OK) {
            err = tb + ": " + qFromUtf8(sqlite3_errmsg(db));
            if (stmt) { sqlite3_finalize(stmt); }
            fclose(fp);
            return false;
        }
        int cols = stmt ? sqlite3_column_count(stmt) : 0;
        while (stmt && sqlite3_step(stmt) == SQLITE_ROW) {
            fprintf(fp, "INSERT INTO %s VALUES (", sbUtf8(sbSqlIdent(tb)).data());
            for (int c = 0; c < cols; ++c) {
                if (c > 0) { fprintf(fp, ","); }
                dumpWriteValue(fp, stmt, c);
            }
            fprintf(fp, ");\n");
        }
        if (stmt) { sqlite3_finalize(stmt); }
    }
    /* 索引 / 视图（auto 除外） */
    for (int pass = 0; pass < 2; ++pass) {
        const char* ty = pass == 0 ? "index" : "view";
        QStringList items = listObjects(ty);
        for (int i = 0; i < (int)items.count(); ++i) {
            QString ddl = objectDdl(items[i]);
            if (!ddl.isEmpty()) {
                fprintf(fp, "%s;\n", sbUtf8(ddl).data());
            }
        }
    }
    fprintf(fp, "COMMIT;\n");
    fclose(fp);
    err = QString();
    return true;
}

bool DbSession::importDumpFile(const QString& file, QString& err) {
    QString data = sbReadUtf8File(file);
    if (data.isEmpty()) {
        err = qFromUtf8("SQL 文件为空或无法读取：") + file;
        return false;
    }
    return execScript(data, err);
}
