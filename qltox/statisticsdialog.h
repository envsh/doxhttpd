#ifndef STATISTICSDIALOG_H
#define STATISTICSDIALOG_H

#include "compat34.h"
#include <qdialog.h>
#include <vector>

class QPushButton;
class QLabel;
class StatsWorker;
class QCloseEvent;

class StatisticsDialog : public QDialog {
    Q_OBJECT
public:
    explicit StatisticsDialog(QWidget* parent = nullptr);
    ~StatisticsDialog();

private slots:
    void refreshStats();
    void openDataDir();
    void copyStats();
    void onTabClicked();

protected:
    void customEvent(CustomEventBase* event);
    void closeEvent(QCloseEvent* e);

private:
    void buildOverviewPage(QWidget* inner);
    void buildResultGrid(QWidget* host);
    QString dataDirText() const;
    void rebuildResultText();

    StackedWidget* m_pageStack = nullptr;
    QWidget* m_tabBar = nullptr;
    std::vector<QPushButton*> m_tabButtons;
    std::vector<QWidget*> m_pages;
    QLabel* m_dirLabel = nullptr;
    QWidget* m_resultBox = nullptr;
    std::vector<QLabel*> m_valueLabels;
    QString m_resultText;
    QPushButton* m_calcBtn = nullptr;
    QLabel* m_progressLabel = nullptr;
    StatsWorker* m_worker = nullptr;
    std::vector<int64_t> m_vals;
    TimePoint m_statStart = timeNow();
};

#endif