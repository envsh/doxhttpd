
#include <qapplication.h>
#include <qwidget.h>

// #include "mainwin.h"

extern "C"
int main(int argc, char** argv) {
	QApplication app(argc, argv);

    extern QWidget* createM3u8dlMainWindow(void*);
    QWidget* w = createM3u8dlMainWindow(0);
	w->show();

	return app.exec();
}
