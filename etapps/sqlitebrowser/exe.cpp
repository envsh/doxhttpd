
#include <qapplication.h>
#include "LimeStyle.h"
#include "ThemeManager.h"
#include <qwidget.h>
#include <qstring.h>

extern "C"
int main(int argc, char** argv) {
    QApplication app(argc, argv);

    /* 默认深色主题（与 qltox 一致：LimeStyle + qtFusion dark） */
    app.setStyle(new LimeStyle);
    ThemeManager::setStyle("qtFusion", true);

    extern QWidget* createSqlBrowserMainWindow(void*);
    QWidget* w = createSqlBrowserMainWindow(0);
#ifdef QT3_BUILD
    app.setMainWidget(w);      /* 主窗关闭→app 退出（Qt3 机制） */
#endif
    w->show();
    if (argc > 1 && argv[1] && argv[1][0]) {
        extern void sbBrowserOpenEarly(const QString& path);
        sbBrowserOpenEarly(QString::fromLocal8Bit(argv[1]));
    }

    int rc = app.exec();
    delete w;                  /* 主窗析构 → destroyed 日志（exe 形态） */
    return rc;
}
