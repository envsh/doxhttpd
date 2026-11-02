#include "compat34.h"
#include "dblogic.h"
#include "cJSON.h"

#include <qwidget.h>
#include <qlayout.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <qcombobox.h>
#include <qtabwidget.h>
#include <qtextedit.h>
#include <qmessagebox.h>
#include <qdialog.h>
#include <qfile.h>
#include <qdir.h>
#include <qdatetime.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ══════════════════════════════════════════════════
 * UI/Qt 小工具
 * ══════════════════════════════════════════════════ */

static void sbSetWTitle(QWidget* w, const QString& t) {
#ifdef QT3_BUILD
    w->setCaption(t);
#else
    w->setWindowTitle(t);
#endif
}

static bool sbAskYesNo(QWidget* parent, const QString& title,
                       const QString& text) {
#ifdef QT3_BUILD
    return QMessageBox::information(parent, title, text,
                                    qFromUtf8("确定"), qFromUtf8("取消")) == 0;
#else
    return QMessageBox::information(parent, title, text,
                                    QMessageBox::Yes | QMessageBox::No,
                                    QMessageBox::No) == QMessageBox::Yes;
#endif
}

static void sbShowMsg(QWidget* parent, const QString& title,
                      const QString& text) {
#ifdef QT3_BUILD
    QMessageBox::information(parent, title, text, qFromUtf8("确定"));
#else
    QMessageBox::information(parent, title, text,
                             QMessageBox::Ok);
#endif
}

static QString sbUpperStr(const QString& s) {
    QString up;
    for (int i = 0; i < (int)s.length(); ++i) {
        char c = sbCharAt(s, i);
        if (c >= 'a' && c <= 'z') { c = (char)(c - 'a' + 'A'); }
        up += QChar(c);
    }
    return up;
}

static int sbFind(const QString& s, const char* sub) {
#ifdef QT3_BUILD
    return s.find(sub);
#else
    return s.indexOf(QString::fromLatin1(sub));
#endif
}

static int sbFindChar(const QString& s, char c, int from = 0) {
#ifdef QT3_BUILD
    return s.find(c, from);
#else
    return s.indexOf(QChar(c), from);
#endif
}

static bool sbContainsWordUpper(const QString& s, const char* word) {
    return sbFind(sbUpperStr(s), word) >= 0;
}

/* ══════════════════════════════════════════════════
 * 对话框
 * ══════════════════════════════════════════════════ */

/* 选表（+ CSV 首行表头开关，headerCheck=false 时隐藏） */
static bool sbPickTableDialog(QWidget* parent, DbSession* session,
                              bool headerCheck, QString& outTable,
                              bool& outHeader) {
    if (!session || !session->isOpen()) { return false; }
    QStringList tables = session->listObjects("table");
    if ((int)tables.count() <= 0) { return false; }
    QDialog dlg(parent);
    sbSetWTitle(&dlg, qFromUtf8("选择表"));
    QBoxLayout* lo = qNewBoxLayout(&dlg, QBoxLayout::TopToBottom, 10, 5);
    lo->addWidget(new QLabel(qFromUtf8("请选择目标表："), &dlg));
    SbListBox* lb = new SbListBox(&dlg);
    for (int i = 0; i < (int)tables.count(); ++i) {
        sbLbAdd(lb, tables[i]);
    }
    lo->addWidget(lb, 1);
    QCheckBox* hdrBox = 0;
    if (headerCheck) {
        hdrBox = new QCheckBox(qFromUtf8("CSV 首行是表头"), &dlg);
        hdrBox->setChecked(true);
        lo->addWidget(hdrBox);
    }
    QBoxLayout* btnRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 5);
    QPushButton* okBtn = new QPushButton(qFromUtf8("确定"), &dlg);
    QPushButton* cancelBtn = new QPushButton(qFromUtf8("取消"), &dlg);
    btnRow->addStretch(1);
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    lo->addLayout(btnRow);
    QObject::connect(okBtn, SIGNAL(clicked()), &dlg, SLOT(accept()));
    QObject::connect(cancelBtn, SIGNAL(clicked()), &dlg, SLOT(reject()));
    if (dlg.exec() != QDialog::Accepted) { return false; }
    int ci = sbLbCurrent(lb);
    if (ci < 0) { return false; }
    if (ci >= (int)tables.count()) { return false; }
    outTable = tables[ci];
    outHeader = hdrBox ? hdrBox->isChecked() : true;
    return true;
}

/* 行内单元格编辑：outNull=是否置 NULL；outText=编辑框文本（可为空串） */
static bool sbEditCellDialog(QWidget* parent, const QString& colName,
                             const QString& colType,
                             const QString& curLiteral, bool isBlob,
                             QString& outText, bool& outNull) {
    QDialog dlg(parent);
    sbSetWTitle(&dlg, qFromUtf8("编辑：") + colName);
    QBoxLayout* lo = qNewBoxLayout(&dlg, QBoxLayout::TopToBottom, 10, 5);
    QString info;
    info += qFromUtf8("列 ");
    info += colName;
    if (!colType.isEmpty()) {
        info += QString::fromLatin1("（") + colType + QString::fromLatin1("）");
    }
    if (isBlob) {
        info += qFromUtf8("（BLOB，值请写成 X'十六进制' 字面量）");
    }
    lo->addWidget(new QLabel(info, &dlg));
    QString shown = curLiteral;
    if ((int)shown.length() > 4000) {
        shown = shown.left(4000) + "...";
    }
    lo->addWidget(new QLabel(qFromUtf8("当前值：") + shown, &dlg));
    QTextEdit* ed = new QTextEdit(&dlg);
    sbTextEditSetPlain(ed);
    sbTextEditSet(ed, shown);
    ed->setMinimumHeight(140);
    lo->addWidget(ed, 1);
    QCheckBox* nullBox = new QCheckBox(qFromUtf8("设为 NULL"), &dlg);
    lo->addWidget(nullBox);
    QBoxLayout* btnRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 5);
    QPushButton* okBtn = new QPushButton(qFromUtf8("确定"), &dlg);
    QPushButton* cancelBtn = new QPushButton(qFromUtf8("取消"), &dlg);
    btnRow->addStretch(1);
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    lo->addLayout(btnRow);
    QObject::connect(okBtn, SIGNAL(clicked()), &dlg, SLOT(accept()));
    QObject::connect(cancelBtn, SIGNAL(clicked()), &dlg, SLOT(reject()));
    if (dlg.exec() != QDialog::Accepted) { return false; }
    outText = sbTextEdit(ed);
    outNull = nullBox->isChecked();
    return true;
}

/* 列定义行解析（CREATE TABLE / ADD COLUMN 共用）
 * 格式：名字 [类型] [PRIMARY KEY [AUTOINCREMENT]] [NOT NULL] [UNIQUE]
 *              [DEFAULT 值（取自该处到行尾）] */
static bool sbParseColumnSpec(const QString& line, QString& colSql,
                              QString& err) {
    QString raw = sbTrim(line);
    if (raw.isEmpty()) {
        err = qFromUtf8("空列定义");
        return false;
    }
    QStringList toks;
    int start = 0;
    int len = (int)raw.length();
    for (int i = 0; i <= len; ++i) {
        if (i == len || raw[i] == ' ' || raw[i] == '\t') {
            if (i > start) { toks.append(raw.mid(start, i - start)); }
            start = i + 1;
        }
    }
    if ((int)toks.count() <= 0) {
        err = qFromUtf8("空列定义");
        return false;
    }
    QString name = toks[0];
    if (name.isEmpty()) {
        err = qFromUtf8("列名不能为空");
        return false;
    }
    int ti = 1;
    QString type;
    if ((int)toks.count() > 1) {
        QString t1 = sbUpperStr(toks[1]);
        if (t1 == "INTEGER" || t1 == "TEXT" || t1 == "REAL" ||
            t1 == "BLOB" || t1 == "NUMERIC" || t1 == "INT" ||
            t1 == "VARCHAR" || t1 == "DATETIME" || t1 == "BOOLEAN") {
            type = toks[1];
            ti = 2;
        }
    }
    bool pk = false;
    bool autoInc = false;
    bool notnull = false;
    bool uniq = false;
    QString dflt;
    for (int i = ti; i < (int)toks.count(); ++i) {
        QString t = sbUpperStr(toks[i]);
        if (t == "PRIMARY") {
            bool hasKey = i + 1 < (int)toks.count() &&
                          sbUpperStr(toks[i + 1]) == "KEY";
            if (!hasKey) {
                err = qFromUtf8("PRIMARY 后应为 KEY：") + line;
                return false;
            }
            pk = true;
            ++i;
            if (i + 1 < (int)toks.count() &&
                sbUpperStr(toks[i + 1]) == "AUTOINCREMENT") {
                autoInc = true;
                ++i;
            }
        } else if (t == "NOT") {
            bool hasNull = i + 1 < (int)toks.count() &&
                           sbUpperStr(toks[i + 1]) == "NULL";
            if (!hasNull) {
                err = qFromUtf8("NOT 后应为 NULL：") + line;
                return false;
            }
            notnull = true;
            ++i;
        } else if (t == "UNIQUE") {
            uniq = true;
        } else if (t == "DEFAULT") {
            QString rest;
            for (int j = i + 1; j < (int)toks.count(); ++j) {
                if (!rest.isEmpty()) { rest += " "; }
                rest += toks[j];
            }
            if (rest.isEmpty()) {
                err = qFromUtf8("DEFAULT 需要值：") + line;
                return false;
            }
            dflt = rest;
            break;
        } else {
            err = qFromUtf8("无法识别的列修饰词：") + toks[i] +
                  qFromUtf8("（支持 PRIMARY KEY / AUTOINCREMENT / NOT NULL"
                            " / UNIQUE / DEFAULT）");
            return false;
        }
    }
    if (autoInc && (!pk || sbUpperStr(type) != "INTEGER")) {
        err = qFromUtf8("AUTOINCREMENT 需与 INTEGER PRIMARY KEY 同用：")
              + line;
        return false;
    }
    colSql = sbSqlIdent(name);
    if (!type.isEmpty()) { colSql += " " + type; }
    if (pk) {
        colSql += " PRIMARY KEY";
        if (autoInc) { colSql += " AUTOINCREMENT"; }
    }
    if (notnull) { colSql += " NOT NULL"; }
    if (uniq) { colSql += " UNIQUE"; }
    if (!dflt.isEmpty()) { colSql += " DEFAULT " + dflt; }
    err = QString();
    return true;
}

/* 建表对话框：表名 + 每行一列定义 */
static bool sbCreateTableDialog(QWidget* parent, QString& outSql) {
    QDialog dlg(parent);
    sbSetWTitle(&dlg, qFromUtf8("建表"));
    QBoxLayout* lo = qNewBoxLayout(&dlg, QBoxLayout::TopToBottom, 10, 5);
    QBoxLayout* nameRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    nameRow->addWidget(new QLabel(qFromUtf8("表名："), &dlg));
    QLineEdit* nameEd = new QLineEdit(&dlg);
    nameRow->addWidget(nameEd, 1);
    lo->addLayout(nameRow);
    lo->addWidget(new QLabel(qFromUtf8(
        "每行一列，格式：名字 类型 [PRIMARY KEY] [AUTOINCREMENT] "
        " [NOT NULL] [UNIQUE] [DEFAULT 值]\n"
        "例：id INTEGER PRIMARY KEY AUTOINCREMENT\n"
        "    name TEXT NOT NULL\n"
        "    val TEXT DEFAULT 'x'"), &dlg));
    QTextEdit* colEd = new QTextEdit(&dlg);
    sbTextEditSetPlain(colEd);
    sbTextEditSet(colEd,
        "id INTEGER PRIMARY KEY AUTOINCREMENT\n"
        "name TEXT NOT NULL\n"
        "val TEXT DEFAULT 'x'");
    colEd->setMinimumHeight(140);
    lo->addWidget(colEd, 1);
    QBoxLayout* btnRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 5);
    QPushButton* okBtn = new QPushButton(qFromUtf8("确定"), &dlg);
    QPushButton* cancelBtn = new QPushButton(qFromUtf8("取消"), &dlg);
    btnRow->addStretch(1);
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    lo->addLayout(btnRow);
    QObject::connect(okBtn, SIGNAL(clicked()), &dlg, SLOT(accept()));
    QObject::connect(cancelBtn, SIGNAL(clicked()), &dlg, SLOT(reject()));
    if (dlg.exec() != QDialog::Accepted) { return false; }
    QString tname = sbTrim(nameEd->text());
    if (tname.isEmpty()) {
        sbShowMsg(parent, qFromUtf8("建表"), qFromUtf8("表名不能为空"));
        return false;
    }
    QStringList lines;
    QString cols = sbTextEdit(colEd);
    int len = (int)cols.length();
    int start = 0;
    for (int i = 0; i <= len; ++i) {
        if (i == len || cols[i] == '\n') {
            if (i > start) { lines.append(cols.mid(start, i - start)); }
            start = i + 1;
        }
    }
    if ((int)lines.count() <= 0) {
        sbShowMsg(parent, qFromUtf8("建表"), qFromUtf8("至少一行列定义"));
        return false;
    }
    QString colDefs;
    for (int i = 0; i < (int)lines.count(); ++i) {
        QString def;
        QString err;
        if (!sbParseColumnSpec(lines[i], def, err)) {
            sbShowMsg(parent, qFromUtf8("建表"), err);
            return false;
        }
        if (i > 0) { colDefs += ",\n  "; }
        else { colDefs += " "; }
        colDefs += def;
    }
    outSql = "CREATE TABLE " + sbSqlIdent(tname) + " (" + colDefs + "\n)";
    return true;
}

/* 建索引/删索引对话框 */
static bool sbCreateIndexDialog(QWidget* parent,
                                DbSession* session,
                                const QString& curTable, QString& outSql) {
    if (!session || !session->isOpen()) { return false; }
    QStringList tables = session->listObjects("table");
    if ((int)tables.count() <= 0) { return false; }
    QString table = curTable;
    if (table.isEmpty() || !session->tableExists(table)) {
        table = tables[0];
    }
    QDialog dlg(parent);
    sbSetWTitle(&dlg, qFromUtf8("建索引"));
    QBoxLayout* lo = qNewBoxLayout(&dlg, QBoxLayout::TopToBottom, 10, 5);
    QBoxLayout* nameRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    nameRow->addWidget(new QLabel(qFromUtf8("索引名："), &dlg));
    QLineEdit* nameEd = new QLineEdit(&dlg);
    nameRow->addWidget(nameEd, 1);
    lo->addLayout(nameRow);
    QBoxLayout* tblRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    tblRow->addWidget(new QLabel(qFromUtf8("目标表："), &dlg));
    QComboBox* tblCombo = new QComboBox(&dlg);
    for (int i = 0; i < (int)tables.count(); ++i) {
        sbComboAdd(tblCombo, tables[i]);
    }
    sbComboSelect(tblCombo, table);
    tblRow->addWidget(tblCombo, 1);
    lo->addLayout(tblRow);
    lo->addWidget(new QLabel(qFromUtf8("列（逗号分隔）："), &dlg));
    QLineEdit* colsEd = new QLineEdit(&dlg);
    lo->addWidget(colsEd);
    QCheckBox* uniqBox = new QCheckBox(qFromUtf8("UNIQUE"), &dlg);
    lo->addWidget(uniqBox);
    lo->addWidget(new QLabel(qFromUtf8(
        "生成例如：CREATE UNIQUE INDEX i_x ON \"t\" (\"a\",\"b\")"), &dlg));
    QBoxLayout* btnRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 5);
    QPushButton* okBtn = new QPushButton(qFromUtf8("确定"), &dlg);
    QPushButton* cancelBtn = new QPushButton(qFromUtf8("取消"), &dlg);
    btnRow->addStretch(1);
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    lo->addLayout(btnRow);
    QObject::connect(okBtn, SIGNAL(clicked()), &dlg, SLOT(accept()));
    QObject::connect(cancelBtn, SIGNAL(clicked()), &dlg, SLOT(reject()));
    if (dlg.exec() != QDialog::Accepted) { return false; }
    QString iname = sbTrim(nameEd->text());
    if (iname.isEmpty()) {
        sbShowMsg(parent, qFromUtf8("建索引"), qFromUtf8("索引名不能为空"));
        return false;
    }
    QString colsRaw = sbTrim(colsEd->text());
    if (colsRaw.isEmpty()) {
        sbShowMsg(parent, qFromUtf8("建索引"), qFromUtf8("至少一个列"));
        return false;
    }
    /* 逗号/空白分隔成列名列表 */
    QStringList colNames;
    int len = (int)colsRaw.length();
    QString tok;
    for (int i = 0; i <= len; ++i) {
        bool isSep = i == len || colsRaw[i] == ',' || colsRaw[i] == ' ' ||
                     colsRaw[i] == '\t';
        if (isSep) {
            if (!tok.isEmpty()) { colNames.append(tok); }
            tok = QString();
        } else {
            tok += colsRaw[i];
        }
    }
    QString colList;
    for (int i = 0; i < (int)colNames.count(); ++i) {
        if (i > 0) { colList += ","; }
        colList += sbSqlIdent(colNames[i]);
    }
    outSql = "CREATE ";
    if (uniqBox->isChecked()) { outSql += "UNIQUE "; }
    outSql += "INDEX " + sbSqlIdent(iname) + " ON " +
              sbSqlIdent(sbComboText(tblCombo, sbComboCur(tblCombo))) +
              " (" + colList + ")";
    return true;
}

/* 改表对话框（菜单式操作 + 一/两个参数） */
#define SB_ALTER_ADDCOL      0
#define SB_ALTER_RENAMETABLE 1
#define SB_ALTER_RENAMECOL   2
#define SB_ALTER_DROPCOL     3

static bool sbAlterTableDialog(QWidget* parent, const QString& table,
                               int& op, QString& f1, QString& f2) {
    QDialog dlg(parent);
    sbSetWTitle(&dlg, qFromUtf8("改表：") + table);
    QBoxLayout* lo = qNewBoxLayout(&dlg, QBoxLayout::TopToBottom, 10, 5);
    QBoxLayout* opRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    opRow->addWidget(new QLabel(qFromUtf8("操作："), &dlg));
    QComboBox* opBox = new QComboBox(&dlg);
    sbComboAdd(opBox, qFromUtf8("新增列"));
    sbComboAdd(opBox, qFromUtf8("重命名表"));
    sbComboAdd(opBox, qFromUtf8("重命名列"));
    sbComboAdd(opBox, qFromUtf8("删除列"));
    opRow->addWidget(opBox);
    opRow->addStretch(1);
    lo->addLayout(opRow);
    lo->addWidget(new QLabel(qFromUtf8(
        "新增列：参数1 = 列定义（同建表行的格式）\n"
        "重命名表：参数1 = 新表名\n"
        "重命名列：参数1 = 旧列名，参数2 = 新列名\n"
        "删除列：参数1 = 列名"), &dlg));
    lo->addWidget(new QLabel(qFromUtf8("参数1："), &dlg));
    QLineEdit* e1 = new QLineEdit(&dlg);
    lo->addWidget(e1);
    lo->addWidget(new QLabel(qFromUtf8("参数2："), &dlg));
    QLineEdit* e2 = new QLineEdit(&dlg);
    lo->addWidget(e2);
    lo->addStretch(1);
    QBoxLayout* btnRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 5);
    QPushButton* okBtn = new QPushButton(qFromUtf8("确定"), &dlg);
    QPushButton* cancelBtn = new QPushButton(qFromUtf8("取消"), &dlg);
    btnRow->addStretch(1);
    btnRow->addWidget(okBtn);
    btnRow->addWidget(cancelBtn);
    lo->addLayout(btnRow);
    QObject::connect(okBtn, SIGNAL(clicked()), &dlg, SLOT(accept()));
    QObject::connect(cancelBtn, SIGNAL(clicked()), &dlg, SLOT(reject()));
    if (dlg.exec() != QDialog::Accepted) { return false; }
    op = sbComboCur(opBox);
    f1 = sbTrim(e1->text());
    f2 = sbTrim(e2->text());
    return true;
}

/* ══════════════════════════════════════════════════
 * SqlBrowserLogic 生命周期与状态
 * ══════════════════════════════════════════════════ */

SqlBrowserLogic::SqlBrowserLogic(QObject* parent)
    : QObject(parent) {
    session = new DbSession();
    sqlPageCount = 0;
    pageNum = 0;
    sortCol = -1;
    sortAsc = true;
    lastRows = 0;
    browseHasRowid = true;
    uiRoot = 0;
    if (parent && parent->isWidgetType()) {
        uiRoot = (QWidget*)parent;
    }
    openBtn = 0;
    newBtn = 0;
    memBtn = 0;
    closeBtn = 0;
    vacuumBtn = 0;
    roCheck = 0;
    recentsCombo = 0;
    dbLabel = 0;
    objKindCombo = 0;
    objList = 0;
    objRefreshBtn = 0;
    createTableBtn = 0;
    editTableBtn = 0;
    dropTableBtn = 0;
    createIndexBtn = 0;
    dropIndexBtn = 0;
    ddlView = 0;
    browseTableCombo = 0;
    filterEdit = 0;
    filterGoBtn = 0;
    pagePrevBtn = 0;
    pageNextBtn = 0;
    pageLabel = 0;
    browseGrid = 0;
    browseTableName = QString();
    insertRowBtn = 0;
    deleteRowBtn = 0;
    rowCountLabel = 0;
    sqlTabs = 0;
    addSqlTabBtn = 0;
    closeSqlTabBtn = 0;
    sqlErrLabel = 0;
    openSqlFileBtn = 0;
    saveSqlFileBtn = 0;
    pageSizeEdit = 0;
    journalModeCombo = 0;
    autoVacuumCombo = 0;
    foreignKeysCombo = 0;
    userVersionEdit = 0;
    pragmaApplyBtn = 0;
    pragmaRefreshBtn = 0;
    quickCheckBtn = 0;
    integrityBtn = 0;
    pragmaResultView = 0;
    importCsvBtn = 0;
    exportCsvBtn = 0;
    exportDumpBtn = 0;
    importDumpBtn = 0;
    logView = 0;
    clearLogBtn = 0;
    statusLabel = 0;
}

SqlBrowserLogic::~SqlBrowserLogic() {
    saveState();
    delete session;
    session = 0;
}

void SqlBrowserLogic::setWidgets(
        QPushButton* openBtn_, QPushButton* newBtn_,
        QPushButton* memBtn_, QPushButton* closeBtn_,
        QPushButton* vacuumBtn_, QCheckBox* roCheck_,
        QComboBox* recentsCombo_, QLabel* dbLabel_,
        QComboBox* objKindCombo_, SbListBox* objList_,
        QPushButton* objRefreshBtn_,
        QPushButton* createTableBtn_, QPushButton* editTableBtn_,
        QPushButton* dropTableBtn_, QPushButton* createIndexBtn_,
        QPushButton* dropIndexBtn_, QTextEdit* ddlView_,
        QComboBox* browseTableCombo_, QLineEdit* filterEdit_,
        QPushButton* filterGoBtn_, QPushButton* pagePrevBtn_,
        QPushButton* pageNextBtn_, QLabel* pageLabel_, SbTable* browseTable_,
        QPushButton* insertRowBtn_, QPushButton* deleteRowBtn_,
        QLabel* rowCountLabel_,
        QTabWidget* sqlTabs_, QPushButton* addSqlTabBtn_,
        QPushButton* closeSqlTabBtn_, QLabel* sqlErrLabel_,
        QPushButton* openSqlFileBtn_, QPushButton* saveSqlFileBtn_,
        QLineEdit* pageSizeEdit_, QComboBox* journalModeCombo_,
        QComboBox* autoVacuumCombo_, QComboBox* foreignKeysCombo_,
        QLineEdit* userVersionEdit_, QPushButton* pragmaApplyBtn_,
        QPushButton* pragmaRefreshBtn_, QPushButton* quickCheckBtn_,
        QPushButton* integrityBtn_, QTextEdit* pragmaResultView_,
        QPushButton* importCsvBtn_, QPushButton* exportCsvBtn_,
        QPushButton* exportDumpBtn_, QPushButton* importDumpBtn_,
        QTextEdit* logView_, QPushButton* clearLogBtn_, QLabel* statusLabel_) {
    openBtn = openBtn_;
    newBtn = newBtn_;
    memBtn = memBtn_;
    closeBtn = closeBtn_;
    vacuumBtn = vacuumBtn_;
    roCheck = roCheck_;
    recentsCombo = recentsCombo_;
    dbLabel = dbLabel_;
    objKindCombo = objKindCombo_;
    objList = objList_;
    objRefreshBtn = objRefreshBtn_;
    createTableBtn = createTableBtn_;
    editTableBtn = editTableBtn_;
    dropTableBtn = dropTableBtn_;
    createIndexBtn = createIndexBtn_;
    dropIndexBtn = dropIndexBtn_;
    ddlView = ddlView_;
    browseTableCombo = browseTableCombo_;
    filterEdit = filterEdit_;
    filterGoBtn = filterGoBtn_;
    pagePrevBtn = pagePrevBtn_;
    pageNextBtn = pageNextBtn_;
    pageLabel = pageLabel_;
    browseGrid = browseTable_;
    insertRowBtn = insertRowBtn_;
    deleteRowBtn = deleteRowBtn_;
    rowCountLabel = rowCountLabel_;
    sqlTabs = sqlTabs_;
    addSqlTabBtn = addSqlTabBtn_;
    closeSqlTabBtn = closeSqlTabBtn_;
    sqlErrLabel = sqlErrLabel_;
    openSqlFileBtn = openSqlFileBtn_;
    saveSqlFileBtn = saveSqlFileBtn_;
    pageSizeEdit = pageSizeEdit_;
    journalModeCombo = journalModeCombo_;
    autoVacuumCombo = autoVacuumCombo_;
    foreignKeysCombo = foreignKeysCombo_;
    userVersionEdit = userVersionEdit_;
    pragmaApplyBtn = pragmaApplyBtn_;
    pragmaRefreshBtn = pragmaRefreshBtn_;
    quickCheckBtn = quickCheckBtn_;
    integrityBtn = integrityBtn_;
    pragmaResultView = pragmaResultView_;
    importCsvBtn = importCsvBtn_;
    exportCsvBtn = exportCsvBtn_;
    exportDumpBtn = exportDumpBtn_;
    importDumpBtn = importDumpBtn_;
    logView = logView_;
    clearLogBtn = clearLogBtn_;
    statusLabel = statusLabel_;

    sbEnsureDirs();
    loadStateInternal();
    refreshRecentsCombo();
    if (sbTabCount(sqlTabs) <= 0) {
        addSqlPage(QString());
    }
    showStatus(qFromUtf8("就绪"));
}

void SqlBrowserLogic::loadPersist() {
    loadStateInternal();
    refreshRecentsCombo();
}

/* ── 状态持久化（state.json） ── */
void SqlBrowserLogic::loadStateInternal() {
    recents.clear();
    QString data = sbReadUtf8File(sbBaseDir() + "/state.json");
    if (data.isEmpty()) { return; }
    QByteArray b = sbUtf8(data);
    cJSON* root = cJSON_Parse(b.data());
    if (!root) { return; }
    cJSON* list = cJSON_GetObjectItem(root, "recent");
    if (list && list->type == cJSON_Array) {
        int n = cJSON_GetArraySize(list);
        for (int i = 0; i < n; ++i) {
            cJSON* o = cJSON_GetArrayItem(list, i);
            if (o && o->valuestring && o->valuestring[0]) {
                recents.append(qFromUtf8(o->valuestring));
            }
        }
    }
    cJSON* slist = cJSON_GetObjectItem(root, "sqltabs");
    if (slist && slist->type == cJSON_Array) {
        int n = cJSON_GetArraySize(slist);
        for (int i = 0; i < n; ++i) {
            cJSON* o = cJSON_GetArrayItem(slist, i);
            if (o && o->valuestring) {
                QString t = qFromUtf8(o->valuestring);
                if (sqlPageCount < SB_SQLTAB_CAP) {
                    addSqlPage(t);
                }
            }
        }
    }
    cJSON* jbt = cJSON_GetObjectItem(root, "browsetable");
    if (jbt && jbt->valuestring) {
        browseTableName = qFromUtf8(jbt->valuestring);
    }
    cJSON* jbf = cJSON_GetObjectItem(root, "browfilter");
    if (jbf && jbf->valuestring) {
        browseFilter = qFromUtf8(jbf->valuestring);
        if (filterEdit) { filterEdit->setText(browseFilter); }
    }
    cJSON_Delete(root);
}

void SqlBrowserLogic::saveState() {
    cJSON* root = cJSON_CreateObject();
    if (!root) { return; }
    cJSON* rec = cJSON_CreateArray();
    for (int i = 0; i < (int)recents.count() && i < SB_RECENT_CAP; ++i) {
        QByteArray u = sbUtf8(recents[i]);
        cJSON_AddItemToArray(rec, cJSON_CreateString(u.data()));
    }
    cJSON_AddItemToObject(root, "recent", rec);
    cJSON* slist = cJSON_CreateArray();
    if (sqlTabs) {
        for (int i = 0; i < sqlPageCount; ++i) {
            if (sqlPages[i].edit) {
                QByteArray u = sbUtf8(sbTextEdit(sqlPages[i].edit));
                cJSON_AddItemToArray(slist, cJSON_CreateString(u.data()));
            }
        }
    }
    cJSON_AddItemToObject(root, "sqltabs", slist);
    QByteArray bt = sbUtf8(browseTableName);
    QByteArray bf = sbUtf8(browseFilter);
    cJSON_AddStringToObject(root, "browsetable", bt.data());
    cJSON_AddStringToObject(root, "browfilter", bf.data());
    char* out = cJSON_Print(root);
    if (out) {
        QString text = QString::fromLatin1(out);
        sbWriteUtf8File(sbBaseDir() + "/state.json", text);
        free(out);
    }
    cJSON_Delete(root);
}

void SqlBrowserLogic::refreshRecentsCombo() {
    if (!recentsCombo) { return; }
    sbComboClear(recentsCombo);
    for (int i = 0; i < (int)recents.count(); ++i) {
        sbComboAdd(recentsCombo, recents[i]);
    }
    sbComboSetCur(recentsCombo, 0);
}

void SqlBrowserLogic::pushRecent(const QString& path) {
    QStringList nxt;
    nxt.append(path);
    for (int i = 0; i < (int)recents.count(); ++i) {
        if (recents[i] == path) { continue; }
        if ((int)nxt.count() >= SB_RECENT_CAP) { break; }
        nxt.append(recents[i]);
    }
    recents = nxt;
    refreshRecentsCombo();
    saveState();
}

/* ══════════════════════════════════════════════════
 * 通用 UI 辅助
 * ══════════════════════════════════════════════════ */

void SqlBrowserLogic::appendLog(const QString& prefix, const QString& msg) {
    if (!logView) { return; }
    QString ts = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString line = "[" + ts + "] ";
    if (!prefix.isEmpty()) { line = line + prefix + " "; }
    line += msg;
#ifdef QT3_BUILD
    logView->append(line);
#else
    logView->append(line);
#endif
}

void SqlBrowserLogic::showStatus(const QString& msg) {
    if (statusLabel) { statusLabel->setText(msg); }
    appendLog(QString(), msg);
}

bool SqlBrowserLogic::requireDb() {
    if (!session->isOpen()) {
        showStatus(qFromUtf8("未打开数据库"));
        return false;
    }
    return true;
}

bool SqlBrowserLogic::requireWritable() {
    if (!requireDb()) { return false; }
    if (session->readOnly()) {
        showStatus(qFromUtf8("数据库为只读打开"));
        return false;
    }
    return true;
}

QString SqlBrowserLogic::currentObject() const {
    if (!objList) { return QString(); }
    int ci = sbLbCurrent(objList);
    if (ci < 0 || ci >= sbLbCount(objList)) { return QString(); }
    return sbLbText(objList, ci);
}

/* ── 刷新派生视图 ── */
void SqlBrowserLogic::refreshDbDerived() {
    refreshObjectList();
    refreshBrowseTables();
    reloadPragmas();
    if (sbTabCount(sqlTabs) <= 0) {
        addSqlPage(QString());
    }
}

void SqlBrowserLogic::refreshObjectList() {
    if (!objList || !objKindCombo) { return; }
    sbLbClear(objList);
    if (!session->isOpen()) {
        return;
    }
    static const char* KINDS[3] = { "table", "index", "view" };
    int ki = sbComboCur(objKindCombo);
    if (ki < 0 || ki > 2) { ki = 0; }
    QStringList objs = session->listObjects(KINDS[ki]);
    for (int i = 0; i < (int)objs.count(); ++i) {
        sbLbAdd(objList, objs[i]);
    }
    if (ddlView) {
        sbTextEditSet(ddlView, QString());
    }
}

void SqlBrowserLogic::refreshBrowseTables() {
    if (!browseTableCombo) { return; }
    sbComboClear(browseTableCombo);
    if (!session->isOpen()) { return; }
    QStringList tables = session->listObjects("table");
    for (int i = 0; i < (int)tables.count(); ++i) {
        sbComboAdd(browseTableCombo, tables[i]);
    }
    int found = sbComboSelect(browseTableCombo, browseTableName);
    if (!found && sbComboCount(browseTableCombo) > 0) {
        browseTableName = sbComboText(browseTableCombo, 0);
        sbComboSetCur(browseTableCombo, 0);
    } else if (!found) {
        browseTableName = QString();
    }
    if (browseTableName.isEmpty()) {
        clearBrowseViews();
    } else {
        reloadBrowse(true);
    }
}

void SqlBrowserLogic::clearBrowseViews() {
    lastRows = 0;
    pageNum = 0;
    sortCol = -1;
    sortAsc = true;
    if (browseGrid) {
        sbTableSetSize(browseGrid, 0, 0, QStringList());
    }
    if (pageLabel) { pageLabel->setText(qFromUtf8("页：-")); }
    if (rowCountLabel) { rowCountLabel->setText(qFromUtf8("行数：-")); }
}

QString SqlBrowserLogic::browseTableFromCombo() const {
    if (!browseTableCombo) { return QString(); }
    if (sbComboCount(browseTableCombo) <= 0) { return QString(); }
    int ci = sbComboCur(browseTableCombo);
    if (ci < 0 || ci >= sbComboCount(browseTableCombo)) { return QString(); }
    return sbComboText(browseTableCombo, ci);
}

/* ══════════════════════════════════════════════════
 * SQL 页签管理
 * ══════════════════════════════════════════════════ */

int SqlBrowserLogic::addSqlPage(const QString& content) {
    if (!sqlTabs) { return -1; }
    if (sqlPageCount >= SB_SQLTAB_CAP || sbTabCount(sqlTabs) >= SB_SQLTAB_CAP) {
        showStatus(qFromUtf8("页签已达上限"));
        return -1;
    }
    QWidget* page = new QWidget(sqlTabs);
    QBoxLayout* lo = qNewBoxLayout(page, QBoxLayout::TopToBottom, 6, 4);
    QBoxLayout* btnRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    QPushButton* runBtn = new QPushButton(qFromUtf8("执行(F5)"), page);
    btnRow->addWidget(runBtn);
    btnRow->addStretch(1);
    lo->addLayout(btnRow);
    QTextEdit* ed = new QTextEdit(page);
    sbTextEditSetPlain(ed);
    sbTextEditSet(ed, content);
    ed->setMinimumHeight(120);
    lo->addWidget(ed, 2);
    SbTable* gt = new SbTable(page);
    sbTableSetReadOnly(gt, true);
    lo->addWidget(gt, 3);
    int idx = sbTabCount(sqlTabs);
    sbTabAdd(sqlTabs, page, qFromUtf8("SQL-") + QString::number(idx + 1));
    sbTabSetCurrent(sqlTabs, idx);
    SqlPage& pg = sqlPages[sqlPageCount];
    pg.page = page;
    pg.edit = ed;
    pg.grid = gt;
    pg.runBtn = runBtn;
    int pageNumRet = sqlPageCount;
    ++sqlPageCount;
    QObject::connect(runBtn, SIGNAL(clicked()), this, SLOT(onRunSql()));
    return pageNumRet;
}

void SqlBrowserLogic::setActiveSqlPage(int idx) {
    if (!sqlTabs) { return; }
    if (idx < 0 || idx >= sbTabCount(sqlTabs)) { return; }
    sbTabSetCurrent(sqlTabs, idx);
}

int SqlBrowserLogic::activeSqlIndex() const {
    if (!sqlTabs) { return -1; }
    return sbTabIndex(sqlTabs);
}

SqlBrowserLogic::SqlPage* SqlBrowserLogic::activeSqlPage() {
    if (!sqlTabs) { return 0; }
    int idx = sbTabIndex(sqlTabs);
    if (idx < 0) { return 0; }
    /* 页签页与 sqlPages[] 同序创建/移除 */
    for (int i = 0; i < sqlPageCount; ++i) {
        if (sqlPages[i].page == sbTabPage(sqlTabs, idx)) {
            return &sqlPages[i];
        }
    }
    return 0;
}

/* ══════════════════════════════════════════════════
 * 浏览数据实现
 * ══════════════════════════════════════════════════ */

void SqlBrowserLogic::updatePageLabel() {
    if (!pageLabel) { return; }
    int totalPages = lastRows > 0 ? (lastRows + SB_PAGE_SIZE - 1) / SB_PAGE_SIZE : 0;
    QString txt = qFromUtf8("第 ");
    txt += QString::number(pageNum + 1);
    txt += qFromUtf8(" / ");
    txt += QString::number(totalPages > 0 ? totalPages : 1);
    txt += qFromUtf8(" 页");
    pageLabel->setText(txt);
    if (rowCountLabel) {
        rowCountLabel->setText(qFromUtf8("行数：") +
                               QString::number(lastRows));
    }
}

int SqlBrowserLogic::currentPageNum() const { return pageNum; }

void SqlBrowserLogic::setPageNum(int n) {
    if (n < 0) { n = 0; }
    int totalPages = lastRows > 0 ? (lastRows + SB_PAGE_SIZE - 1) / SB_PAGE_SIZE : 1;
    if (totalPages <= 0) { totalPages = 1; }
    if (n > totalPages - 1) { n = totalPages - 1; }
    pageNum = n;
}

void SqlBrowserLogic::reloadBrowse(bool resetOffset) {
    if (!browseGrid || !session->isOpen() || browseTableName.isEmpty()) {
        clearBrowseViews();
        return;
    }
    if (resetOffset) { pageNum = 0; }
    sortAsc = sortCol >= 0 ? sortAsc : true;
    QString err;
    bool hasRowid = true;
    if (!session->checkRowidSupport(browseTableName, hasRowid, err)) {
        hasRowid = false;
    }
    browseHasRowid = hasRowid;
    QString where = browseFilter;
    QString orderBy;
    if (sortCol >= 0 && sortCol < (int)browseHeaders.count()) {
        orderBy = sbSqlIdent(browseHeaders[sortCol]);
        orderBy += sortAsc ? " ASC" : " DESC";
    }
    int rows = session->tableRows(browseTableName, where, err);
    if (rows < 0) {
        sbShowMsg(uiRoot, qFromUtf8("浏览数据"), err);
        lastRows = 0;
        clearBrowseViews();
        updatePageLabel();
        return;
    }
    lastRows = rows;
    SbBrowsePage pg;
    pg.hasRowid = hasRowid;
    if (!session->fetchPage(browseTableName, (sb_int64)pageNum * SB_PAGE_SIZE,
                            SB_PAGE_SIZE, orderBy, where, pg, err)) {
        sbShowMsg(uiRoot, qFromUtf8("浏览数据"), err);
        clearBrowseViews();
        updatePageLabel();
        return;
    }
    browseHeaders = pg.headers;
    int cols = (int)pg.headers.count() + (hasRowid ? 1 : 0);
    QStringList headers;
    if (hasRowid) { headers.append(QString::fromLatin1("rowid")); }
    for (int i = 0; i < (int)pg.headers.count(); ++i) {
        headers.append(pg.headers[i]);
    }
    sbTableSetSize(browseGrid, (int)pg.cells.size(), cols, headers);
    for (int r = 0; r < (int)pg.cells.size(); ++r) {
        if (hasRowid) {
            sbTableSetText(browseGrid, r, 0,
                           QString::number((long long)pg.rowids[r]));
        }
        for (int c = 0; c < (int)pg.cells[r].count(); ++c) {
            int tc = c + (hasRowid ? 1 : 0);
            sbTableSetText(browseGrid, r, tc, pg.cells[r][c]);
        }
    }
    sbTableFitCols(browseGrid);
    if (deleteRowBtn) {
        deleteRowBtn->setEnabled((bool)hasRowid && !session->readOnly());
    }
    if (insertRowBtn) {
        insertRowBtn->setEnabled((bool)!session->readOnly());
    }
    updatePageLabel();
}

/* ══════════════════════════════════════════════════
 * PRAGMA
 * ══════════════════════════════════════════════════ */

void SqlBrowserLogic::reloadPragmas() {
    if (!session->isOpen()) {
        if (pageSizeEdit) { pageSizeEdit->setText(QString()); }
        if (userVersionEdit) { userVersionEdit->setText(QString()); }
        return;
    }
    QString val;
    if (pageSizeEdit && session->pragmaText("page_size", val)) {
        pageSizeEdit->setText(val);
    }
    if (journalModeCombo && session->pragmaText("journal_mode", val)) {
        sbComboSelect(journalModeCombo, sbTrim(val));
    }
    if (autoVacuumCombo && session->pragmaText("auto_vacuum", val)) {
        sbComboSelect(autoVacuumCombo, val == "full" ? "1" :
                      (val == "incremental" ? "2" : "0"));
    }
    if (foreignKeysCombo && session->pragmaText("foreign_keys", val)) {
        sbComboSelect(foreignKeysCombo, val == "1" || val == "true" ? "1" : "0");
    }
    if (userVersionEdit && session->pragmaText("user_version", val)) {
        userVersionEdit->setText(val);
    }
}

/* ══════════════════════════════════════════════════
 * 库文件槽
 * ══════════════════════════════════════════════════ */

void SqlBrowserLogic::openByPath(const QString& path, bool readOnly) {
    QString err;
    if (!session->open(path, readOnly, err)) {
        sbShowMsg(uiRoot, qFromUtf8("打开"), err);
        showStatus(qFromUtf8("打开失败：") + path);
        return;
    }
    appendLog("打开", path);
    showStatus(qFromUtf8("打开：") + path);
    if (dbLabel) {
        dbLabel->setText(qFromUtf8("库：") + path);
    }
    refreshDbDerived();
    if (!readOnly && path != QString::fromLatin1(":memory:")) {
        pushRecent(path);
    }
}

void SqlBrowserLogic::onOpenDb() {
    if (!openBtn) { return; }
    QString filter = qFromUtf8(
        "SQLite 数据库 (*.db *.db3 *.sqlite *.sqlite3);;所有文件 (*)");
    QString path = sbFileDialogOpen(uiRoot, filter);
    if (path.isEmpty()) { return; }
    openByPath(path, roCheck ? roCheck->isChecked() : false);
}

void SqlBrowserLogic::onNewDb() {
    if (!newBtn) { return; }
    QString filter = qFromUtf8(
        "SQLite 数据库 (*.db *.db3 *.sqlite *.sqlite3);;所有文件 (*)");
    QString path = sbFileDialogSave(uiRoot, filter);
    if (path.isEmpty()) { return; }
    QString err;
    if (!DbSession::createEmptyFile(path, err)) {
        sbShowMsg(uiRoot, qFromUtf8("新建"), err);
        return;
    }
    appendLog("新建", path);
    openByPath(path, false);
}

void SqlBrowserLogic::onMemoryDb() {
    QString err;
    if (!session->open(QString::fromLatin1(":memory:"), false, err)) {
        sbShowMsg(uiRoot, qFromUtf8("内存库"), err);
        return;
    }
    showStatus(qFromUtf8("打开：内存库(:memory:)"));
    if (dbLabel) { dbLabel->setText(qFromUtf8("库：内存库")); }
    refreshDbDerived();
}

void SqlBrowserLogic::onCloseDb() {
    if (!session->isOpen()) { return; }
    saveState();
    session->close();
    browseTableName = QString();
    browseFilter = QString();
    if (dbLabel) { dbLabel->setText(qFromUtf8("库：未打开")); }
    refreshObjectList();
    clearBrowseViews();
    reloadPragmas();
    showStatus(qFromUtf8("已关闭"));
}

void SqlBrowserLogic::onVacuum() {
    if (!requireWritable()) { return; }
    QString err;
    if (!session->vacuum(err)) {
        sbShowMsg(uiRoot, qFromUtf8("VACUUM"), err);
        return;
    }
    appendLog("VACUUM", session->dbPath());
    showStatus(qFromUtf8("压缩完成"));
    reloadBrowse(false);
}

void SqlBrowserLogic::onRecentPicked(int idx) {
    if (!recentsCombo) { return; }
    if (idx < 0 || idx >= (int)recents.count()) { return; }
    openByPath(recents[idx], roCheck ? roCheck->isChecked() : false);
}

void SqlBrowserLogic::onRoToggled(bool checked) {
    if (session->isOpen()) {
        showStatus(checked ? qFromUtf8("只读模式（重新打开后生效）")
                           : qFromUtf8("读写模式（重新打开后生效）"));
    }
}

/* ══════════════════════════════════════════════════
 * 对象/结构槽
 * ══════════════════════════════════════════════════ */

void SqlBrowserLogic::onKindChanged(int) {
    refreshObjectList();
}

void SqlBrowserLogic::onRefreshObjects() {
    refreshObjectList();
}

void SqlBrowserLogic::onObjectSelected() {
    if (!requireDb()) { return; }
    QString obj = currentObject();
    if (obj.isEmpty()) { return; }
    QString ddl = session->objectDdl(obj);
    if (ddlView) {
        sbTextEditSet(ddlView, ddl.isEmpty()
            ? qFromUtf8("(无源码：内建对象)") : ddl);
    }
}

void SqlBrowserLogic::onCreateTable() {
    if (!requireWritable()) { return; }
    QString sql;
    if (!sbCreateTableDialog(uiRoot, sql)) { return; }
    QString err;
    if (!session->runSql(sql, err, 0)) {
        sbShowMsg(uiRoot, qFromUtf8("建表"), err);
        return;
    }
    appendLog("DDL", sql);
    showStatus(qFromUtf8("建表完成"));
    refreshDbDerived();
}

void SqlBrowserLogic::onEditTable() {
    if (!requireWritable()) { return; }
    QString obj = currentObject();
    if (obj.isEmpty()) {
        showStatus(qFromUtf8("请先在左侧选择表"));
        return;
    }
    if (session->objectType(obj) != QString::fromLatin1("table")) {
        showStatus(qFromUtf8("改表需要选中一个表"));
        return;
    }
    int op = SB_ALTER_ADDCOL;
    QString f1;
    QString f2;
    if (!sbAlterTableDialog(uiRoot, obj, op, f1, f2)) { return; }
    QString sql;
    bool ok = true;
    switch (op) {
    case SB_ALTER_ADDCOL: {
        QString def;
        QString err;
        if (!sbParseColumnSpec(f1, def, err)) {
            sbShowMsg(uiRoot, qFromUtf8("改表"), err);
            return;
        }
        sql = "ALTER TABLE " + sbSqlIdent(obj) + " ADD COLUMN " + def;
        break;
    }
    case SB_ALTER_RENAMETABLE:
        if (f1.isEmpty()) {
            sbShowMsg(uiRoot, qFromUtf8("改表"), qFromUtf8("参数1 不能为空"));
            return;
        }
        sql = "ALTER TABLE " + sbSqlIdent(obj) + " RENAME TO " +
              sbSqlIdent(f1);
        break;
    case SB_ALTER_RENAMECOL:
        if (f1.isEmpty() || f2.isEmpty()) {
            sbShowMsg(uiRoot, qFromUtf8("改表"),
                      qFromUtf8("重命名列需要参数1/参数2"));
            return;
        }
        sql = "ALTER TABLE " + sbSqlIdent(obj) + " RENAME COLUMN " +
              sbSqlIdent(f1) + " TO " + sbSqlIdent(f2);
        break;
    case SB_ALTER_DROPCOL:
        if (f1.isEmpty()) {
            sbShowMsg(uiRoot, qFromUtf8("改表"), qFromUtf8("参数1 不能为空"));
            return;
        }
        sql = "ALTER TABLE " + sbSqlIdent(obj) + " DROP COLUMN " +
              sbSqlIdent(f1);
        break;
    default:
        ok = false;
        break;
    }
    if (!ok) { return; }
    QString err;
    if (!session->runSql(sql, err, 0)) {
        sbShowMsg(uiRoot, qFromUtf8("改表"), err);
        return;
    }
    appendLog("DDL", sql);
    showStatus(qFromUtf8("改表完成"));
    refreshDbDerived();
}

void SqlBrowserLogic::onDropTable() {
    if (!requireWritable()) { return; }
    QString obj = currentObject();
    if (obj.isEmpty()) {
        showStatus(qFromUtf8("请先在左侧选择表"));
        return;
    }
    if (session->objectType(obj) != QString::fromLatin1("table")) {
        showStatus(qFromUtf8("删表需要选中一个表"));
        return;
    }
    if (!sbAskYesNo(uiRoot, qFromUtf8("删表"),
                    qFromUtf8("删除表 ") + obj + qFromUtf8(" 及其数据？"))) {
        return;
    }
    QString sql = "DROP TABLE " + sbSqlIdent(obj);
    QString err;
    if (!session->runSql(sql, err, 0)) {
        sbShowMsg(uiRoot, qFromUtf8("删表"), err);
        return;
    }
    appendLog("DDL", sql);
    if (browseTableName == obj) {
        browseTableName = QString();
    }
    showStatus(qFromUtf8("已删除表：") + obj);
    refreshDbDerived();
}

void SqlBrowserLogic::onCreateIndex() {
    if (!requireWritable()) { return; }
    QString obj = currentObject();
    QString sql;
    if (!sbCreateIndexDialog(uiRoot, session, obj, sql)) { return; }
    QString err;
    if (!session->runSql(sql, err, 0)) {
        sbShowMsg(uiRoot, qFromUtf8("建索引"), err);
        return;
    }
    appendLog("DDL", sql);
    showStatus(qFromUtf8("建索引完成"));
    refreshDbDerived();
}

void SqlBrowserLogic::onDropIndex() {
    if (!requireWritable()) { return; }
    QString obj = currentObject();
    if (obj.isEmpty()) {
        showStatus(qFromUtf8("请先在左侧选择索引"));
        return;
    }
    if (session->objectType(obj) != QString::fromLatin1("index")) {
        showStatus(qFromUtf8("删索引需要选中一个索引"));
        return;
    }
    if (!sbAskYesNo(uiRoot, qFromUtf8("删索引"),
                    qFromUtf8("删除索引 ") + obj + "？")) {
        return;
    }
    QString sql = "DROP INDEX " + sbSqlIdent(obj);
    QString err;
    if (!session->runSql(sql, err, 0)) {
        sbShowMsg(uiRoot, qFromUtf8("删索引"), err);
        return;
    }
    appendLog("DDL", sql);
    showStatus(qFromUtf8("已删除索引：") + obj);
    refreshDbDerived();
}

/* ══════════════════════════════════════════════════
 * 浏览数据槽
 * ══════════════════════════════════════════════════ */

void SqlBrowserLogic::onBrowseTableChanged(int idx) {
    if (!browseTableCombo || !browseGrid) { return; }
    QString t = browseTableFromCombo();
    if (t.isEmpty()) {
        browseTableName = QString();
        clearBrowseViews();
        return;
    }
    browseTableName = t;
    sortCol = -1;
    sortAsc = true;
    reloadBrowse(true);
    saveState();
}

void SqlBrowserLogic::onFilterGo() {
    if (!filterEdit) { return; }
    QString f = sbTrim(filterEdit->text());
    browseFilter = f;
    sortCol = -1;
    reloadBrowse(true);
    saveState();
}

void SqlBrowserLogic::onPagePrev() {
    setPageNum(pageNum - 1);
    reloadBrowse(false);
}

void SqlBrowserLogic::onPageNext() {
    setPageNum(pageNum + 1);
    reloadBrowse(false);
}

void SqlBrowserLogic::onInsertRow() {
    if (!requireWritable()) { return; }
    if (browseTableName.isEmpty()) {
        showStatus(qFromUtf8("请先选择要浏览的表"));
        return;
    }
    QString err;
    if (!session->insertDefaultRow(browseTableName, err)) {
        sbShowMsg(uiRoot, qFromUtf8("加行"), err);
        return;
    }
    QString w = browseFilter;
    int rows = session->tableRows(browseTableName, w, err);
    int lastPage = rows > 0 ? (rows - 1) / SB_PAGE_SIZE : 0;
    pageNum = lastPage;
    reloadBrowse(false);
    showStatus(qFromUtf8("已加行（最后页）"));
}

void SqlBrowserLogic::onDeleteRow() {
    if (!requireWritable()) { return; }
    if (browseTableName.isEmpty()) {
        showStatus(qFromUtf8("请先选择要浏览的表"));
        return;
    }
    if (!browseHasRowid) {
        showStatus(qFromUtf8("该表（WITHOUT ROWID）不支持按 rowid 删行"));
        return;
    }
    int cur = browseGrid ? browseGrid->currentRow() : -1;
    if (cur < 0) {
        showStatus(qFromUtf8("请先选中一行"));
        return;
    }
    QString sel = sbTableText(browseGrid, cur, 0);
    bool isNum = false;
    long long rid = sel.toLongLong(&isNum);
    if (!isNum) {
        showStatus(qFromUtf8("行未选中或 rowid 无效"));
        return;
    }
    if (!sbAskYesNo(uiRoot, qFromUtf8("删行"),
                    qFromUtf8("删除 rowid=") + sel +
                    qFromUtf8(" 这一行？"))) {
        return;
    }
    QString err;
    if (!session->deleteRowByRowid(browseTableName, rid, err)) {
        sbShowMsg(uiRoot, qFromUtf8("删行"), err);
        return;
    }
    reloadBrowse(false);
    showStatus(qFromUtf8("已删除 rowid=") + sel);
}

void SqlBrowserLogic::onCellDouble(int r, int c) {
    if (!requireDb()) { return; }
    if (session->readOnly()) {
        showStatus(qFromUtf8("数据库为只读打开"));
        return;
    }
    if (browseTableName.isEmpty() || !browseHasRowid) { return; }
    if (r < 0 || c <= 0) { return; }
    int dataCol = c - 1;
    if (dataCol < 0 || dataCol >= (int)browseHeaders.count()) { return; }
    QString colName = browseHeaders[dataCol];
    QString shown = sbTableText(browseGrid, r, c);
    QString sel = sbTableText(browseGrid, r, 0);
    bool isNum = false;
    long long rid = sel.toLongLong(&isNum);
    if (!isNum) { return; }
    bool isNull = shown == QString::fromLatin1("(NULL)");
    bool isBlob = sbFind(shown, "X'") == 0;
    QString literal;
    QString err;
    if (!session->cellLiteral(browseTableName, rid, colName, literal, err)) {
        sbShowMsg(uiRoot, qFromUtf8("编辑"), err);
        return;
    }
    QString curText = literal;
    if (curText == QString::fromLatin1("NULL")) { curText = QString(); }
    QString newText;
    bool toNull = false;
    if (!sbEditCellDialog(uiRoot, colName, QString(), curText, isBlob,
                          newText, toNull)) {
        return;
    }
    if (toNull) {
        literal = QString::fromLatin1("NULL");
    } else if (isBlob) {
        QString cand = sbTrim(newText);
        if (cand.isEmpty()) {
            literal = QString::fromLatin1("NULL");
        } else if (sbFind(cand, "X'") == 0) {
            literal = cand;
        } else {
            QByteArray u = sbUtf8(cand);
            literal = sbSqlBlobLiteralHex((const unsigned char*)u.data(),
                                          (int)u.size());
        }
    } else {
        QString cand = newText;
        QByteArray blob;
        if (DbSession::parseHexLiteral(cand, blob)) {
            literal = sbSqlBlobLiteralHex((const unsigned char*)blob.data(),
                                          (int)blob.size());
        } else {
            literal = sbSqlEscapeText(cand);
        }
    }
    if (!session->updateCell(browseTableName, rid, colName, literal, err)) {
        sbShowMsg(uiRoot, qFromUtf8("编辑"), err);
        return;
    }
    appendLog("UPDATE", qFromUtf8("UPDATE ") + browseTableName + "." +
              colName + " (rowid=" + sel + ")");
    reloadBrowse(false);
}

void SqlBrowserLogic::onSortByCol(int col) {
    if (!browseGrid) { return; }
    if (browseTableName.isEmpty()) { return; }
    if (col < 0 || col >= (int)browseHeaders.count()) {
        return;
    }
    if (col == sortCol) {
        sortAsc = !sortAsc;
    } else {
        sortCol = col;
        sortAsc = true;
    }
    reloadBrowse(true);
}

/* ══════════════════════════════════════════════════
 * SQL 编辑与执行槽
 * ══════════════════════════════════════════════════ */

void SqlBrowserLogic::onRunSql() {
    SqlPage* pg = activeSqlPage();
    if (!pg || !pg->edit || !pg->grid) {
        showStatus(qFromUtf8("没有活动的 SQL 页"));
        return;
    }
    if (!requireDb()) { return; }
    QString sqlText = sbTextEdit(pg->edit);
    if (sbTrim(sqlText).isEmpty()) {
        showStatus(qFromUtf8("SQL 为空"));
        return;
    }
    SbGrid grid;
    QString err;
    sb_int64 changes = 0;
    if (!session->execQuery(sqlText, grid, err, &changes)) {
        sbTableSetSize(pg->grid, 0, 0, QStringList());
        if (sqlErrLabel) {
            sqlErrLabel->setText(qFromUtf8("SQL 错误：") + err);
        }
        appendLog("SQL错误", err);
        return;
    }
    sbTableSetSize(pg->grid, (int)grid.rows.size(),
                   (int)grid.headers.count(), grid.headers);
    for (int r = 0; r < (int)grid.rows.size(); ++r) {
        for (int c = 0; c < (int)grid.rows[r].size(); ++c) {
            sbTableSetText(pg->grid, r, c, grid.rows[r][c].text);
        }
    }
    sbTableFitCols(pg->grid);
    QString msg;
    if ((int)grid.rows.size() > 0) {
        msg = qFromUtf8("结果 ") + QString::number((int)grid.rows.size()) +
              qFromUtf8(" 行");
        if (changes > 0) {
            msg += qFromUtf8("，另有改动 ") + QString::number((long long)changes) +
                   qFromUtf8(" 行");
        }
    } else if (changes > 0) {
        msg = qFromUtf8("已执行，改动 ") + QString::number((long long)changes) +
              qFromUtf8(" 行");
    } else {
        msg = qFromUtf8("已执行无结果集");
    }
    if (sqlErrLabel) { sqlErrLabel->setText("SQL：" + msg); }
    appendLog("SQL", err.isEmpty() ? sqlText : sqlText);
    showStatus(msg);
    saveState();
}

void SqlBrowserLogic::onAddSqlTab() {
    addSqlPage(QString());
}

void SqlBrowserLogic::onCloseSqlTab() {
    if (!sqlTabs) { return; }
    if (sbTabCount(sqlTabs) <= 1) {
        showStatus(qFromUtf8("至少保留一个 SQL 页"));
        return;
    }
    int idx = sbTabIndex(sqlTabs);
    if (idx < 0) { return; }
    SqlPage* pg = 0;
    int foundPos = -1;
    for (int i = 0; i < sqlPageCount; ++i) {
        if (sbTabPage(sqlTabs, idx) == sqlPages[i].page) {
            pg = &sqlPages[i];
            foundPos = i;
            break;
        }
    }
    if (!pg) { return; }
    sbTabRemove(sqlTabs, idx);
    delete pg->page;
    for (int i = foundPos; i < sqlPageCount - 1; ++i) {
        sqlPages[i] = sqlPages[i + 1];
    }
    --sqlPageCount;
    sqlPages[sqlPageCount] = SqlPage();
    showStatus(qFromUtf8("已关闭 SQL 页"));
    saveState();
}

void SqlBrowserLogic::onOpenSqlFile() {
    SqlPage* pg = activeSqlPage();
    if (!pg || !pg->edit || !requireDb()) { return; }
    QString filter = qFromUtf8("SQL 文件 (*.sql *.ddl *.txt);;所有文件 (*)");
    QString path = sbFileDialogOpen(uiRoot, filter);
    if (path.isEmpty()) { return; }
    QString data = sbReadUtf8File(path);
    if (data.isEmpty()) {
        sbShowMsg(uiRoot, qFromUtf8("载入SQL"), qFromUtf8("文件为空或无法读取"));
        return;
    }
    sbTextEditSet(pg->edit, data);
    showStatus(qFromUtf8("已载入 SQL 文件：") + path);
}

void SqlBrowserLogic::onSaveSqlFile() {
    SqlPage* pg = activeSqlPage();
    if (!pg || !pg->edit) { return; }
    QString filter = qFromUtf8("SQL 文件 (*.sql *.ddl *.txt);;所有文件 (*)");
    QString path = sbFileDialogSave(uiRoot, filter);
    if (path.isEmpty()) { return; }
    QString text = sbTextEdit(pg->edit);
    if (!sbWriteUtf8File(path, text)) {
        sbShowMsg(uiRoot, qFromUtf8("保存SQL"), qFromUtf8("无法写入文件"));
        return;
    }
    showStatus(qFromUtf8("已保存 SQL 文件：") + path);
}

/* ══════════════════════════════════════════════════
 * 导入导出槽
 * ══════════════════════════════════════════════════ */

void SqlBrowserLogic::onImportCsv() {
    if (!requireWritable()) { return; }
    QString table;
    bool header = true;
    if (!sbPickTableDialog(uiRoot, session, true, table, header)) { return; }
    QString filter = qFromUtf8("CSV 文件 (*.csv *.txt);;所有文件 (*)");
    QString path = sbFileDialogOpen(uiRoot, filter);
    if (path.isEmpty()) { return; }
    QString err;
    sb_int64 ins = 0;
    if (!session->importCsvInto(table, path, err, header, &ins)) {
        sbShowMsg(uiRoot, qFromUtf8("CSV 导入"), err);
        return;
    }
    appendLog("CSV导入", qFromUtf8("表 ") + table + qFromUtf8(" ← ") + path +
              qFromUtf8("（") + QString::number((long long)ins) +
              qFromUtf8(" 行）"));
    showStatus(qFromUtf8("CSV 导入完成：") + QString::number((long long)ins) +
               qFromUtf8(" 行"));
    reloadBrowse(true);
}

void SqlBrowserLogic::onExportCsv() {
    if (!requireDb()) { return; }
    QString table;
    bool header = false;
    if (!sbPickTableDialog(uiRoot, session, false, table, header)) { return; }
    QString filter = qFromUtf8("CSV 文件 (*.csv *.txt);;所有文件 (*)");
    QString path = sbFileDialogSave(uiRoot, filter);
    if (path.isEmpty()) { return; }
    QString err;
    if (!session->exportTableCsv(table, path, err, true)) {
        sbShowMsg(uiRoot, qFromUtf8("CSV 导出"), err);
        return;
    }
    appendLog("CSV导出", qFromUtf8("表 ") + table + qFromUtf8(" → ") + path);
    showStatus(qFromUtf8("CSV 导出完成"));
}

void SqlBrowserLogic::onExportDump() {
    if (!requireDb()) { return; }
    QString filter = qFromUtf8("SQL 文件 (*.sql *.ddl *.txt);;所有文件 (*)");
    QString path = sbFileDialogSave(uiRoot, filter);
    if (path.isEmpty()) { return; }
    QString err;
    if (!session->exportDump(path, err)) {
        sbShowMsg(uiRoot, qFromUtf8("SQL 导出"), err);
        return;
    }
    appendLog("SQL导出", session->dbPath() + " → " + path);
    showStatus(qFromUtf8("SQL 导出完成：") + path);
}

void SqlBrowserLogic::onImportDump() {
    if (!requireWritable()) { return; }
    QString filter = qFromUtf8("SQL 文件 (*.sql *.ddl *.txt);;所有文件 (*)");
    QString path = sbFileDialogOpen(uiRoot, filter);
    if (path.isEmpty()) { return; }
    if (!sbAskYesNo(uiRoot, qFromUtf8("SQL 导入"),
                    qFromUtf8("执行此 SQL 文件可能修改/清空现有数据，继续？"))) {
        return;
    }
    QString err;
    if (!session->importDumpFile(path, err)) {
        sbShowMsg(uiRoot, qFromUtf8("SQL 导入"), err);
        return;
    }
    appendLog("SQL导入", path);
    showStatus(qFromUtf8("SQL 导入完成"));
    refreshDbDerived();
}

/* ══════════════════════════════════════════════════
 * PRAGMA 槽与日志
 * ══════════════════════════════════════════════════ */

void SqlBrowserLogic::onPragmaRefresh() {
    if (!requireDb()) { return; }
    reloadPragmas();
    showStatus(qFromUtf8("PRAGMA 已刷新"));
}

void SqlBrowserLogic::onPragmaApply() {
    if (!requireWritable()) { return; }
    QString err;
    QString val;
    bool any = false;
    for (int round = 0; round < 5; ++round) {
        switch (round) {
        case 0: {
            val = sbTrim(pageSizeEdit ? pageSizeEdit->text() : QString());
            if (!val.isEmpty() && val.toInt() > 0) {
                if (session->pragmaSet("page_size", val, err)) {
                    appendLog("PRAGMA", "page_size = " + val);
                    any = true;
                }
            }
            break;
        }
        case 1: {
            if (journalModeCombo && sbComboCount(journalModeCombo) > 0) {
                val = sbComboText(journalModeCombo, sbComboCur(journalModeCombo));
                if (session->pragmaSet("journal_mode", val, err)) {
                    appendLog("PRAGMA", "journal_mode = " + val);
                    any = true;
                }
            }
            break;
        }
        case 2: {
            if (autoVacuumCombo && sbComboCount(autoVacuumCombo) > 0) {
                val = "0";
                QString av = sbComboText(autoVacuumCombo,
                                         sbComboCur(autoVacuumCombo));
                if (av == "1" || av == "2") { val = av; }
                if (session->pragmaSet("auto_vacuum", val, err)) {
                    appendLog("PRAGMA", "auto_vacuum = " + val);
                    any = true;
                }
            }
            break;
        }
        case 3: {
            if (foreignKeysCombo && sbComboCount(foreignKeysCombo) > 0) {
                val = sbComboText(foreignKeysCombo,
                                  sbComboCur(foreignKeysCombo));
                if (val == "1" || val == "0") {
                    if (session->pragmaSet("foreign_keys", val, err)) {
                        appendLog("PRAGMA", "foreign_keys = " + val);
                        any = true;
                    }
                }
            }
            break;
        }
        case 4: {
            val = sbTrim(userVersionEdit ? userVersionEdit->text() : QString());
            if (!val.isEmpty() && val.toInt() >= 0 &&
                !session->pragmaSet("user_version", val, err)) {
                sbShowMsg(uiRoot, qFromUtf8("PRAGMA"),
                          qFromUtf8("user_version 设置失败：") + err);
            }
            break;
        }
        }
    }
    reloadPragmas();
    showStatus(any ? qFromUtf8("PRAGMA 已应用") : qFromUtf8("PRAGMA 无更改"));
}

void SqlBrowserLogic::onQuickCheck() {
    if (!requireDb()) { return; }
    QString out;
    QString err;
    if (!session->pragmaText("quick_check", out)) {
        sbShowMsg(uiRoot, qFromUtf8("quick_check"),
                  qFromUtf8("执行失败：") + err);
        return;
    }
    if (pragmaResultView) {
        sbTextEditSet(pragmaResultView, out);
    }
    appendLog("PRAGMA", "quick_check => " + out);
}

void SqlBrowserLogic::onIntegrityCheck() {
    if (!requireDb()) { return; }
    QString out;
    QString err;
    if (!session->pragmaText("integrity_check", out)) {
        sbShowMsg(uiRoot, qFromUtf8("integrity_check"),
                  qFromUtf8("执行失败：") + err);
        return;
    }
    if (pragmaResultView) {
        sbTextEditSet(pragmaResultView, out);
    }
    appendLog("PRAGMA", "integrity_check => " + out);
}

void SqlBrowserLogic::onClearLog() {
    if (logView) {
        sbTextEditSet(logView, QString());
    }
}

/* ══════════════════════════════════════════════════
 * 主窗工厂与 exe 命令行辅助
 * ══════════════════════════════════════════════════ */

static SqlBrowserLogic* sBrowserLogic = 0;

extern "C"
void sbBrowserOpenEarly(const QString& path) {
    if (!sBrowserLogic) { return; }
    sBrowserLogic->openByPath(path, false);
}

void sbSetBrowserLogicPtr(SqlBrowserLogic* p) {
    sBrowserLogic = p;
}
