#ifndef CPUZ_LOGIC_H
#define CPUZ_LOGIC_H

#include "compat34.h"
#include <qobject.h>
#include <qprogressbar.h>
#include <qmap.h>
#include <qlist.h>
#include <qdatetime.h>
#include <qthread.h>
#ifdef QT3_BUILD
#include <qtable.h>
#include <qvaluelist.h>
typedef QTable CpuTable;
#define CPU_CACHE_LIST QValueList<CpuCacheLevel>
#else
#include <qtablewidget.h>
typedef QTableWidget CpuTable;
#define CPU_CACHE_LIST QList<CpuCacheLevel>
#endif

class CpuLogic;

/* UI 指针包（沿用 ytdlp 范式：全裸指针注入，mainwin 构建） */
struct CpuUi {
    QWidget*    cpuzTabs;      /* QTabWidget 本体 */

    /* CPU 页 */
    QLabel*     lblName;       /* Name 大字 */
    QLabel*     lblSpec;       /* Family/Model/Stepping/微码 */
    QLabel*     lblIdent;      /* CodeName / Package / Technology */
    QLabel*     lblClocks;     /* Core Speed 大字 */
    QProgressBar* barClock;    /* min~max 折算条 */
    QLabel*     lblMult;       /* 倍频 + BCLK */
    QLabel*     lblTemp;       /* 实时温度行（coretemp/acpitz，无则 —） */
    CpuTable*   coresTable;    /* 每核实时频率 4 列 */
    QTimer*     tickTimer;     /* 1000ms 实时刷新 */
    QLabel*     lblInstr;      /* 指令集一行（多行换行） */

    /* Caches 页 */
    QLabel*     lblL1D;
    QLabel*     lblL1I;
    QLabel*     lblL2;
    QLabel*     lblL3;

    /* Mainboard 页 */
    QLabel*     lblBoardMfr;
    QLabel*     lblBoardModel;
    QLabel*     lblBoardVer;
    QLabel*     lblChipset;
    QLabel*     lblBiosBrand;
    QLabel*     lblBiosVer;
    QLabel*     lblChassis;

    /* Graphics 页 */
    CpuTable*   gpusTable;     /* 5 列 lspci 解析 */

    /* Memory 页 */
    QLabel*     lblMemInfo;    /* 多行 */
    QLabel*     lblMemNote;    /* 限制注记 */

    /* Bench 页 */
    QPushButton* btnBench;
    QPushButton* btnReport;   /* 导出 txt 报告 */
    QProgressBar* barBench;
    QLabel*     lblBenchScore; /* Single/Multi/倍比 三行 */
    QLabel*     lblBenchState;
    QLabel*     lblBenchNote;

    CpuUi() {
        cpuzTabs = 0;
        lblName = 0; lblSpec = 0; lblIdent = 0;
        lblClocks = 0; barClock = 0; lblMult = 0;
        coresTable = 0; tickTimer = 0; lblInstr = 0;
        lblL1D = 0; lblL1I = 0; lblL2 = 0; lblL3 = 0;
        lblTemp = 0;
        lblBoardMfr = 0; lblBoardModel = 0; lblBoardVer = 0;
        lblChipset = 0; lblBiosBrand = 0; lblBiosVer = 0; lblChassis = 0;
        gpusTable = 0;
        lblMemInfo = 0; lblMemNote = 0;
        btnBench = 0; btnReport = 0; barBench = 0; lblBenchScore = 0;
        lblBenchState = 0; lblBenchNote = 0;
    }
};

class CpuLogic : public QObject {
    Q_OBJECT
public:
    CpuLogic(QObject* parent);
    ~CpuLogic();
    void setWidgets(CpuUi* ui);

private slots:
    void onTick();                     /* 实时频率刷新（仅 CPU 页数据量小，不做可见性抑制） */
    void onBench();                    /* [开始基准] */
    void onReport();                   /* [导出报告] */
    void onBenchTick();                /* 段进度推进/阶段切换/收尾 */

private:
    CpuUi*  m_ui;
    QTimer* m_tickTimer;
    int     m_benchState;      /* 0 空闲 1 运行 */

    /* Step4：实时频率 */
    int readCurMhz(int cpuIdx);
    static void barPct(QProgressBar* bar, int pct);  /* Qt3 setProgress / Qt4 setValue */

    /* ── Step3：静态采集 ── */
    void    probeBase();       /* lscpu/cpuinfo/sysfs 一次性采集 */
    void    probeChipset();    /* lspci Host/ISA bridge ≈ 北/南桥近似 */
    void    fillCpuPage();
    void    fillCachesPage();
    void    fillMainboardPage();
    void    fillGraphicsPage();
    void    fillMemoryPage();
    QString makeReportText();  /* 六页汇总文本 + 时间戳 */

    /* 采集结果缓存 */
    QString m_cpuName;
    QString m_cpuVendor;
    int     m_fam, m_model, m_step;
    QString m_microCode;
    int     m_sockets, m_coresPer, m_threadsPer, m_totalCpus;
    int     m_minMhz, m_maxMhz;
    QString m_flags;
    QString m_codename;
    QString m_technode;
    QString m_chipset;
    QString m_southbridge;
    double  m_bogo;

    struct CpuCacheLevel {
        int     level;
        QString type;          /* Data / Instruction / Unified */
        int     sizeK;
        int     ways;
        int     line;
        QString shared;        /* shared_cpu_list */
        CpuCacheLevel() : level(0), sizeK(0), ways(0), line(0) {}
    };
    CPU_CACHE_LIST m_caches;

    /* helpers */
    static QMap<QString, QString> parseKvLines(const QString& text);
    static QString instrFriendly(const QString& rawFlags);
    static QString codenameLookup(int fam, int model, int step);
    static QString readTrimmedFile(const QString& path);

    /* ── Bench（自研口径：定时长计次，与 CPU-Z 分不可比） ── */
    void    benchFinishUi();

    QTimer*  m_benchTimer;
    QThread* m_benchRunner;            /* 当前分段线程（单/多线程皆此类） */
    QTime    m_benchT0;                /* 段起始时间 */
    int      m_benchPhase;             /* 0 空 1 单线程 2 多线程 */
    long     m_singleScore;
    long     m_multiScore;
};

#endif
