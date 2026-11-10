
#include <qapplication.h>
#include <qwidget.h>

extern QWidget* createCpuMainWindow(void*);

extern "C"
int main(int argc, char** argv) {
	QApplication app(argc, argv);

    QWidget* w = createCpuMainWindow(0);
	w->show();

	return app.exec();
}
