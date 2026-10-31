#ifndef SUPERVISOR_MGR_LOGIC_H
#define SUPERVISOR_MGR_LOGIC_H

#include <qobject.h>
#include <qstring.h>
#include <qstringlist.h>
#include <qprocess.h>
#include <qtimer.h>
#include <qdatetime.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qtextedit.h>
#include <qlabel.h>
#include <qpushbutton.h>
#ifdef QT3_BUILD
#include <q3ptrlist.h>
typedef Q3PtrList<SupProcInfo> SupProcList;
#else
#include <qlist.h>
typedef QList<SupProcInfo*> SupProcList;
#endif
#include <qmap.h>

#ifdef QT3_BUILD
#include <qcolor.h>
#include <qtable.h>
#include <qheader.h>
typedef QTable RmProcTable;
#else
#include <qcolor.h>
#include <qbrush.h>
#include <qtablewidget.h>
typedef QTableWidget RmProcTable;
#endif

struct SupProcInfo {
    QString name;
    QString group;
    int state;          /* 状态码 int（supervisord "state"，如 20） */
    QString statename;  /* 状态字符串（supervisord "statename"，如 RUNNING） */
    int pid;
    QString description;
    long start, stop, now;  /* 时间戳，用于计算 uptime */
    SupProcInfo() : state(-1), pid(0), start(0), stop(0), now(0) {}
};

class SupervisorMgrLogic : public QObject {
    Q_OBJECT
public:
    SupervisorMgrLogic(QWidget* parent = 0);
    void setWidgets(QLineEdit* host, QLineEdit* port, QLineEdit* user, QLineEdit* pass,
                    RmProcTable* procTable, QLabel* statusLabel,
                    QPushButton* refreshBtn, QPushButton* startBtn, QPushButton* stopBtn, QPushButton* restartBtn, QPushButton* clearLogBtn,
                    QTextEdit* logEdit);

public slots:
    void onConnect();
    void onRefresh();
    void onStart();
    void onStop();
    void onRestart();
    void onRestartPhase2();
    void onClearLog();
    void onProcSelected();
    void onProcDone();

private:
    void loadConfig();
    void saveConfig();
    QString configPath();
    QString rpcUrl();
    void sendRpc(const QString& method, const QStringList& params, const QString& tag);
    void parseProcList(const QString& xml);
    void clearProcTable();
    void addProcRow(const SupProcInfo* info, int row);
    void appendLog(const QString& msg);
    QString stateText(int code);
    void disableOps();
    void setStatusColor(const QColor& c);

    QWidget* m_parent;
    QLineEdit* m_host;
    QLineEdit* m_port;
    QLineEdit* m_user;
    QLineEdit* m_pass;
    RmProcTable* m_procTable;
    QLabel* m_statusLabel;
    QPushButton* m_refreshBtn;
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_restartBtn;
    QPushButton* m_clearLogBtn;
    QTextEdit* m_logEdit;
    SupProcList m_procs;
    int m_selected;
    QMap<QProcess*, QString> m_rpcTags;
};

#endif
