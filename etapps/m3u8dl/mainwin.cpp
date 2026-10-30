#include "compat34.h"
#include "plugin_api.h"
#include <qlabel.h>
#include <qpushbutton.h>
#include <qlayout.h>
#include <qmessagebox.h>

extern "C"
QWidget* createM3u8dlMainWindow(void* parent) {
    QWidget* w = new QWidget((QWidget*)parent);
    QVBoxLayout* layout = new QVBoxLayout(w);

    QLabel* lbl = new QLabel("M3U8 Downloader", w);
    layout->addWidget(lbl);

#ifdef QT3_BUILD
    w->setCaption("M3U8 Downloader");
#else
    w->setWindowTitle("M3U8 Downloader");
#endif
    w->resize(300, 200);
    return w;

}
