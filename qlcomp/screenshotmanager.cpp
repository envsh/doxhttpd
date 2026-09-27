#include "screenshotmanager.h"
#include "screenshotoverlay.h"
#include "compat34.h"

#ifdef QT3_BUILD
#include <qdesktopwidget.h>
#include <qdir.h>
#include <qbitmap.h>
#else
#if QT_VERSION < 0x060000
#include <QDesktopWidget>  // Qt6 移除 QDesktopWidget（改用 QScreen）
#endif
#include <QDir>
#endif

#ifdef Q_WS_X11
#include <X11/Xlib.h>
#endif

#include <qapplication.h>

static ScreenshotManager* g_instance = nullptr;
static int s_screenshotCounter = 0;

ScreenshotManager* ScreenshotManager::instance() {
    if (!g_instance) {
        g_instance = new ScreenshotManager();
    }
    return g_instance;
}

ScreenshotManager::ScreenshotManager()
    : QObject(nullptr)
    , m_hideWindow(nullptr)
    , m_autoHide(false)
    , m_delayMs(0)
{
}

ScreenshotManager::~ScreenshotManager() {
    g_instance = nullptr;
}

QPixmap ScreenshotManager::grabScreen() {
    // Qt3: https://doc.qt.io/archives/3.3/qpixmap.html#grabWindow
    // Qt4: https://doc.qt.io/archives/4.2/qpixmap.html#grabWindow
    // Qt6 移除了 QPixmap::grabWindow，等价能力迁到 QScreen::grabWindow
    return qGrabWholeScreen();
}

void ScreenshotManager::captureFullScreen() {
    if (m_autoHide && m_hideWindow) {
        m_hideWindow->hide();
    }

    if (m_delayMs > 0) {
        QTimer::singleShot(m_delayMs, this, SLOT(doCaptureFullScreen()));
    } else {
        doCaptureFullScreen();
    }
}

void ScreenshotManager::doCaptureFullScreen() {
    QPixmap result = grabScreen();

    if (m_autoHide && m_hideWindow) {
        m_hideWindow->show();
    }

    finishCapture(result);
}

void ScreenshotManager::captureActiveWindow() {
    if (m_autoHide && m_hideWindow) {
        m_hideWindow->hide();
    }

    if (m_delayMs > 0) {
        QTimer::singleShot(m_delayMs, this, SLOT(doCaptureActiveWindow()));
    } else {
        doCaptureActiveWindow();
    }
}

void ScreenshotManager::doCaptureActiveWindow() {
    QPixmap result;

#ifdef Q_WS_X11
    // XGetInputFocus: 获取当前焦点窗口
    // Reference: https://www.x.org/releases/X11R7.7/doc/man/man3/XGetInputFocus.3.xhtml
    Display* dpy = XOpenDisplay(nullptr);
    if (dpy) {
        Window focused;
        int revert;
        XGetInputFocus(dpy, &focused, &revert);
        if (focused != None && focused != DefaultRootWindow(dpy)) {
            result = QPixmap::grabWindow(focused);
        }
        XCloseDisplay(dpy);
    }
#endif

    // X11 未获取到时回退全屏
    if (result.isNull()) {
        result = grabScreen();
    }

    if (m_autoHide && m_hideWindow) {
        m_hideWindow->show();
    }

    finishCapture(result);
}

void ScreenshotManager::captureRegion() {
    if (m_autoHide && m_hideWindow) {
        m_hideWindow->hide();
    }

    if (m_delayMs > 0) {
        QTimer::singleShot(m_delayMs, this, SLOT(doCaptureForRegion()));
    } else {
        doCaptureForRegion();
    }
}

void ScreenshotManager::doCaptureForRegion() {
    QPixmap fullPm = grabScreen();

    if (m_autoHide && m_hideWindow) {
        m_hideWindow->show();
    }

    // 弹出 ScreenshotRegionSelector 全屏覆盖选区
    ScreenshotRegionSelector* selector = new ScreenshotRegionSelector(fullPm);
    connect(selector, SIGNAL(regionSelected(const QRect&, const QPixmap&)),
            this, SLOT(onRegionSelected(const QRect&, const QPixmap&)));
    connect(selector, SIGNAL(cancelled()), this, SIGNAL(cancelled()));
    selector->show();
}

void ScreenshotManager::onRegionSelected(const QRect& rect, const QPixmap& fullPixmap) {
    // Qt3: QRect::normalize() is void (in-place), QPixmap::copy(x,y,w,h)
    // Qt4: QRect::normalized() returns copy, QPixmap::copy(QRect)
#ifdef QT3_BUILD
    QRect r = rect;
    r.normalize();
    // Qt3: bitBlt() for sub-rectangle extraction; QPixmap::copy() only supports bool ignoreMask
    // Reference: https://doc.qt.io/archives/3.3/qpixmap.html#bitBlt
    QPixmap cropped(r.width(), r.height());
    bitBlt(&cropped, 0, 0, &fullPixmap, r.x(), r.y(), r.width(), r.height());
#else
    QRect r = rect.normalized();
    QPixmap cropped = fullPixmap.copy(r);
#endif
    finishCapture(cropped);
}

void ScreenshotManager::finishCapture(const QPixmap& result) {
    if (result.isNull()) {
        emit cancelled();
        return;
    }
    QString path = saveToTempFile(result);
    emit screenshotReady(path);
}

QString ScreenshotManager::saveToTempFile(const QPixmap& pixmap) {
    // 与 qltox/messageinput.cpp:202 同样的临时文件命名模式
    QString tmpPath;
#ifdef QT3_BUILD
    tmpPath.sprintf("/tmp/toxhttpd_screenshot_%d.png", ++s_screenshotCounter);
#else
    tmpPath = QString("/tmp/toxhttpd_screenshot_%1.png").arg(++s_screenshotCounter);
#endif
    pixmap.save(tmpPath, "PNG");
    return tmpPath;
}
