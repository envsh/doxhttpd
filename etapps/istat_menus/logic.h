#ifndef ISTAT_LOGIC_H
#define ISTAT_LOGIC_H

#include "compat34.h"
#include <qobject.h>
#include <qtimer.h>
#include <qframe.h>
#include <qprogressbar.h>
#include <qmap.h>
#include <qlabel.h>
#include <qlist.h>
#ifdef QT3_BUILD
#include <qtable.h>
typedef QTable IstmTable;
#else
#include <qtablewidget.h>
typedef QTableWidget IstmTable;
#endif

class QPaintEvent;

#define ISTM_RING_MAX 120
#define ISTM_MAXCORES 12

class IstatLogic;

/* ═══════════ IstatGraph：60 点环形 bar-chart 历史小图（iStat 风格） ═══════════ */

class IstatGraph : public QFrame {
    Q_OBJECT
public:
    IstatGraph(QWidget* parent);
    void setRing(int cap);
    void addSample(double v01);          /* [0,1]，越界 clamp */
    void setColor(int r, int g, int b);  /* Step3：Rx/Tx 双图分色 */
    void getVals(double* out, int max) const; /* Step8：历史持久化读环 */
protected:
    void paintEvent(QPaintEvent*);
private:
    double m_vals[ISTM_RING_MAX];
    int    m_cap;
    int    m_head;
    int    m_n;
    int    m_colR;
    int    m_colG;
    int    m_colB;
};

/* UI 指针包（全裸指针注入，mainwin 构建） */
struct IstatUi {
    /* 顶条 menu-bar 快照（本步点亮 CPU/MEM/Temp；NET/电池/时钟已接通） */
    QLabel*  topCpu;      /* CPU 88% */
    QLabel*  topMem;      /* MEM 60% */
    QLabel*  topNet;      /* NET 下/上行 */
    QLabel*  topTemp;     /* 47°C */
    QLabel*  topBat;      /* 电池 0% · AC */
    QLabel*  topClock;    /* Step7：本地时间 hh:mm · MM-dd 周X */

    /* CPU 页 */
    QLabel*     lblCpuBig;     /* 总占用大字 */
    IstatGraph* graphCpu;      /* 总占用历史 60 点 */
    QLabel*     lblLoad;       /* loadavg · uptime · 型号 */
    QLabel*     lblGpu;        /* Step7：lspci -knn GPU 清单（一次性） */
    IstmTable*  tblCores;      /* Core/MHz/%/°C 每核实时 */
    IstmTable*  tblTopCpu;     /* Top 进程（进程/CPU%/MEM%） */

    /* Memory 页 */
    QLabel*       lblMemBig;   /* used/total 大字 */
    QProgressBar* barMem;      /* 使用率折算条 */
    IstatGraph*   graphMem;    /* 内存历史 */
    QLabel*       lblMemKv;    /* used/avail/cached/SReclaimable/shmem/swapcached */
    QLabel*       lblSwap;     /* swap 总量 + sda6/zram 分列 */
    QLabel*       lblPsi;      /* PSI some/full avg10/60/300 */
    IstmTable*    tblTopMem;   /* Top 进程（与 CPU 页同源数据） */

    /* Network 页（Step3） */
    QLabel*       lblNetBig;   /* ↓总速率 大字 */
    IstatGraph*   graphNetRx;  /* 下行历史（teal） */
    IstatGraph*   graphNetTx;  /* 上行历史（orange） */
    IstmTable*    tblNet;      /* Iface/State·IP/RX速/TX速/累计RX/累计TX */
    QLabel*       lblNetPub;   /* 公网 IP + ss 统计 */

    /* Disk 页（Step4） */
    QLabel*       lblDiskBig;   /* 读写 KB/s 大字 */
    IstatGraph*   graphDisk;    /* 磁盘活动历史（orange） */
    QLabel*       lblDiskIops;  /* IOPS · in_flight 队列 */
    IstmTable*    tblDisk;      /* Device/Type/Size/Used%/Mount */
    QLabel*       lblDiskNote;  /* S.M.A.R.T. root 注记 */

    /* Sensors 页（Step5） */
    IstmTable*  tblSens;       /* Chip/Label/Value 温度·电压·电流全表 */
    QLabel*     lblSensNote;   /* 无风扇/轨数注记 */

    /* Battery 页（Step5） */
    QLabel*       lblBatBig;   /* 电量大字 */
    IstatGraph*   graphBat;    /* 电量史图（blue） */
    QLabel*       lblBatHealth;/* 健康% · 功率 W · 状态 */
    QLabel*       lblBatKv;    /* charge_now/full/design·循环·技术·厂·型 */
    QLabel*       lblBatAc;    /* AC 适配器行 */
    QLabel*       lblWorld;    /* Step7：世界钟 6 城（UTC offset 小字典） */

    /* 每页占位（Step6 起逐页替换为真实控件） */
    QLabel*  lblAbout;         /* About 页文本（通知历史 + 版本说明） */

    IstatUi() {
        topCpu = 0; topMem = 0; topNet = 0; topTemp = 0; topBat = 0;
        topClock = 0;
        lblCpuBig = 0; graphCpu = 0; lblLoad = 0;
        tblCores = 0; tblTopCpu = 0;
        lblGpu = 0;
        lblMemBig = 0; barMem = 0; graphMem = 0;
        lblMemKv = 0; lblSwap = 0; lblPsi = 0; tblTopMem = 0;
        lblNetBig = 0; graphNetRx = 0; graphNetTx = 0;
        tblNet = 0; lblNetPub = 0;
        lblDiskBig = 0; graphDisk = 0; lblDiskIops = 0;
        tblDisk = 0; lblDiskNote = 0;
        tblSens = 0; lblSensNote = 0;
        lblBatBig = 0; graphBat = 0; lblBatHealth = 0;
        lblBatKv = 0; lblBatAc = 0;
        lblWorld = 0;
        lblAbout = 0;
    }
};

class IstatLogic : public QObject {
    Q_OBJECT
public:
    IstatLogic(QObject* parent);
    ~IstatLogic();
    void setWidgets(IstatUi* ui);

private slots:
    void onTick();        /* 1s 节拍：CPU/Mem 差分采样 + 顶条 + 图 push */

private:
    IstatUi*  m_ui;
    QTimer*   m_tickTimer;
    int       m_tick;                      /* 1s tick 计数 */

    enum { ISTM_MAXIF = 8 };               /* 表行上限（超出截断） */
    int       m_netRows;                   /* 当前表行数（10s 重估） */

    /* 静态（/proc/cpuinfo 一次性） */
    QString   m_cpuModel;
    int       m_ncpu;

    /* CPU 差分采样（jiffies；数组下标=逻辑核） */
    long long m_prevTotal;                  /* 汇总行 */
    long long m_prevBusy;
    long long m_prevCoreAll[ISTM_MAXCORES];
    long long m_prevCoreBusy[ISTM_MAXCORES];
    int       m_cpuPct;                     /* 汇总占用 % */
    int       m_corePct[ISTM_MAXCORES];

    /* Memory 采样（KB） */
    long long m_memTotalKB;
    long long m_memUsedKB;

    /* Network 采样（Step3） */
    QMap<QString, long long> m_prevRx;      /* if → 上 tick rx bytes */
    QMap<QString, long long> m_prevTx;
    QMap<QString, QString>   m_ifIp;                /* if → "UP 192.168.49.136/24" */
    QMap<QString, double>    m_rateRx;              /* if → B/s（本 tick 差分） */
    QMap<QString, double>    m_rateTx;
    double    m_netPeakRx;                          /* 图归一化峰值（下限 64KB/s） */
    double    m_netPeakTx;
    QString   m_pubIp;                              /* 公网 IP（120s best-effort） */
    QString   m_ssLine;                             /* ss -s 头两行 */

    /* Disk 采样（Step4） */
    long long m_prevSect;                           /* 读+写扇区累计（上 tick） */
    long long m_prevIos;                            /* 读+写请求累计（上 tick） */
    int       m_diskRows;                           /* df 分区表行数 */
    double    m_diskPeak;                           /* 图归一化峰值（下限 1MB/s） */
    QString   m_diskName;                           /* 主块设备（nvme→sda→vda） */
    int       m_diskIops;
    int       m_diskQueue;
    double    m_diskKbps;                           /* 读+写 合计 KB/s */

    /* Notifications（Step6） */
    int       m_alertCpuHi;      /* CPU>90 连续轮数（轮=10s） */
    int       m_alertMemHi;      /* 内存>90 连续轮数 */
    int       m_alertCoolCpu;    /* CPU 冷却剩余轮（30 轮=5min） */
    int       m_alertCoolMem;
    int       m_alertCoolDisk;
    int       m_alertCoolBat;
    int       m_alertNetRxHi;    /* 下行单独计数（Step8） */
    int       m_alertNetTxHi;    /* 上行单独计数（Step8） */
    int       m_alertCoolNet;
    double    m_netSumRx;        /* 本 tick 全接口合计 B/s（Step7） */
    double    m_netSumTx;
    QString   m_alertHist;       /* 最近 10 条提醒（About 页展示） */

    void    probeCpuStatic();                       /* cpuinfo：核数/型号 */
    void    sampleCpu();                            /* /proc/stat 差分 + 页更新 */
    void    sampleMem();                            /* meminfo/PSI/swaps + 页更新 */
    void    sampleNet();                            /* /proc/net/dev 差分 + 页更新 */
    void    fillNetStatic();                        /* 每 10 tick: ip -brief+ss+行数 */
    void    fetchPubIp();                           /* 每 120 tick：curl 公网 IP */
    void    refreshNetPub();                        /* 公网 IP + ss 文本组合刷新 */
    void    sampleDisk();                           /* /sys/block/<d>/stat 差分 + 页更新 */
    void    fillDiskStatic();                       /* 每 10 tick：df 分区表 */
    void    sampleSensors();                        /* 每 10 tick：hwmon 全表扫描 */
    void    sampleBattery();                        /* 每 10 tick：BAT1/ACAD + 顶条 */
    void    checkAlerts();                          /* 阈值轮询（每 10 tick） */
    void    fillAbout();                            /* About 静态文本 + 通知历史 */
    void    fillGpuRow();                           /* Step7：lspci -knn 一次性 */
    void    sampleTemp();                           /* 顶条 Package 温度 */
    void    saveHistory();                          /* Step8：历史落盘（每 600 tick） */
    void    loadHistory();                          /* Step8：启动还原历史 */
    void    fillTopTables();                        /* 每 10 tick：Top 进程 */
    void    updateTopbar();                         /* CPU/MEM/Temp 三段点亮 */
};

#endif