#ifndef V2RAY_SUBER_LOGIC_H
#define V2RAY_SUBER_LOGIC_H

#include <qobject.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qdatetime.h>
#include <qmap.h>

#ifdef QT3_BUILD
#include <qlistbox.h>
typedef QListBox V2ListBox;
#else
#include <qlistwidget.h>
typedef QListWidget V2ListBox;
#endif

class QTextEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QProcess;
class QCheckBox;
class QComboBox;
class QTimer;

/* 单个节点（来自订阅解析） */
struct V2Node {
    QString raw;    /* 原始分享链接 */
    QString proto;  /* vmess / vless / trojan / ss */
    QString name;   /* 备注 */
    QString addr;
    QString port;
    QString uuid;   /* vmess/vless 为 id，trojan 为 password */
    QString net;    /* tcp / ws */
    QString path;
    QString host;
    QString tls;
};

class V2RayLogic : public QObject {
    Q_OBJECT
public:
    V2RayLogic(QObject* parent);
    void setWidgets(QLineEdit* subEdit, V2ListBox* subList, V2ListBox* nodeList,
                    QLabel* coreState, QTextEdit* log,
                    QPushButton* startBtn, QPushButton* stopBtn);
    void setRouteWidgets(QCheckBox* bypassCnCheck, QCheckBox* bypassLanCheck = 0);
    void setListenWidgets(QComboBox* addrBox, QLineEdit* portEdit);
    void setUpdateWidgets(QPushButton* updBtn, QPushButton* updAllBtn, QLabel* nodeStatus);
    void setCoreWidgets(QPushButton* dlBtn);
    void refreshCoreState();

public slots:
    void onSaveSettings();
    void onAddSubscription();
    void onDelSubscription();
    void onUpdateSubscription();
    void onUpdateAllSubscriptions();
    void onDownloadCore();
    void onStartCore();
    void onStopCore();
    void onCopyNodeLink();
    void onTestLatency();
    void onAutoUpdateTick();

private slots:
    void onSubOutput();
    void onSubErrOutput();
    void onSubFetched();
    void onCoreDlOutput();
    void onCoreDlErrOutput();
    void onCoreDlDone();
    void onCoreUnzipDone();
    void onCoreOutput();
    void onCoreErrOutput();
    void onCoreExit();
    void onLatOutput();
    void onLatErrOutput();
    void onLatExited();

private:
    QString baseDir() const;
    QString coreDir() const;
    QString coreBinaryPath() const;
    QString zipPath() const;
    void ensureDirs();
    void saveSettings();
    QString listenAddress() const;
    int     listenPort() const;          /* 非法返回 -1 */
    void startCoreDownload();
    void setCoreMessage(const QString& msg);
    void setUpdateBusy(bool busy, const QString& msg);
    void setCoreDownloadBusy(bool busy);
    void loadSubMeta();
    void saveSubMeta();
    void loadSubscriptions();
    void saveSubscriptions();
    void saveSubLast(const QString& content);
    void appendLog(const QString& msg);
    void appendLogWithPrefix(const QString& prefix, const QString& msg);
    void refreshSubList();
    void refreshNodeList();
    bool coreInstalled() const;
    bool hasGeoFiles() const;
    QString maybeDecodeBase64(const QString& in);
    void parseSubscription(const QString& content);
    void parseVmess(const QString& line, V2Node& node, bool& ok, QString& err);
    void parseVlessTrojan(const QString& line, V2Node& node, bool& ok, QString& err);
    bool generateConfig(const V2Node& node, QString& err);
    int  selectedNodeIndex();
    int  selectedSubIndex();
    void clearNodes();
    void startSubFetch(const QString& url);
    void startNextPendingSub();
    void updateAllSubscriptions();
    void startNextLatency();

    QStringList subs;          /* 订阅 URL 列表 */
    V2Node nodes[512];
    int nodeCount;
    int nodeLatencyMs[512];    /* -1 = 失败/超时 */
    bool nodeLatencyKnown[512];

    QProcess* subProc;
    QProcess* dlProc;
    QProcess* unzipProc;
    QProcess* coreProc;
    QProcess* latProc;
    QByteArray subData;
    QByteArray dlErrData;      /* 核心下载 stderr（进度/错误） */
    QByteArray latErrData;     /* 延迟测试 stderr（curl -v trace） */
    bool coreDlBusy;
    bool latBusy;
    bool latGotConnected;
    int  curLatIndex;
    int  latQueueArr[512];
    int  latQueueCount;
    QTime latTime;

    QLineEdit* subEdit;
    V2ListBox* subList;
    V2ListBox* nodeList;
    QLabel* coreState;
    QTextEdit* log;
    QPushButton* startBtn;
    QPushButton* stopBtn;
    QCheckBox* bypassCnCheck;
    QCheckBox* bypassLanCheck;
    QComboBox* listenAddrBox;
    QLineEdit* listenPortEdit;
    QPushButton* updBtn;
    QPushButton* updAllBtn;
    QLabel* nodeStatus;
    QPushButton* coreDlBtn;

    /* 订阅自动更新（串行队列） */
    QTimer* autoUpdateTimer;
    int autoUpdateIntervalMin;
    QStringList pendingSubUrls;
    int pendingSubIndex;
    bool updatingAll;

    /* 核心下载进度（百分比，-1 = 未知） */
    int dlProgressPct;
    int dlTry;              /* C：下载尝试 0=gh-proxy，1=直连 GitHub */
    QString dlZipName;      /* C：按架构选择的包名 */

    bool coreStopRequested; /* 区分手动停止 / 异常退出 */
    QString curSubUrl;      /* 当前更新中的订阅（写 sub_meta 用） */
    QMap<QString, QString> subMeta;  /* url -> "hh:mm:ss|节点数" */
};

#endif
