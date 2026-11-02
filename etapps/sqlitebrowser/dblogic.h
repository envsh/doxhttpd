#ifndef SB_DBLOGIC_H
#define SB_DBLOGIC_H

/* SQLite Browser（DB4S 移植）— 数据层 + 上层 UI 数据
 * Qt3/Qt4 双构建：表格/列表/下拉通过 typedef 对齐
 */

#include "compat34.h"

#include <qstring.h>
#include <qstringlist.h>
#include <qobject.h>
#include <sqlite3.h>

/* 容器兼容：Qt3 的 QList/QVector 是指针容器，
 * 值容器 Qt3 = QValueList/QValueVector，Qt4 = QList/QVector */
#ifdef QT3_BUILD
#include <qvaluevector.h>
#else
#include <qlist.h>
#include <qvector.h>
#endif

#ifdef QT3_BUILD
#include <qtable.h>
typedef QTable SbTable;
#include <qlistbox.h>
typedef QListBox SbListBox;
#else
#include <qtablewidget.h>
typedef QTableWidget SbTable;
#include <qlistwidget.h>
typedef QListWidget SbListBox;
#endif

class QWidget;
class QComboBox;
class QTextEdit;
class QCheckBox;
class QLabel;
class QPushButton;
class QLineEdit;
class QTabWidget;
class QListWidgetItem;
class QTableWidgetItem;

/* 64 位整数：Qt3 无 qlonglong，统一用本仓 typedef */
typedef long long sb_int64;

#define SB_BASE_DIR_NAME      "sqlitebrowser"
#define SB_RECENT_CAP         10
#define SB_SQLTAB_CAP         8
#define SB_PAGE_SIZE          50      /* 浏览数据每页行数 */
#define SB_GRID_MAX_ROWS      10000   /* 任意查询结果抓取上限 */

/* ── 通用兼容工具（mainwin/dblogic 共用） ── */
/* ASCII/Latin1 单字符取值（Qt3 QChar::latin1 / Qt4 QChar::toLatin1 兼容，
 * 本项目 trimmed Qt4 无 latin1()，统一走 cell()） */
inline char sbCharAt(const QString& s, int i) {
    return (char)(unsigned char)s.at(i).cell();
}
QString sbTrim(const QString& s);
QByteArray sbUtf8(const QString& s);
QString sbReadUtf8File(const QString& path);
bool     sbWriteUtf8File(const QString& path, const QString& content);
void     sbAppendBytes(QByteArray& dst, const QByteArray& src);
QString  sbFileDialogOpen(QWidget* parent, const QString& filter);
QString  sbFileDialogSave(QWidget* parent, const QString& filter);

/* QTextEdit 兼容（Qt3 text / Qt4 toPlainText） */
QString sbTextEdit(const QTextEdit* e);
void    sbTextEditSet(QTextEdit* e, const QString& s);
void    sbTextEditSetPlain(QTextEdit* e);

/* QComboBox 兼容 */
void    sbComboAdd(QComboBox* b, const QString& s);
void    sbComboClear(QComboBox* b);
int     sbComboCount(const QComboBox* b);
QString sbComboText(const QComboBox* b, int i);
void    sbComboSetCur(QComboBox* b, int i);
int     sbComboCur(const QComboBox* b);
bool    sbComboSelect(QComboBox* b, const QString& t);

/* ListBox 兼容 */
void    sbLbAdd(SbListBox* lb, const QString& s);
void    sbLbClear(SbListBox* lb);
int     sbLbCount(const SbListBox* lb);
int     sbLbCurrent(const SbListBox* lb);
QString sbLbText(const SbListBox* lb, int i);

/* 表格兼容（Qt3 QTable / Qt4 QTableWidget） */
void    sbTableSetSize(SbTable* t, int rows, int cols, const QStringList& headers);
void    sbTableSetText(SbTable* t, int r, int c, const QString& s);
QString sbTableText(const SbTable* t, int r, int c);
void    sbTableSetReadOnly(SbTable* t, bool ro);
void    sbTableFitCols(SbTable* t);

/* QTabWidget 兼容（Qt3 currentPage/page(i)/removePage 与 Qt4 差异） */
void    sbTabAdd(QTabWidget* w, QWidget* page, const QString& label);
int     sbTabCount(const QTabWidget* w);
int     sbTabIndex(const QTabWidget* w);
void    sbTabSetCurrent(QTabWidget* w, int i);
QWidget* sbTabPage(QTabWidget* w, int i);
void    sbTabRemove(QTabWidget* w, int i);

/* SQL 值转义：text → 'a''b'；blob 十六进制字面量 → X'AB..'
 * 标识符（表名/列名）双引号包裹："a""b" */
QString sbSqlEscapeText(const QString& s);
QString sbSqlBlobLiteralHex(const unsigned char* data, int len);
QString sbSqlIdent(const QString& name);

/* 基础目录：~/.config/sqlitebrowser */
QString sbBaseDir();
void    sbEnsureDirs();

/* ── 数据结构 ── */

struct SbCell {
    QString text;   /* blob: "X'..' (N字节)" 预览 */
    int     kind;   /* 0=普通文本 1=blob 2=NULL */
    SbCell() : kind(0) {}
};

struct SbColumn {
    QString name;
    QString type;
    bool    pk;
    bool    notnull;
    bool    unique;
    QString dflt;   /* 空串=无默认值 */
    SbColumn() : pk(false), notnull(false), unique(false) {}
};

/* 值容器（Qt3 QValueVector / Qt4 QVector） */
#ifdef QT3_BUILD
typedef QValueVector<SbCell>       SbCellRow;
typedef QValueVector<SbCellRow>    SbCellRows;
typedef QValueVector<QString>      SbStrRow;
typedef QValueVector<SbStrRow>     SbStrRows;
typedef QValueVector<sb_int64>    SbRowids;
typedef QValueList<SbColumn>       SbColumns;
#else
typedef QVector<SbCell>            SbCellRow;
typedef QVector<QVector<SbCell> >  SbCellRows;
typedef QVector<QString>           SbStrRow;
typedef QVector<QVector<QString> > SbStrRows;
typedef QVector<sb_int64>         SbRowids;
typedef QList<SbColumn>            SbColumns;
#endif

struct SbGrid {
    QStringList headers;
    SbCellRows  rows;
    void clear() { headers.clear(); rows.clear(); }
    int rowCount() const { return (int)rows.size(); }
    int colCount() const { return (int)headers.count(); }
};

struct SbBrowsePage {
    QStringList headers;    /* 不含 rowid */
    SbStrRows   cells;
    SbRowids    rowids;
    bool        hasRowid;
    SbBrowsePage() : hasRowid(true) {}
    void clear() {
        headers.clear();
        cells.clear();
        rowids.clear();
        hasRowid = true;
    }
    int rowCount() const { return (int)cells.size(); }
    int colCount() const { return (int)headers.count(); }
};

/* ── 数据库会话（无 QObject，不参与 moc） ── */

class DbSession {
public:
    DbSession();
    ~DbSession();

    bool isOpen() const { return db != 0; }
    bool readOnly() const { return ro; }
    const QString& dbPath() const { return path; }

    /* 打开失败 err 汇报 */
    bool open(const QString& path, bool readOnly, QString& err);
    /* 新建空库文件（先 close 现有连接），随后调用方再 open */
    static bool createEmptyFile(const QString& path, QString& err);
    void close();
    sqlite3* handle() { return db; }

    bool vacuum(QString& err);

    /* 多语句脚本（无结果集需求，dump 导入等） */
    bool execScript(const QString& sql, QString& err);

    /* 执行：抓最后一条有结果集的语句（SQL 编辑区/PRAGMA 查询共用）
     * changes 可取 NULL。无任何结果集时 grid 保持空 + ok。 */
    bool execQuery(const QString& sql, SbGrid& grid, QString& err,
                   sb_int64* changes);

    /* 低层单语句执行（UPDATE/INSERT/DELETE/DDL，一次内可多条） */
    bool runSql(const QString& sql, QString& err, sb_int64* changes);

    /* ── schema 元数据 ── */
    QStringList listObjects(const char* type);       /* type: ""=table index view 都列 */
    QString     objectDdl(const QString& name);
    QString     objectType(const QString& name);
    bool        tableExists(const QString& name);
    SbColumns tableColumns(const QString& table, QString& err);
    /* WITHOUT ROWID 表无 rowid：hasRowid=false 时浏览页用 rowid 兜底层 */
    bool        checkRowidSupport(const QString& table, bool& hasRowid, QString&);

    /* ── 浏览数据 ── */
    int  tableRows(const QString& table, const QString& where, QString& err);
    bool fetchPage(const QString& table, sb_int64 offset, int limit,
                   const QString& orderBySql, const QString& where,
                   SbBrowsePage& page, QString& err);
    /* 值 kind：与 SbCell 一致；blobHex 为 X'..' 字面量原样执行 */
    bool updateCell(const QString& table, sb_int64 rowid,
                    const QString& colName, const QString& valueLiteral,
                    QString& err);
    bool insertDefaultRow(const QString& table, QString& err);
    bool deleteRowByRowid(const QString& table, sb_int64 rowid, QString& err);

    /* ── pragma ── */
    bool pragmaText(const QString& name, QString& out);
    bool pragmaSet(const QString& name, const QString& value, QString& err);

    /* 单元格完整字面量（SELECT quote(col)） */
    bool cellLiteral(const QString& table, sb_int64 rowid,
                     const QString& colName, QString& outLiteral,
                     QString& err);
    /* X'..' blob 字面量解析（供 UI 层判断用） */
    static bool parseHexLiteral(const QString& s, QByteArray& bytes);

    /* ── 导入导出（P5） ── */
    bool exportTableCsv(const QString& table, const QString& file, QString& err,
                        bool includeHeader);
    bool importCsvInto(const QString& table, const QString& file, QString& err,
                       bool firstRowHeader, sb_int64* inserted);
    bool exportDump(const QString& file, QString& err);
    bool importDumpFile(const QString& file, QString& err);

private:
    void gridFromStmt(sqlite3_stmt* stmt, SbGrid& grid);
    /* rowid 定位执行通用段（update/delete 用） */
    bool runWhereRowid(const QString& table, sb_int64 rowid,
                       const QString& stmtSql, QString& err, sb_int64* changes);
    /* CSV 单行解析（处理引号/转义/换行内嵌），返回 false=文件尾部 */
    bool csvNextRecord(FILE* fp, QStringList& out, bool& eof, QString& err);

    sqlite3* db;
    bool     ro;
    QString  path;
};

/* 函数式主窗：factory 声明不进本头文件，按 dummy 范式
 * 在 main.cpp（plugin_create 内）与 exe.cpp 内部就地 extern 声明，
 * 定义加 extern "C"（见 mainwin.cpp） */
class SqlBrowserLogic;
void sbSetBrowserLogicPtr(SqlBrowserLogic* p);

/* ── 上层逻辑（QObject + 槽） ── */

class SqlBrowserLogic : public QObject {
    Q_OBJECT
public:
    SqlBrowserLogic(QObject* parent);
    ~SqlBrowserLogic();

    /* exe 命令行打开（private 只在 UI 流程内用） */
    void openByPath(const QString& path, bool readOnly);

    /* mainwin 组装后注入全部控件 */
    void setWidgets(
        QPushButton* openBtn, QPushButton* newBtn, QPushButton* memBtn,
        QPushButton* closeBtn, QPushButton* vacuumBtn, QCheckBox* roCheck,
        QComboBox* recentsCombo, QLabel* dbLabel,
        QComboBox* objKindCombo, SbListBox* objList, QPushButton* objRefreshBtn,
        /* 结构 tab */
        QPushButton* createTableBtn, QPushButton* editTableBtn,
        QPushButton* dropTableBtn, QPushButton* createIndexBtn,
        QPushButton* dropIndexBtn, QTextEdit* ddlView,
        /* 浏览数据 tab */
        QComboBox* browseTableCombo, QLineEdit* filterEdit,
        QPushButton* filterGoBtn, QPushButton* pagePrevBtn,
        QPushButton* pageNextBtn, QLabel* pageLabel, SbTable* browseGrid,
        QPushButton* insertRowBtn, QPushButton* deleteRowBtn,
        QLabel* rowCountLabel,
        /* SQL tab */
        QTabWidget* sqlTabs, QPushButton* addSqlTabBtn,
        QPushButton* closeSqlTabBtn, QLabel* sqlErrLabel,
        QPushButton* openSqlFileBtn, QPushButton* saveSqlFileBtn,
        /* Pragma tab */
        QLineEdit* pageSizeEdit, QComboBox* journalModeCombo,
        QComboBox* autoVacuumCombo, QComboBox* foreignKeysCombo,
        QLineEdit* userVersionEdit, QPushButton* pragmaApplyBtn,
        QPushButton* pragmaRefreshBtn, QPushButton* quickCheckBtn,
        QPushButton* integrityBtn, QTextEdit* pragmaResultView,
        /* 导入导出 */
        QPushButton* importCsvBtn, QPushButton* exportCsvBtn,
        QPushButton* exportDumpBtn, QPushButton* importDumpBtn,
        /* 日志 */
        QTextEdit* logView, QPushButton* clearLogBtn,
        QLabel* statusLabel);

    void loadPersist();        /* 从 state.json 恢复 recents/SQL 页签 */

public slots:
    /* 库文件 */
    void onOpenDb();
    void onNewDb();
    void onMemoryDb();
    void onCloseDb();
    void onVacuum();
    void onRecentPicked(int idx);
    void onRoToggled(bool checked);

    /* 对象/结构 */
    void onKindChanged(int idx);
    void onRefreshObjects();
    void onObjectSelected();
    void onCreateTable();
    void onEditTable();
    void onDropTable();
    void onCreateIndex();
    void onDropIndex();

    /* 浏览数据 */
    void onBrowseTableChanged(int idx);
    void onFilterGo();
    void onPagePrev();
    void onPageNext();
    void onInsertRow();
    void onDeleteRow();
    void onCellDouble(int r, int c);
    void onSortByCol(int col);

    /* SQL 编辑区 */
    void onRunSql();
    void onAddSqlTab();
    void onCloseSqlTab();
    void onSaveSqlFile();
    void onOpenSqlFile();

    /* 导入导出 */
    void onImportCsv();
    void onExportCsv();
    void onExportDump();
    void onImportDump();

    /* Pragma */
    void onPragmaRefresh();
    void onPragmaApply();
    void onQuickCheck();
    void onIntegrityCheck();

    /* 日志 */
    void onClearLog();

private:
    /* SQL 页签结构（声明置前，成员方法后面引用） */
    struct SqlPage {
        QWidget* page;
        QTextEdit* edit;
        SbTable* grid;
        QPushButton* runBtn;
        SqlPage() : page(0), edit(0), grid(0), runBtn(0) {}
    };
    SqlPage sqlPages[SB_SQLTAB_CAP];
    int     sqlPageCount;

    /* UI 状态 */
    void appendLog(const QString& prefix, const QString& msg);
    void showStatus(const QString& msg);
    void refreshRecentsCombo();
    void pushRecent(const QString& path);
    void saveState();
    void loadStateInternal();
    void refreshDbDerived();        /* 对象列表+浏览combo+pragma 全刷新 */
    void refreshObjectList();
    void refreshBrowseTables();
    void clearBrowseViews();
    QString currentObject() const;
    bool requireDb();
    bool requireWritable();
    void reloadBrowse(bool resetOffset);
    void updatePageLabel();
    void reloadPragmas();
    QString browseTableFromCombo() const;
    /* 浏览页状态 */
    int  currentPageNum() const;
    void setPageNum(int n);

    /* SQL 页签（每页自持编辑区） */
    int  addSqlPage(const QString& content);
    void setActiveSqlPage(int idx);
    int  activeSqlIndex() const;
    SqlPage* activeSqlPage();

    DbSession* session;

    /* 顶排 */
    QPushButton* openBtn;
    QPushButton* newBtn;
    QPushButton* memBtn;
    QPushButton* closeBtn;
    QPushButton* vacuumBtn;
    QCheckBox*   roCheck;
    QComboBox*   recentsCombo;
    QLabel*      dbLabel;
    QStringList  recents;

    /* 对象列 */
    QComboBox*  objKindCombo;
    SbListBox*  objList;
    QPushButton* objRefreshBtn;

    /* 结构 tab */
    QPushButton* createTableBtn;
    QPushButton* editTableBtn;
    QPushButton* dropTableBtn;
    QPushButton* createIndexBtn;
    QPushButton* dropIndexBtn;
    QTextEdit*   ddlView;

    /* 浏览数据 tab */
    QComboBox*   browseTableCombo;
    QLineEdit*   filterEdit;
    QPushButton* filterGoBtn;
    QPushButton* pagePrevBtn;
    QPushButton* pageNextBtn;
    QLabel*      pageLabel;
    SbTable*     browseGrid;
    QPushButton* insertRowBtn;
    QPushButton* deleteRowBtn;
    QLabel*      rowCountLabel;
    QString      browseTableName;  /* 当前浏览表名 */
    QString      browseFilter;     /* WHERE 条件 */
    int          pageNum;          /* 当前页号 0 起 */
    int          sortCol;          /* -1=不排 */
    bool         sortAsc;
    int          lastRows;         /* 最近一次 count(*) */
    QStringList  browseHeaders;    /* 含 rowid 标记 */
    bool         browseHasRowid;
    QWidget*     uiRoot;           /* 对话框父窗 */

    /* SQL tab */
    QTabWidget*  sqlTabs;
    QPushButton* addSqlTabBtn;
    QPushButton* closeSqlTabBtn;
    QPushButton* openSqlFileBtn;
    QPushButton* saveSqlFileBtn;
    QLabel*      sqlErrLabel;

    /* Pragma tab */
    QLineEdit*   pageSizeEdit;
    QComboBox*   journalModeCombo;
    QComboBox*   autoVacuumCombo;
    QComboBox*   foreignKeysCombo;
    QLineEdit*   userVersionEdit;
    QPushButton* pragmaApplyBtn;
    QPushButton* pragmaRefreshBtn;
    QPushButton* quickCheckBtn;
    QPushButton* integrityBtn;
    QTextEdit*   pragmaResultView;

    /* 导入导出 */
    QPushButton* importCsvBtn;
    QPushButton* exportCsvBtn;
    QPushButton* exportDumpBtn;
    QPushButton* importDumpBtn;

    /* 日志 */
    QTextEdit*   logView;
    QPushButton* clearLogBtn;
    QLabel*      statusLabel;
};

#endif
