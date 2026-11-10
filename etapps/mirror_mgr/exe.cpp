
#include <qapplication.h>
#include <qwidget.h>

// #include "mainwin.h"

extern "C"
int main(int argc, char** argv) {
	QApplication app(argc, argv);

    extern QWidget* createMirrorMgrMainWindow(void*);
    QWidget* w = createMirrorMgrMainWindow(0);
#ifdef QT3_BUILD
	app.setMainWidget(w);      /* 主窗关闭→app 退出（Qt3 机制） */
#endif
	w->show();

	int rc = app.exec();
	delete w;                  /* 主窗析构 → destroyed 日志（exe 形态） */
	return rc;
}
