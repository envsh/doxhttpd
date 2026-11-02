
#include <qapplication.h>
#include <qwidget.h>
#include <qstring.h>

extern "C"
int main(int argc, char** argv) {
    QApplication app(argc, argv);

    extern QWidget* createSqlBrowserMainWindow(void*);
    QWidget* w = createSqlBrowserMainWindow(0);
    w->show();
    if (argc > 1 && argv[1] && argv[1][0]) {
        extern void sbBrowserOpenEarly(const QString& path);
        sbBrowserOpenEarly(QString::fromLocal8Bit(argv[1]));
    }

    return app.exec();
}
