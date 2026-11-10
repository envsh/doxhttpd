
#include <qapplication.h>
#include <qwidget.h>

extern QWidget* createIstatMainWindow(void*);

extern "C"
int main(int argc, char** argv) {
	QApplication app(argc, argv);

    QWidget* w = createIstatMainWindow(0);
	w->show();

	return app.exec();
}