
#include <qapplication.h>
#include <qwidget.h>

// #include "mainwin.h"

extern "C"
int main(int argc, char** argv) {
	QApplication app(argc, argv);

    extern QWidget* createBookmarkMgrMainWindow(void*);
    QWidget* w = createBookmarkMgrMainWindow(0);
	w->show();

	return app.exec();
}
