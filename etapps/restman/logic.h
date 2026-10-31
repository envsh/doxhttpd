#ifndef RESTMAN_LOGIC_H
#define RESTMAN_LOGIC_H

#include <qobject.h>
#include <qstring.h>
#include <qdatetime.h>

#ifdef QT3_BUILD
#include <qlistbox.h>
typedef QListBox RmListBox;
#else
#include <qlistwidget.h>
typedef QListWidget RmListBox;
#endif

class QTextEdit;
class QLabel;
class QComboBox;
class QLineEdit;
class QPushButton;
class QProcess;

/* 请求快照：收藏/历史共用的完整参数 */
struct RestManSnap {
    QString method;
    QString url;        /* 完整 URL（Query 由编辑区拼接，此处存原始输入） */
    QString query;      /* 每行 key=value */
    QString headers;    /* 每行 Key: Value */
    QString bodyType;   /* none / raw / urlencoded */
    QString bodyText;
    QString authType;   /* none / basic */
    QString authUser;
    QString authPass;
    QString createdAt;
};

#define RM_HIST_CAP  30
#define RM_BOOKMARK_CAP 256
#define RM_BASE_DIR_NAME  "restman"

class RestmanLogic : public QObject {
    Q_OBJECT
public:
    RestmanLogic(QObject* parent);
    void setWidgets(QComboBox* methodBox,
                    QLineEdit* urlEdit,
                    QTextEdit* queryEdit,
                    QTextEdit* headerEdit,
                    QComboBox* bodyTypeBox,
                    QTextEdit* bodyEdit,
                    QComboBox* authTypeBox,
                    QLineEdit* authUserEdit,
                    QLineEdit* authPassEdit,
                    RmListBox* histList,
                    RmListBox* bmList,
                    QLabel* respState,
                    QTextEdit* respBody,
                    QTextEdit* respHeaders,
                    QTextEdit* respRaw,
                    QPushButton* sendBtn);

public slots:
    void onSend();
    void onSaveBookmark();
    void onDelBookmark();
    void onLoadBookmark();
    void onDelHistory();
    void onLoadHistory();
    void onClearHistory();

private slots:
    void onReqOutput();
    void onReqErrOutput();
    void onReqDone();

private:
    QString baseDir() const;
    void ensureDirs();
    void appendLogWithPrefix(const QString& prefix, const QString& msg);
    void loadHistory();
    void saveHistory();
    void loadBookmarks();
    void saveBookmarks();
    void refreshHistList();
    void refreshBmList();
    void captureSnapshot(RestManSnap& snap);
    void applySnapshot(const RestManSnap& snap);
    void addHistory(const RestManSnap& snap);
    /* curl 构造 */
    bool buildCurlArgs(const RestManSnap& snap);
    QString buildFullUrl(const RestManSnap& snap, QString& err);

    RestManSnap histArr[RM_HIST_CAP];   /* [0] 最新 */
    int histCount;
    RestManSnap bmArr[RM_BOOKMARK_CAP];
    int bmCount;

    QProcess* reqProc;
    QByteArray reqStdout;
    QByteArray reqStderr;
    QTime reqTime;

    QComboBox* methodBox;
    QLineEdit* urlEdit;
    QTextEdit* queryEdit;
    QTextEdit* headerEdit;
    QComboBox* bodyTypeBox;
    QTextEdit* bodyEdit;
    QComboBox* authTypeBox;
    QLineEdit* authUserEdit;
    QLineEdit* authPassEdit;
    RmListBox* histList;
    RmListBox* bmList;
    QLabel* respState;
    QTextEdit* respBody;
    QTextEdit* respHeaders;
    QTextEdit* respRaw;
    QPushButton* sendBtn;

    int curHistIdx;
    int curBmIdx;
};

#endif
