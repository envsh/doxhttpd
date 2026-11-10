#ifndef V2RAY_SUBER_LOGIC_H
#define V2RAY_SUBER_LOGIC_H

#include <qobject.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qdatetime.h>
#include <qmap.h>

#ifdef QT3_BUILD
#include <qtable.h>
#include <qvaluelist.h>
typedef QTable V2Table;
typedef QValueList<int> V2IntList;
#else
#include <qtablewidget.h>
#include <qlist.h>
typedef QTableWidget V2Table;
typedef QList<int> V2IntList;
#endif

class QTextEdit;
class QLabel;
class QLineEdit;
class QPushButton;
class QProcess;
class QCheckBox;
class QComboBox;
class QTimer;

/* 单个节点（来自订阅解析；字段覆盖持久化/配置生成/测速） */
struct V2Node {
    /* 身份（持久化/去重/选中恢复） */
    QString id;
    QString raw;        /* 原始分享链接 */
    QString proto;      /* vmess / vless / trojan / ss / hysteria2 */
    QString name;       /* URL 解码后的备注 */
    QString addr;
    QString port;

    /* 凭据 */
    QString uuid;       /* vmess/vless id；trojan password；ss password */
    QString aid;        /* vmess alterId */
    QString scy;        /* vmess security */
    QString method;     /* ss 加密方法 */

    /* 传输 */
    QString net;        /* tcp / ws / grpc / h2 / httpupgrade */
    QString path;
    QString host;
    QString serviceName;/* grpc serviceName */
    QString headerType; /* tcp header */

    /* 安全 */
    QString security;   /* none / tls / reality */
    QString sni;
    QString alpn;
    bool    allowInsecure;
    QString fp;         /* reality fingerprint */
    QString pbk;        /* reality publicKey */
    QString sid;        /* reality shortId */
    QString spx;        /* reality spiderX */
    QString flow;       /* vless xtls */

    /* 归属/生命周期 */
    QString sourceSub;
    QString updatedAt;

    /* 测速：-2=未测，-1=失败，>0 毫秒 */
    int     tcpMs;
    int     realMs;
    QString testedAt;

    /* 当前 v2ray-core 能力 */
    bool    supported;

    V2Node() : allowInsecure(false), tcpMs(-2), realMs(-2), supported(false) {}
};

#ifdef QT3_BUILD
typedef QValueList<V2Node> V2NodeList;
#else
typedef QList<V2Node> V2NodeList;
#endif

class V2RayLogic : public QObject {
    Q_OBJECT
public:
    V2RayLogic(QObject* parent);
    void setWidgets(QLineEdit* subEdit, V2Table* subList, V2Table* nodeList,
                    QLabel* coreState, QTextEdit* log,
                    QPushButton* startBtn, QPushButton* stopBtn);
    void setTestWidgets(QPushButton* tcpSel, QPushButton* realSel,
                        QPushButton* tcpAll, QPushButton* realAll, QPushButton* stopTest);
    void setRouteWidgets(QCheckBox* bypassCnCheck, QCheckBox* bypassLanCheck = 0);
    void setListenWidgets(QComboBox* addrBox, QLineEdit* portEdit);
    void setUpdateWidgets(QPushButton* updBtn, QPushButton* updAllBtn, QLabel* nodeStatus);
    void setCoreWidgets(QPushButton* dlBtn);
    void refreshCoreState();

public slots:
    void onSaveSettings();
    void onSaveAllOnQuit();
    void onAddSubscription();
    void onDelSubscription();
    void onUpdateSubscription();
    void onUpdateAllSubscriptions();
    void onDownloadCore();
    void onStartCore();
    void onStopCore();
    void onCopyNodeLink();
    void onTestLatency();
    void onTestTcpSelected();
    void onTestTcpAll();
    void onTestRealSelected();
    void onTestRealAll();
    void onStopTest();
    void onCopySubLink();
    void onSortByLatency();
    void onCleanFailedNodes();
    void onAutoUpdateTick();

signals:
    void nodesChanged();
    void testProgress(int done, int total, int nodeIndex);
    void testFinished();

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
    void onContinueTest();
    void onRealCoreReady();
    void onRealCurlExited();

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
    void loadNodes();
    void saveNodes();
    QString nodeId(const V2Node& n) const;
    int  indexByNodeId(const QString& id) const;
    void removeNodesBySource(const QString& src);
    bool nodeSupportedByCore(const V2Node& n) const;
    void appendNodeDedup(const V2Node& n);
    void startTestBatch(bool real, bool all);
    void startNextTest();
    void finishTest();
    int  pickFreePort();
    void startRealTest(int idx);
    bool buildTestConfig(const V2Node& n, int port, QString& cfg);
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
    void parseShadowsocks(const QString& line, V2Node& node, bool& ok, QString& err);
    bool generateConfig(const V2Node& node, QString& err);
    int  selectedNodeIndex();
    int  selectedSubIndex();
    void clearNodes();
    void startSubFetch(const QString& url);
    void startNextPendingSub();
    void updateAllSubscriptions();
    void startNextLatency();

    QStringList subs;          /* 订阅 URL 列表 */
    V2NodeList nodes;

    QProcess* subProc;
    QProcess* dlProc;
    QProcess* unzipProc;
    QProcess* coreProc;
    QProcess* latProc;
    QProcess* testCoreProc;    /* 真连接测试临时核心 */
    QProcess* testCurlProc;    /* 真连接测试 curl */
    QByteArray subData;
    QByteArray dlErrData;      /* 核心下载 stderr（进度/错误） */
    QByteArray latErrData;     /* 延迟测试 stderr（curl -v trace） */
    bool coreDlBusy;
    bool latBusy;
    bool latGotConnected;
    int  curLatIndex;
    V2IntList testQueue;
    bool testIsReal;
    bool testCancel;
    int  testDone;
    int  testTotal;
    int  testPort;             /* 真连接测试临时 SOCKS 端口 */
    QString testUrl;           /* 默认 https://www.gstatic.com/generate_204 */
    QTime latTime;

    QLineEdit* subEdit;
    V2Table* subList;
    V2Table* nodeList;
    QPushButton* testTcpSelBtn;
    QPushButton* testRealSelBtn;
    QPushButton* testTcpAllBtn;
    QPushButton* testRealAllBtn;
    QPushButton* testStopBtn;
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
