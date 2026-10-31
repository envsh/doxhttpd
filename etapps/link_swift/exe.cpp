
#include <qapplication.h>
#include <qwidget.h>

// #include "mainwin.h"

extern "C"
int main(int argc, char** argv) {
	QApplication app(argc, argv);

    extern QWidget* createLinkSwiftMainWindow(void*);
    QWidget* w = createLinkSwiftMainWindow(0);
	w->show();

	return app.exec();
}
