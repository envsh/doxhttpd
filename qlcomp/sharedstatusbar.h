#ifndef SHAREDSTATUSBAR_H
#define SHAREDSTATUSBAR_H

// Compatible with Qt3 (buildqt3.sh) and Qt4 (buildqt4.sh)

#ifdef QT3_BUILD
#include <qwidget.h>
#include <qstatusbar.h>
#include <qapplication.h>
#include <qrect.h>
#include <qpoint.h>
#include <qlayout.h>
#include <qtimer.h>
#include <qtoolbutton.h>
#include <qlabel.h>
#include <qvaluelist.h>
#else
#include <QWidget>
#include <QStatusBar>
#include <QMouseEvent>
#include <QApplication>
#include <QHBoxLayout>
#include <QTimer>
#include <QPointer>
#include <QToolButton>
#include <QLabel>
#include <QList>
#endif

// 消息类型（数值对齐 SticonIcon / SystemTrayIcon::MessageIcon：1/2/3）
enum StatusMessageType {
	StatusInfo    = 1,
	StatusWarning = 2,
	StatusError   = 3
};

struct StatusHistoryEntry
{
    QString timeStr;
    QString text;
    StatusMessageType type;
};

#ifdef QT3_BUILD
typedef QValueList<StatusHistoryEntry> StatusHistoryList;
#else
typedef QList<StatusHistoryEntry> StatusHistoryList;
#endif

class SharedStatusBar : public QWidget
{
    Q_OBJECT
public:
    static SharedStatusBar *instance();
    static bool instanceExists();

    void showMessage(const QString &msg, int timeout = 0);
    void showMessageTyped(const QString &msg, StatusMessageType type, int timeout = 0);
    void clearMessage();
    void addWidget(QWidget *w, int stretch = 0);
    void addPermanentWidget(QWidget *w, int stretch = 0);
    void removeWidget(QWidget *w);

private:
    SharedStatusBar();
    ~SharedStatusBar();
    static SharedStatusBar *s_instance;

    QStatusBar *m_bar;
#ifdef QT3_BUILD
    QWidget    *m_activeWindow;
#else
    QPointer<QWidget> m_activeWindow;
#endif
    bool        m_dragging;
    bool        m_repositioning;
    QPoint      m_dragStartGlobal;
    QRect       m_windowStartGeo;
    QToolButton *m_historyBtn;
    QLabel       *m_iconLbl;
    QTimer       *m_iconTimer;
    StatusHistoryList m_history;

    bool isInGripArea(const QPoint &localPos) const;
    void handleGripPress(const QPoint &globalPos);
    void handleGripDrag(const QPoint &globalPos);
    void handleGripRelease();
    void reposition();
    void showHistoryMenu();
    void doMessage(const QString &msg, int timeout,
                   StatusMessageType type, bool typed);

    void paintEvent(QPaintEvent *e) override;
    bool event(QEvent *e) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onFocusChanged(QWidget *old, QWidget *now);
    void onHistoryClicked();
    void onIconTimeout();

private:
    void installEventFiltersOnMyselfTopLevelWidgets();

#ifdef QT3_BUILD
private slots:
    void onDebounceTimeout();

private:
    QTimer *m_debounceTimer;
    bool m_pendingHide;
#endif

private slots:
    void retrack();
};

#endif
