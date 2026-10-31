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
#include <qptrlist.h>
#include <qlistbox.h>
#include <qmap.h>

#ifdef QT3_BUILD
typedef QListBox RmListBox;
#else
class QListWidget;
typedef QListWidget RmListBox;
#endif


struct SupProcInfo {
    QString name;
    QString group;
    int pid;
    QString state;
    int statename;
    QString description;
    QString uptime;
};

class SupervisorMgrLogic : public QObject {
    Q_OBJECT
public:
    SupervisorMgrLogic(QWidget* parent = 0);
    void setWidgets(QLineEdit* host, QLineEdit* port, QLineEdit* user, QLineEdit* pass,
                    RmListBox* procList, QLabel* statusLabel,
                    QPushButton* refreshBtn, QPushButton* startBtn, QPushButton* stopBtn, QPushButton* restartBtn, QPushButton* clearLogBtn,
                    QTextEdit* logEdit);

public slots:
    void onConnect();
    void onRefresh();
    void onStart();
    void onStop();
    void onRestart();
    void onClearLog();
    void onProcSelected();
    void onProcDone();

private:
    void loadConfig();
    void saveConfig();
    QString configPath();
    QString rpcUrl();
    QString authArgs();
    void sendRpc(const QString& method, const QStringList& params, const QString& tag);
    void parseProcList(const QString& xml);
    void appendLog(const QString& msg);
    QString stateText(int code);

    QWidget* m_parent;
    QLineEdit* m_host;
    QLineEdit* m_port;
    QLineEdit* m_user;
    QLineEdit* m_pass;
    RmListBox* m_procList;
    QLabel* m_statusLabel;
    QPushButton* m_refreshBtn;
    QPushButton* m_startBtn;
    QPushButton* m_stopBtn;
    QPushButton* m_restartBtn;
    QPushButton* m_clearLogBtn;
    QTextEdit* m_logEdit;
    QPtrList<SupProcInfo> m_procs;
    int m_selected;
    QMap<QProcess*, QString> m_rpcTags;
};

#endif
