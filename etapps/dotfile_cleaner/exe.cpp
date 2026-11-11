
#include <qapplication.h>
#include "ThemeManager.h"
#include <qwidget.h>

// #include "mainwin.h"

extern "C"
int main(int argc, char** argv) {
	QApplication app(argc, argv);

    /* 默认深色主题：只用深色调色板（LimeStyle 会导致 QTabWidget 点击失效，不安装） */
    ThemeManager::setStyle("qtFusion", true);

    extern QWidget* createDotfileCleanerMainWindow(void*);
    QWidget* w = createDotfileCleanerMainWindow(0);
#ifdef QT3_BUILD
	app.setMainWidget(w);      /* 主窗关闭→app 退出（Qt3 机制） */
#endif
	w->show();

	int rc = app.exec();
	delete w;                  /* 主窗析构 → destroyed 日志（exe 形态） */
	return rc;
}
