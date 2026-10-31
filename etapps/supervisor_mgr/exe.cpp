
#include <qapplication.h>
#include <qwidget.h>

extern "C"
int main(int argc, char** argv) {
	QApplication app(argc, argv);
	
    extern QWidget* createSupervisorMgrMainWindow(void*);
    QWidget* w = createSupervisorMgrMainWindow(0);
	w->show();
	
	return app.exec();
}
