#include "compat34.h"
#include "dblogic.h"

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
#include <qdatetime.h>
#include <qfile.h>

#ifdef QT3_BUILD
#include <qheader.h>
#else
#include <qheaderview.h>
#endif

extern "C"
QWidget* createSqlBrowserMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);

#ifdef QT3_BUILD
    w->setCaption(qFromUtf8("SQLite 数据库浏览器"));
#else
    w->setWindowTitle(qFromUtf8("SQLite 数据库浏览器"));
#endif
    w->resize(980, 660);

    QBoxLayout* mainLayout = qNewBoxLayout(w, QBoxLayout::TopToBottom, 6, 5);

    /* ── 顶部：库文件操作 ── */
    QBoxLayout* topRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    mainLayout->addLayout(topRow);
    QPushButton* openBtn = new QPushButton(qFromUtf8("打开"), w);
    QPushButton* newBtn = new QPushButton(qFromUtf8("新建"), w);
    QPushButton* memBtn = new QPushButton(qFromUtf8("内存库"), w);
    QPushButton* closeBtn = new QPushButton(qFromUtf8("关闭"), w);
    QPushButton* vacuumBtn = new QPushButton(qFromUtf8("压缩(VACUUM)"), w);
    QCheckBox* roCheck = new QCheckBox(qFromUtf8("只读"), w);
    QLabel* recentsLbl = new QLabel(qFromUtf8("最近："), w);
    QComboBox* recentsCombo = new QComboBox(w);
    QLabel* dbLabel = new QLabel(qFromUtf8("库：未打开"), w);
    topRow->addWidget(openBtn);
    topRow->addWidget(newBtn);
    topRow->addWidget(memBtn);
    topRow->addWidget(closeBtn);
    topRow->addWidget(vacuumBtn);
    topRow->addWidget(roCheck);
    topRow->addWidget(recentsLbl);
    topRow->addWidget(recentsCombo, 1);
    topRow->addWidget(dbLabel);

    /* ── 中部：左对象列表 + 右功能区 ── */
    QBoxLayout* midRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 5);
    mainLayout->addLayout(midRow, 1);

    /* 左列：对象 */
    QBoxLayout* leftCol = qNewBoxLayout(0, QBoxLayout::TopToBottom, 0, 3);
    midRow->addLayout(leftCol, 1);
    QLabel* objLbl = new QLabel(qFromUtf8("数据库对象"), w);
    leftCol->addWidget(objLbl);
    QComboBox* objKindCombo = new QComboBox(w);
    sbComboAdd(objKindCombo, qFromUtf8("表"));
    sbComboAdd(objKindCombo, qFromUtf8("索引"));
    sbComboAdd(objKindCombo, qFromUtf8("视图"));
    leftCol->addWidget(objKindCombo);
    SbListBox* objList = new SbListBox(w);
    leftCol->addWidget(objList, 1);
    QPushButton* objRefreshBtn = new QPushButton(qFromUtf8("刷新"), w);
    leftCol->addWidget(objRefreshBtn);

    /* 左列：导入导出 */
    leftCol->addWidget(new QLabel(qFromUtf8("导入导出"), w));
    QPushButton* importCsvBtn = new QPushButton(qFromUtf8("CSV 导入"), w);
    QPushButton* exportCsvBtn = new QPushButton(qFromUtf8("CSV 导出"), w);
    QPushButton* importDumpBtn = new QPushButton(qFromUtf8("SQL 导入"), w);
    QPushButton* exportDumpBtn = new QPushButton(qFromUtf8("SQL 导出"), w);
    leftCol->addWidget(importCsvBtn);
    leftCol->addWidget(exportCsvBtn);
    leftCol->addWidget(importDumpBtn);
    leftCol->addWidget(exportDumpBtn);

    /* 右：五个功能 tab */
    QTabWidget* mainTabs = new QTabWidget(w);
    midRow->addWidget(mainTabs, 4);

    /* 1) 结构 */
    QWidget* structPage = new QWidget(mainTabs);
    QBoxLayout* structLayout = qNewBoxLayout(structPage, QBoxLayout::TopToBottom, 6, 4);
    QBoxLayout* structBtnRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    structLayout->addLayout(structBtnRow);
    QPushButton* createTableBtn = new QPushButton(qFromUtf8("建表"), structPage);
    QPushButton* editTableBtn = new QPushButton(qFromUtf8("改表"), structPage);
    QPushButton* dropTableBtn = new QPushButton(qFromUtf8("删表"), structPage);
    QPushButton* createIndexBtn = new QPushButton(qFromUtf8("建索引"), structPage);
    QPushButton* dropIndexBtn = new QPushButton(qFromUtf8("删索引"), structPage);
    structBtnRow->addWidget(createTableBtn);
    structBtnRow->addWidget(editTableBtn);
    structBtnRow->addWidget(dropTableBtn);
    structBtnRow->addWidget(createIndexBtn);
    structBtnRow->addWidget(dropIndexBtn);
    structBtnRow->addStretch(1);
    QLabel* ddlLbl = new QLabel(qFromUtf8("DDL（选左侧对象）"), structPage);
    structLayout->addWidget(ddlLbl);
    QTextEdit* ddlView = new QTextEdit(structPage);
    sbTextEditSetPlain(ddlView);
    ddlView->setReadOnly(true);
    structLayout->addWidget(ddlView, 1);
    mainTabs->addTab(structPage, qFromUtf8("结构"));

    /* 2) 浏览数据 */
    QWidget* browsePage = new QWidget(mainTabs);
    QBoxLayout* browseLayout = qNewBoxLayout(browsePage, QBoxLayout::TopToBottom, 6, 4);
    QBoxLayout* browseRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    browseLayout->addLayout(browseRow);
    QLabel* tblLbl = new QLabel(qFromUtf8("表："), browsePage);
    QComboBox* browseTableCombo = new QComboBox(browsePage);
    browseTableCombo->setMinimumWidth(180);
    QLabel* filterLbl = new QLabel(qFromUtf8("WHERE："), browsePage);
    QLineEdit* filterEdit = new QLineEdit(browsePage);
    QPushButton* filterGoBtn = new QPushButton(qFromUtf8("筛选"), browsePage);
    browseRow->addWidget(tblLbl);
    browseRow->addWidget(browseTableCombo);
    browseRow->addWidget(filterLbl);
    browseRow->addWidget(filterEdit, 1);
    browseRow->addWidget(filterGoBtn);
    QBoxLayout* pageRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    browseLayout->addLayout(pageRow);
    QPushButton* pagePrevBtn = new QPushButton(qFromUtf8("上一页"), browsePage);
    QPushButton* pageNextBtn = new QPushButton(qFromUtf8("下一页"), browsePage);
    QLabel* pageLabel = new QLabel(qFromUtf8("页：-"), browsePage);
    QLabel* rowCountLabel = new QLabel(qFromUtf8("行数：-"), browsePage);
    pageRow->addWidget(pagePrevBtn);
    pageRow->addWidget(pageNextBtn);
    pageRow->addWidget(pageLabel);
    pageRow->addStretch(1);
    pageRow->addWidget(rowCountLabel);
    SbTable* browseTable = new SbTable(browsePage);
    browseLayout->addWidget(browseTable, 1);
    sbTableSetReadOnly(browseTable, true);
    QBoxLayout* editRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    browseLayout->addLayout(editRow);
    QPushButton* insertRowBtn = new QPushButton(qFromUtf8("加行"), browsePage);
    QPushButton* deleteRowBtn = new QPushButton(qFromUtf8("删行"), browsePage);
    QLabel* hintLbl = new QLabel(qFromUtf8("双击格编辑；NULL 与空串在(x)列以 (NULL) 显示"), browsePage);
    editRow->addWidget(insertRowBtn);
    editRow->addWidget(deleteRowBtn);
    editRow->addStretch(1);
    editRow->addWidget(hintLbl);
    mainTabs->addTab(browsePage, qFromUtf8("浏览数据"));

    /* 3) 执行 SQL */
    QWidget* sqlPage = new QWidget(mainTabs);
    QBoxLayout* sqlLayout = qNewBoxLayout(sqlPage, QBoxLayout::TopToBottom, 6, 4);
    QBoxLayout* sqlBtnRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    sqlLayout->addLayout(sqlBtnRow);
    QPushButton* addSqlTabBtn = new QPushButton(qFromUtf8("新页签"), sqlPage);
    QPushButton* closeSqlTabBtn = new QPushButton(qFromUtf8("关闭页签"), sqlPage);
    QPushButton* openSqlFileBtn = new QPushButton(qFromUtf8("载入SQL文件"), sqlPage);
    QPushButton* saveSqlFileBtn = new QPushButton(qFromUtf8("保存SQL文件"), sqlPage);
    sqlBtnRow->addWidget(addSqlTabBtn);
    sqlBtnRow->addWidget(closeSqlTabBtn);
    sqlBtnRow->addWidget(openSqlFileBtn);
    sqlBtnRow->addWidget(saveSqlFileBtn);
    sqlBtnRow->addStretch(1);
    QTabWidget* sqlTabs = new QTabWidget(sqlPage);
    sqlLayout->addWidget(sqlTabs, 1);
    QLabel* sqlErrLabel = new QLabel(qFromUtf8("SQL：-"), sqlPage);
    sqlLayout->addWidget(sqlErrLabel);
    mainTabs->addTab(sqlPage, qFromUtf8("执行SQL"));

    /* 4) Pragma */
    QWidget* pragmaPage = new QWidget(mainTabs);
    QBoxLayout* pragmaLayout = qNewBoxLayout(pragmaPage, QBoxLayout::TopToBottom, 6, 4);
    QBoxLayout* prRow1 = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    prRow1->addWidget(new QLabel(qFromUtf8("page_size："), pragmaPage));
    QLineEdit* pageSizeEdit = new QLineEdit(pragmaPage);
    pageSizeEdit->setMaximumWidth(120);
    prRow1->addWidget(pageSizeEdit);
    prRow1->addStretch(1);
    QBoxLayout* prRow2 = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    prRow2->addWidget(new QLabel(qFromUtf8("journal_mode："), pragmaPage));
    QComboBox* journalModeCombo = new QComboBox(pragmaPage);
    sbComboAdd(journalModeCombo, "delete");
    sbComboAdd(journalModeCombo, "truncate");
    sbComboAdd(journalModeCombo, "persist");
    sbComboAdd(journalModeCombo, "memory");
    sbComboAdd(journalModeCombo, "wal");
    prRow2->addWidget(journalModeCombo);
    prRow2->addStretch(1);
    QBoxLayout* prRow3 = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    prRow3->addWidget(new QLabel(qFromUtf8("auto_vacuum："), pragmaPage));
    QComboBox* autoVacuumCombo = new QComboBox(pragmaPage);
    sbComboAdd(autoVacuumCombo, "0");
    sbComboAdd(autoVacuumCombo, "1");
    sbComboAdd(autoVacuumCombo, "2");
    prRow3->addWidget(autoVacuumCombo);
    prRow3->addStretch(1);
    QBoxLayout* prRow4 = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    prRow4->addWidget(new QLabel(qFromUtf8("foreign_keys："), pragmaPage));
    QComboBox* foreignKeysCombo = new QComboBox(pragmaPage);
    sbComboAdd(foreignKeysCombo, "0");
    sbComboAdd(foreignKeysCombo, "1");
    prRow4->addWidget(foreignKeysCombo);
    prRow4->addStretch(1);
    QBoxLayout* prRow5 = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    prRow5->addWidget(new QLabel(qFromUtf8("user_version："), pragmaPage));
    QLineEdit* userVersionEdit = new QLineEdit(pragmaPage);
    userVersionEdit->setMaximumWidth(120);
    prRow5->addWidget(userVersionEdit);
    prRow5->addStretch(1);
    pragmaLayout->addLayout(prRow1);
    pragmaLayout->addLayout(prRow2);
    pragmaLayout->addLayout(prRow3);
    pragmaLayout->addLayout(prRow4);
    pragmaLayout->addLayout(prRow5);
    QBoxLayout* prBtnRow = qNewBoxLayout(0, QBoxLayout::LeftToRight, 2, 4);
    QPushButton* pragmaRefreshBtn = new QPushButton(qFromUtf8("刷新"), pragmaPage);
    QPushButton* pragmaApplyBtn = new QPushButton(qFromUtf8("应用"), pragmaPage);
    QPushButton* quickCheckBtn = new QPushButton(qFromUtf8("quick_check"), pragmaPage);
    QPushButton* integrityBtn = new QPushButton(qFromUtf8("integrity_check"), pragmaPage);
    prBtnRow->addWidget(pragmaRefreshBtn);
    prBtnRow->addWidget(pragmaApplyBtn);
    prBtnRow->addWidget(quickCheckBtn);
    prBtnRow->addWidget(integrityBtn);
    prBtnRow->addStretch(1);
    pragmaLayout->addLayout(prBtnRow);
    QLabel* prResLbl = new QLabel(qFromUtf8("检查结果 / 记录"), pragmaPage);
    pragmaLayout->addWidget(prResLbl);
    QTextEdit* pragmaResultView = new QTextEdit(pragmaPage);
    sbTextEditSetPlain(pragmaResultView);
    pragmaResultView->setReadOnly(true);
    pragmaLayout->addWidget(pragmaResultView, 1);
    mainTabs->addTab(pragmaPage, qFromUtf8("Pragma"));

    /* 5) 日志 */
    QWidget* logPage = new QWidget(mainTabs);
    QBoxLayout* logLayout = qNewBoxLayout(logPage, QBoxLayout::TopToBottom, 6, 4);
    QTextEdit* logView = new QTextEdit(logPage);
    sbTextEditSetPlain(logView);
    logView->setReadOnly(true);
    logLayout->addWidget(logView, 1);
    QPushButton* clearLogBtn = new QPushButton(qFromUtf8("清空日志"), logPage);
    logLayout->addWidget(clearLogBtn);
    mainTabs->addTab(logPage, qFromUtf8("日志"));

    /* ── 底部状态 ── */
    QLabel* statusLabel = new QLabel(qFromUtf8("就绪"), w);
    mainLayout->addWidget(statusLabel);

    /* ── 逻辑关联 ── */
    /* 逻辑关联（工厂创建后记录单例指针，供 exe 命令行打开使用） */
    SqlBrowserLogic* logic = new SqlBrowserLogic(w);
    sbSetBrowserLogicPtr(logic);
    logic->setWidgets(
        openBtn, newBtn, memBtn, closeBtn, vacuumBtn, roCheck,
        recentsCombo, dbLabel,
        objKindCombo, objList, objRefreshBtn,
        createTableBtn, editTableBtn, dropTableBtn, createIndexBtn,
        dropIndexBtn, ddlView,
        browseTableCombo, filterEdit, filterGoBtn, pagePrevBtn,
        pageNextBtn, pageLabel, browseTable, insertRowBtn, deleteRowBtn,
        rowCountLabel,
        sqlTabs, addSqlTabBtn, closeSqlTabBtn, sqlErrLabel,
        openSqlFileBtn, saveSqlFileBtn,
        pageSizeEdit, journalModeCombo, autoVacuumCombo, foreignKeysCombo,
        userVersionEdit, pragmaApplyBtn, pragmaRefreshBtn, quickCheckBtn,
        integrityBtn, pragmaResultView,
        importCsvBtn, exportCsvBtn, exportDumpBtn, importDumpBtn,
        logView, clearLogBtn, statusLabel);

    /* 顶排 */
    QObject::connect(openBtn, SIGNAL(clicked()), logic, SLOT(onOpenDb()));
    QObject::connect(newBtn, SIGNAL(clicked()), logic, SLOT(onNewDb()));
    QObject::connect(memBtn, SIGNAL(clicked()), logic, SLOT(onMemoryDb()));
    QObject::connect(closeBtn, SIGNAL(clicked()), logic, SLOT(onCloseDb()));
    QObject::connect(vacuumBtn, SIGNAL(clicked()), logic, SLOT(onVacuum()));
    QObject::connect(recentsCombo, SIGNAL(activated(int)), logic,
                     SLOT(onRecentPicked(int)));
    QObject::connect(roCheck, SIGNAL(toggled(bool)), logic,
                     SLOT(onRoToggled(bool)));

    /* 对象列 */
    QObject::connect(objKindCombo, SIGNAL(activated(int)), logic,
                     SLOT(onKindChanged(int)));
    QObject::connect(objRefreshBtn, SIGNAL(clicked()), logic,
                     SLOT(onRefreshObjects()));
#ifdef QT3_BUILD
    QObject::connect(objList, SIGNAL(highlighted(int)), logic,
                     SLOT(onObjectSelected()));
#else
    QObject::connect(objList, SIGNAL(currentRowChanged(int)), logic,
                     SLOT(onObjectSelected()));
#endif

    /* 结构 */
    QObject::connect(createTableBtn, SIGNAL(clicked()), logic,
                     SLOT(onCreateTable()));
    QObject::connect(editTableBtn, SIGNAL(clicked()), logic,
                     SLOT(onEditTable()));
    QObject::connect(dropTableBtn, SIGNAL(clicked()), logic,
                     SLOT(onDropTable()));
    QObject::connect(createIndexBtn, SIGNAL(clicked()), logic,
                     SLOT(onCreateIndex()));
    QObject::connect(dropIndexBtn, SIGNAL(clicked()), logic,
                     SLOT(onDropIndex()));

    /* 浏览数据 */
    QObject::connect(browseTableCombo, SIGNAL(activated(int)), logic,
                     SLOT(onBrowseTableChanged(int)));
    QObject::connect(filterGoBtn, SIGNAL(clicked()), logic,
                     SLOT(onFilterGo()));
    QObject::connect(filterEdit, SIGNAL(returnPressed()), logic,
                     SLOT(onFilterGo()));
    QObject::connect(pagePrevBtn, SIGNAL(clicked()), logic,
                     SLOT(onPagePrev()));
    QObject::connect(pageNextBtn, SIGNAL(clicked()), logic,
                     SLOT(onPageNext()));
    QObject::connect(insertRowBtn, SIGNAL(clicked()), logic,
                     SLOT(onInsertRow()));
    QObject::connect(deleteRowBtn, SIGNAL(clicked()), logic,
                     SLOT(onDeleteRow()));
#ifdef QT3_BUILD
    QObject::connect(browseTable, SIGNAL(doubleClicked(int, int)), logic,
                     SLOT(onCellDouble(int, int)));
    QObject::connect(browseTable->horizontalHeader(), SIGNAL(clicked(int)),
                     logic, SLOT(onSortByCol(int)));
#else
    QObject::connect(browseTable, SIGNAL(cellDoubleClicked(int, int)), logic,
                     SLOT(onCellDouble(int, int)));
    QObject::connect(browseTable->horizontalHeader(),
                     SIGNAL(sectionClicked(int)), logic,
                     SLOT(onSortByCol(int)));
#endif

    /* SQL */
    QObject::connect(addSqlTabBtn, SIGNAL(clicked()), logic,
                     SLOT(onAddSqlTab()));
    QObject::connect(closeSqlTabBtn, SIGNAL(clicked()), logic,
                     SLOT(onCloseSqlTab()));
    QObject::connect(openSqlFileBtn, SIGNAL(clicked()), logic,
                     SLOT(onOpenSqlFile()));
    QObject::connect(saveSqlFileBtn, SIGNAL(clicked()), logic,
                     SLOT(onSaveSqlFile()));

    /* Pragma */
    QObject::connect(pragmaRefreshBtn, SIGNAL(clicked()), logic,
                     SLOT(onPragmaRefresh()));
    QObject::connect(pragmaApplyBtn, SIGNAL(clicked()), logic,
                     SLOT(onPragmaApply()));
    QObject::connect(quickCheckBtn, SIGNAL(clicked()), logic,
                     SLOT(onQuickCheck()));
    QObject::connect(integrityBtn, SIGNAL(clicked()), logic,
                     SLOT(onIntegrityCheck()));

    /* 导入导出 */
    QObject::connect(importCsvBtn, SIGNAL(clicked()), logic,
                     SLOT(onImportCsv()));
    QObject::connect(exportCsvBtn, SIGNAL(clicked()), logic,
                     SLOT(onExportCsv()));
    QObject::connect(importDumpBtn, SIGNAL(clicked()), logic,
                     SLOT(onImportDump()));
    QObject::connect(exportDumpBtn, SIGNAL(clicked()), logic,
                     SLOT(onExportDump()));

    /* 日志 */
    QObject::connect(clearLogBtn, SIGNAL(clicked()), logic,
                     SLOT(onClearLog()));

    return w;
}
