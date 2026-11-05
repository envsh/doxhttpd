#ifndef YTDLP_LOGIC_H
#define YTDLP_LOGIC_H

#include <qobject.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qtimer.h>
#include <string>

class QWidget;
class QLabel;
class QLineEdit;
class QTextEdit;
class QPushButton;
class QComboBox;
class QSpinBox;
class QCheckBox;
class QTable;
class QProcess;

/* Qt3 QList<QValueList> 是指针容器；值容器用 QValueList（Qt4 对应 QList） */
#ifdef QT3_BUILD
#define YTDLP_TASK_LIST QValueList<YtdlpTask>
#else
#define YTDLP_TASK_LIST QList<YtdlpTask>
#endif

#ifdef QT3_BUILD
class QTable;
typedef QTable YtdlpTable;
#else
class QTableWidget;
typedef QTableWidget YtdlpTable;
#endif

/* 全部控件指针聚合（mainwin.cpp 构建后注入 logic） */
struct YtdlpUi {
    /* Tab1 主下载 */
    QTextEdit*    urlsEdit;
    QComboBox*    presetH;        /* 0自动 1best+ba 2高度上限 3自定义-f */
    QSpinBox*     heightCap;
    QLineEdit*    fmtCustom;
    QCheckBox*    extractAudio;
    QComboBox*    audioFmt;
    QComboBox*    audioQual;
    QLineEdit*    dirEdit;
    QPushButton*  dirPick;
    QCheckBox*    useArchive;
    QLineEdit*    archiveEdit;
    QPushButton*  archivePick;
    QLineEdit*    itemsSpec;      /* 播放列表 -I，空=整单 */
    QPushButton*  btnStart;
    QPushButton*  btnStop;

    /* Tab2 网络登录 */
    QLineEdit*    cookieEdit;
    QPushButton*  cookiePick;
    QLineEdit*    userEdit;
    QLineEdit*    passEdit;
    QLineEdit*    proxyEdit;
    QLineEdit*    rateEdit;
    QSpinBox*     retriesSpin;
    QSpinBox*     concSpin;

    /* Tab3 后处理 */
    QCheckBox*    embedSubs;
    QCheckBox*    embedThumb;
    QCheckBox*    embedMeta;
    QCheckBox*    embedChapters;
    QLineEdit*    subLangsEdit;
    QComboBox*    convertSubs;
    QComboBox*    mergeFmt;
    QCheckBox*    sbRemove;
    QLineEdit*    sbCatsEdit;
    QLineEdit*    ppaEdit;

    /* Tab4 格式实测 */
    QLineEdit*    probeUrl;
    QPushButton*  btnProbe;
    QPushButton*  btnFill;        /* 回填选中格式 ID */
    QLabel*       probeStatus;
    YtdlpTable*   formatsTable;

    /* Tab5 任务 */
    QLineEdit*    cmdPreview;     /* 只读：完整命令行预览 */
    YtdlpTable*   taskTable;
    QTextEdit*    histLog;        /* appendLogWithPrefix 落点 */

    /* 底部状态横条 */
    QLabel*       versionLabel;   /* yt-dlp/--version 探测结果 */

    YtdlpUi();
};

/* 配置镜像（表单 ⇄ config.json） */
struct YtdlpConfig {
    int     formatPreset;      /* 0自动 1best 2高度上限 3自定义 */
    int     height;
    QString fmtCustom;
    bool    extractAudio;
    QString audioFmt;
    QString audioQual;
    QString dir;
    bool    useArchive;
    QString archive;
    QString items;
    QString cookie;
    QString username;
    QString password;
    QString proxy;
    QString rate;
    int     retries;
    int     conc;
    bool    embedSubs;
    bool    embedThumb;
    bool    embedMeta;
    bool    embedChapters;
    QString subLangs;
    QString convertSubs;       /* ""=不转换 */
    QString mergeFmt;          /* ""=不指定 */
    bool    sbRemove;
    QString sbCats;
    QString ppa;

    YtdlpConfig();
};

/* 任务（串行队列，一次一个） */
struct YtdlpTask {
    QString url;
    int     row;      /* QTable 行号 */
    int     state;    /* 0排队 1运行 2完成 3失败 4已取消 */
    int     pct;
    QString speed;
    QString eta;
    QString outPath;
    int     pctLogged;   /* 上次打日志的进度，防刷屏 */

    YtdlpTask() : row(-1), state(0), pct(0), pctLogged(0) {}
};

class YtdlpLogic : public QObject {
    Q_OBJECT
public:
    YtdlpLogic(QObject* parent = 0);
    void setWidgets(YtdlpUi* ui);

private slots:
    void onFormChanged();        /* 任意表单变化 → 预览刷新 + 延迟存盘 */
    void onSaveTick();           /* 防抖定时器到点 → 写 config.json */
    void onPickDir();
    void onPickArchive();
    void onPickCookie();
    void onCopyCommand();
    void onProbeFormats();       /* 启动 -J 探测（Step5） */
    void onProbeReadOut();
    void onProbeReadErr();
    void onProbeDone();
    void onFillFormat();
    void onStartClicked();       /* urlsEdit 每行 → 任务入队 */
    void onStopClicked();        /* 终止当前任务 */
    void startNext();            /* 串行弹队 */
    void onProcReadOut();
    void onProcReadErr();
    void onProcDone();
    void onKillTick();

private:
    void connectAll();
    void spawnTask();
    void loadConfig();
    void applyConfig(const YtdlpConfig& c);
    void collectConfig(YtdlpConfig* c) const;
    void saveConfig();
    void ensureDirs() const;
    QString baseDir() const;
    void updateCmdPreview();
    QStringList buildArgs(const QString& singleUrl = QString()) const;
    QString buildCmdText() const;       /* join 展示串 */
    void probeYtdlp();
    void setEnabledStates(bool ytdlpOk);
    void appendLog(const QString& msg);
    void appendLogWithPrefix(const QString& prefix, const QString& msg);
    void parseProcLine(const QString& line);
    void consumeStream(const QByteArray& chunk, std::string* buf);

private:
    YtdlpUi*  m_ui;
    QObject*  m_procParent;
    QTimer*   m_saveTimer;
    bool      m_ytdlpOk;
    bool      m_loading;         /* 回填期间抑制 onFormChanged */

    YTDLP_TASK_LIST m_queue;
    QProcess* m_curProc;
    int       m_curIdx;          /* 当前运行任务下标；-1 无 */
    bool      m_cancelling;
    bool      m_killPending;
    QTimer*   m_killTimer;
    std::string m_outBuf;
    std::string m_errBuf;

    QProcess*   m_probeProc;     /* 独立于下载队列，可并行 */
    std::string m_probeBuf;
};

#endif  // YTDLP_LOGIC_H
