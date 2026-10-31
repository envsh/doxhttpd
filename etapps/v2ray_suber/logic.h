#ifndef V2RAY_SUBER_LOGIC_H
#define V2RAY_SUBER_LOGIC_H

#include <qobject.h>
#include <qstring.h>
#include <qstringlist.h>

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
    void refreshCoreState();

public slots:
    void onAddSubscription();
    void onDelSubscription();
    void onUpdateSubscription();
    void onDownloadCore();
    void onStartCore();
    void onStopCore();
    void onCopyNodeLink();

private slots:
    void onSubOutput();
    void onSubFetched();
    void onCoreDlOutput();
    void onCoreDlDone();
    void onCoreUnzipDone();
    void onCoreOutput();
    void onCoreExit();

private:
    QString baseDir() const;
    QString coreDir() const;
    QString coreBinaryPath() const;
    QString zipPath() const;
    void ensureDirs();
    void loadSubscriptions();
    void saveSubscriptions();
    void saveSubLast(const QString& content);
    void appendLog(const QString& msg);
    void refreshSubList();
    void refreshNodeList();
    bool coreInstalled() const;
    QString maybeDecodeBase64(const QString& in);
    void b64Decode(const QString& in, QByteArray& out);
    void parseSubscription(const QString& content);
    void parseVmess(const QString& line, V2Node& node, bool& ok, QString& err);
    void parseVlessTrojan(const QString& line, V2Node& node, bool& ok, QString& err);
    bool generateConfig(const V2Node& node, QString& err);
    int  selectedNodeIndex();
    int  selectedSubIndex();
    void clearNodes();

    QStringList subs;          /* 订阅 URL 列表 */
    V2Node nodes[512];
    int nodeCount;

    QProcess* subProc;
    QProcess* dlProc;
    QProcess* unzipProc;
    QProcess* coreProc;
    QByteArray subData;
    bool coreDlBusy;

    QLineEdit* subEdit;
    V2ListBox* subList;
    V2ListBox* nodeList;
    QLabel* coreState;
    QTextEdit* log;
    QPushButton* startBtn;
    QPushButton* stopBtn;
};

#endif
