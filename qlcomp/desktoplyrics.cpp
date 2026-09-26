#include "desktoplyrics.h"
#ifdef QT3_BUILD
#include <qdatetime.h>
#include <qpainter.h>
#include <qapplication.h>
#include <qpopupmenu.h>
#include <qcolordialog.h>
#include <qbitmap.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#else
#include <QDateTime>
#include <QPainter>
#include <QApplication>
#include <QDesktopWidget>
#include <QPropertyAnimation>
#include <QEasingCurve>
#include <QMenu>
#include <QColorDialog>
#include <QPainterPath>
#endif

static const int kLyricsBorder = 2;
static bool gLyricsBorderEnabled = false;

static void lrcTrim(QString& s)
{
    if (s.isEmpty()) return;
    int start = 0;
    while (start < (int)s.length() && s[start] == QChar(' ')) start++;
    int end = (int)s.length() - 1;
    while (end >= 0 && s[end] == QChar(' ')) end--;
    if (start > 0 || end < (int)s.length() - 1) {
        s = s.mid(start, end - start + 1);
    }
}

DesktopLyrics::DesktopLyrics()
    : QWidget(0
#ifdef QT3_BUILD
      , 0, WStyle_StaysOnTop | WStyle_Customize | WStyle_NoBorder
#else
      , Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint
#endif
      )
    , m_position(0)
    , m_currentLine(-1)
    , m_nextLine(-1)
    , m_animProgress(0.0f)
    , m_animTarget(0.0f)
    , m_animTimerId(0)
    , m_hoverTimerId(0)
#ifdef QT3_BUILD
    , m_backing(0)
#else
    , m_anim(0)
#endif
    , m_playedColor(0x00, 0xB4, 0xD8)
    , m_unplayedColor(0xBB, 0xBB, 0xBB)
    , m_strokeColor(0x00, 0x00, 0x00)
    , m_bgColor(0x00, 0x00, 0x00)
    , m_align(Qt::AlignCenter)
    , m_lineMode(LineMode::Single)
    , m_locked(false)
    , m_hovered(false)
    , m_transparentBg(true)
    , m_dragging(false)
{
    m_font.setPointSize(48);
    m_font.setBold(true);
#ifdef QT3_BUILD
    m_backing = new QPixmap(600, 80);
    m_backing->fill(Qt::color0);
#endif
    setupWindow();
    updateMinSize();
}

DesktopLyrics::~DesktopLyrics()
{
    if (m_animTimerId) {
        killTimer(m_animTimerId);
    }
    if (m_hoverTimerId) {
        killTimer(m_hoverTimerId);
    }
#ifdef QT3_BUILD
    delete m_backing;
#else
    delete m_anim;
#endif
}

void DesktopLyrics::setupWindow()
{
#ifdef QT3_BUILD
    setAutoMask(false);
    Display* dpy = QPaintDevice::x11Display();
    Window win = winId();
    if (m_transparentBg) {
        XSetWindowBackgroundPixmap(dpy, win, None);
    } else {
        clearMask();
        XShapeCombineMask(dpy, win, ShapeInput, 0, 0, None, ShapeSet);
        XSetWindowBackground(dpy, win, BlackPixel(dpy, DefaultScreen(dpy)));
    }
    {
        XEvent ev = {};
        ev.xclient.type = ClientMessage;
        ev.xclient.display = dpy;
        ev.xclient.window = win;
        ev.xclient.message_type = XInternAtom(dpy, "_NET_WM_STATE", False);
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
        ev.xclient.data.l[1] = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
        ev.xclient.data.l[2] = 0;
        ev.xclient.data.l[3] = 0;
        ev.xclient.data.l[4] = 0;
        XSendEvent(dpy, DefaultRootWindow(dpy), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &ev);
    }
    {
        Atom dock = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE_DOCK", False);
        Atom typeAtom = XInternAtom(dpy, "_NET_WM_WINDOW_TYPE", False);
        if (dock && typeAtom) {
            XChangeProperty(dpy, win, typeAtom, XA_ATOM, 32,
                            PropModeReplace, (unsigned char*)&dock, 1);
        }
    }
    XFlush(dpy);
#else
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setMouseTracking(true);
#endif
}

void DesktopLyrics::showLyrics()
{
    emit aboutToShow();    // 调用端在 slot 中通过 setSetting() 恢复配置
    QWidget::show();
    QRect screen = QApplication::desktop()->screenGeometry();
    move((screen.width() - width()) / 2, 50);
    raise();
    // show() 之后再次发送置顶请求（WM 可能在 map 时重置状态）
#ifdef QT3_BUILD
    {
        Display* dpy = QPaintDevice::x11Display();
        XEvent ev = {};
        ev.xclient.type = ClientMessage;
        ev.xclient.display = dpy;
        ev.xclient.window = winId();
        ev.xclient.message_type = XInternAtom(dpy, "_NET_WM_STATE", False);
        ev.xclient.format = 32;
        ev.xclient.data.l[0] = 1; // _NET_WM_STATE_ADD
        ev.xclient.data.l[1] = XInternAtom(dpy, "_NET_WM_STATE_ABOVE", False);
        ev.xclient.data.l[2] = 0;
        ev.xclient.data.l[3] = 0;
        ev.xclient.data.l[4] = 0;
        XSendEvent(dpy, DefaultRootWindow(dpy), False,
                   SubstructureRedirectMask | SubstructureNotifyMask, &ev);
        XFlush(dpy);
    }
#endif
}

void DesktopLyrics::hideLyrics()
{
    hide();
}

void DesktopLyrics::setLocked(bool locked)
{
    m_locked = locked;
}

void DesktopLyrics::toggleLocked()
{
    m_locked = !m_locked;
}

void DesktopLyrics::setFontSize(int pt)
{
    m_font.setPointSize(pt);
    updateMinSize();
    update();
}

void DesktopLyrics::setProgress(float p)
{
    m_animProgress = p;
    update();
}

void DesktopLyrics::setLineMode(LineMode mode)
{
    m_lineMode = mode;
    updateMinSize();
    update();
}

void DesktopLyrics::setPlaying(bool playing)
{
    if (!playing) {
        m_animProgress = 0.0f;
        m_animTarget = 0.0f;
        m_currentLine = -1;
        m_nextLine = -1;
        m_position = 0;
        update();
    }
}

// 备用解析器见 thirdparty/LrcParser.h（遇到 bug 时可替换）
void DesktopLyrics::parseLrc(const QString& content, std::vector<LrcLine>& out)
{
    out.clear();
    if (content.isEmpty()) return;

    // Detect three-digit centiseconds (e.g. [00:12.345])
    int ms10x = 10;
    for (int i = 0; i < (int)content.length() - 5; i++) {
        if (content[i] == QChar('.')
            && i + 4 < (int)content.length()
            && content[i+1].isDigit()
            && content[i+2].isDigit()
            && content[i+3].isDigit())
        {
            ms10x = 1;
            break;
        }
    }

    QStringList rawLines = qSplit(content, "\n");
    long long lastTime = 0;
    for (int li = 0; li < rawLines.size(); li++) {
        QString line = rawLines[li];
        lrcTrim(line);
        if (line.isEmpty()) continue;

        // No timestamp line (doesn't start with '[')
        if (line[0] != QChar('[')) {
            LrcLine lrc;
            lrc.time = -1;
            lrc.endTime = -1;
            lrc.text = line;
            out.push_back(lrc);
            continue;
        }

        std::vector<long long> startTimes;
        std::vector<long long> endTimes;
        int pos = 0;
        int len = (int)line.length();

        while (pos < len && line[pos] == QChar('[')) {
            int close = -1;
            for (int j = pos + 1; j < len; j++) {
                if (line[j] == QChar(']')) {
                    close = j;
                    break;
                }
            }
            if (close < 0) break;

            QString tag = line.mid(pos + 1, close - pos - 1);
            pos = close + 1;

            int colon = -1;
            for (int k = 0; k < (int)tag.length(); k++) {
                if (tag[k] == QChar(':')) {
                    colon = k;
                    break;
                }
            }
            if (colon < 0) continue;

            bool ok1 = false, ok2 = false;
            int min = tag.left(2).toInt(&ok1);
            int sec = tag.mid(3, 2).toInt(&ok2);
            if (!ok1 || !ok2) {
                LrcLine lrc;
                lrc.time = 0;
                lrc.endTime = -1;
                lrc.text = QString("[ERR] tag=") + tag + " ok1=" + (ok1?"1":"0") + " ok2=" + (ok2?"1":"0")
                           + " line=" + line;
                out.push_back(lrc);
                continue;
            }

            int frac = 0;
            int dot = -1;
            for (int k = colon + 1; k < (int)tag.length(); k++) {
                if (tag[k] == QChar('.')) {
                    dot = k;
                    break;
                }
            }
            if (dot >= 0) {
                QString fracStr = tag.mid(dot + 1);
                if (ms10x == 1) {
                    frac = fracStr.left(3).toInt();
                } else {
                    if (fracStr.length() >= 2) {
                        frac = fracStr.left(2).toInt();
                    } else if (fracStr.length() == 1) {
                        frac = fracStr.left(1).toInt() * 10;
                    }
                }
            }

            long long totalMs = (long long)min * 60000
                              + (long long)sec * 1000
                              + (ms10x == 1 ? (long long)frac : (long long)frac * 10);

            if (startTimes.empty()) {
                startTimes.push_back(totalMs);
            } else {
                endTimes.push_back(totalMs);
            }
        }

        if (startTimes.empty()) {
            LrcLine lrc;
            lrc.time = 0;
            lrc.endTime = -1;
            lrc.text = QString("[ERR] no timestamps in: ") + line;
            out.push_back(lrc);
            continue;
        }

        QString text = line.mid(pos);
        lrcTrim(text);
        if (text.isEmpty()) continue;

        for (size_t si = 0; si < startTimes.size(); si++) {
            LrcLine lrc;
            lrc.time = startTimes[si];
            lrc.text = text;
            if (si < endTimes.size()) {
                lrc.endTime = endTimes[si];
            } else {
                lrc.endTime = -1;
            }
            out.push_back(lrc);
            lastTime = startTimes[si];
        }
    }

    // sort by time
    for (size_t i = 0; i < out.size(); i++) {
        for (size_t j = i + 1; j < out.size(); j++) {
            if (out[j].time < out[i].time) {
                LrcLine tmp = out[i];
                out[i] = out[j];
                out[j] = tmp;
            }
        }
    }

    // Fill endTime for lines without one: use next line's start
    for (size_t i = 0; i + 1 < out.size(); i++) {
        if (out[i].endTime < 0) {
            out[i].endTime = out[i + 1].time;
        }
    }
}

// 装载 LRC 文本但不定位显示（load ≠ play 的播放器组件惯例）：
// 解析后 m_currentLine=-1，paintEvent 会画 "No Lyrics" 占位而非歌词；
// 必须随后调用 setPosition(msec) 定位活动行才开始渲染。
// 单行提示类用法（[00:00.000] 立即显示）请紧跟 setPosition(0)，
// 参照 showClockOnLyrics() 的写法。
void DesktopLyrics::setLrcText(const QString& lrcContent)
{
    m_lines.clear();
    parseLrc(lrcContent, m_lines);
    m_currentLine = -1;
    m_nextLine = -1;
    m_position = 0;
    m_animProgress = 0.0f;
    m_animTarget = 0.0f;
    update();
}

void DesktopLyrics::setPosition(long long msec)
{
    m_position = msec;
    if (m_lines.empty()) {
        m_currentLine = -1;
        m_nextLine = -1;
        m_animProgress = 0.0f;
        m_animTarget = 0.0f;
        update();
        return;
    }

    int idx = -1;
    for (size_t i = 0; i < m_lines.size(); i++) {
        if (m_lines[i].time < 0) {
            // no-timestamp line: keep the previous line index
            if (idx < 0) idx = 0;
            continue;
        }
        if (m_lines[i].time <= msec) {
            if (m_lines[i].endTime < 0 || msec <= m_lines[i].endTime) {
                idx = (int)i;
            }
        } else {
            break;
        }
    }

    if (idx < 0) idx = 0;
    if (idx != m_currentLine) {
        m_currentLine = idx;
        m_nextLine = -1;
        for (size_t i = (size_t)(idx + 1); i < m_lines.size(); i++) {
            if (m_lines[i].time >= 0) {
                m_nextLine = (int)i;
                break;
            }
        }
    }

    if (m_currentLine >= 0 && m_nextLine >= 0) {
        long long lineStart = m_lines[m_currentLine].time;
        long long lineEnd = m_lines[m_currentLine].endTime >= 0
                          ? m_lines[m_currentLine].endTime
                          : m_lines[m_nextLine].time;
        if (lineEnd > lineStart) {
            float target = (float)(msec - lineStart) / (float)(lineEnd - lineStart);
            if (target < 0.0f) target = 0.0f;
            if (target > 1.0f) target = 1.0f;
            m_animTarget = target;
#ifdef QT3_BUILD
            if (!m_animTimerId) {
                m_animTimerId = startTimer(30);
            }
#else
            if (m_anim) {
                m_anim->stop();
                delete m_anim;
                m_anim = 0;
            }
            m_anim = new QPropertyAnimation(this, "progress");
            m_anim->setDuration(250);
            m_anim->setStartValue(m_animProgress);
            m_anim->setEndValue(m_animTarget);
            m_anim->setEasingCurve(QEasingCurve::Linear);
            m_anim->start();
#endif
        } else {
            m_animProgress = 1.0f;
            m_animTarget = 1.0f;
            update();
        }
    } else if (m_currentLine >= 0) {
        m_animProgress = 1.0f;
        m_animTarget = 1.0f;
        update();
    } else {
        m_animProgress = 0.0f;
        m_animTarget = 0.0f;
        update();
    }
}

void DesktopLyrics::testShowTime()
{
    m_lines.clear();
    LrcLine lrc;
    lrc.time = 0;
    lrc.endTime = -1;

    QDate d = QDate::currentDate();
    QTime t = QTime::currentTime();
    time_t raw = time(0);
    struct tm* now = localtime(&raw);
    char tzBuf[16] = {};
    strftime(tzBuf, sizeof(tzBuf), "%z", now);
    static const char* weekDays[] = {
        "星期一","星期二","星期三","星期四","星期五","星期六","星期日"
    };
    QString ts = d.toString("yyyy-MM-dd") + " "
        + t.toString("hh:mm:ss") + " "
        + tzBuf + " "
        + qFromUtf8(weekDays[d.dayOfWeek() - 1]);
    lrc.text = ts;
    m_lines.push_back(lrc);
    m_currentLine = 0;
    m_nextLine = -1;
    m_position = 0;
    m_animProgress = 0.0f;
    m_animTarget = 0.0f;
    update();
}

void DesktopLyrics::drawTextWithStroke(QPainter& p, const QString& text,
                                       int x, int y, const QColor& fillColor)
{
    QPen oldPen = p.pen();
    p.setPen(m_strokeColor);
    for (int dx = -1; dx <= 1; dx++) {
        for (int dy = -1; dy <= 1; dy++) {
            if (dx == 0 && dy == 0) continue;
            p.drawText(x + dx, y + dy, text);
        }
    }
    p.setPen(fillColor);
    p.drawText(x, y, text);
    p.setPen(oldPen);
}

void DesktopLyrics::drawLine(QPainter& p, const QString& text, int x, int y, float progress)
{
    int textWidth = p.fontMetrics().width(text);

    drawTextWithStroke(p, text, x, y, m_unplayedColor);

    int playedWidth = (int)(textWidth * progress);
    if (playedWidth > 0 && playedWidth < textWidth) {
        p.save();
        p.setClipRect(x, y - 50, playedWidth, 100);
        drawTextWithStroke(p, text, x, y, m_playedColor);
        p.restore();
    } else if (playedWidth >= textWidth) {
        drawTextWithStroke(p, text, x, y, m_playedColor);
    }
}

void DesktopLyrics::paintEvent(QPaintEvent*)
{
#ifdef QT3_BUILD
    if (!m_backing || m_backing->size() != size()) {
        delete m_backing;
        m_backing = new QPixmap(size());
    }
    QPainter* pp = new QPainter(m_backing);
    QPainter& p = *pp;
#else
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
#endif

    QRect r = rect();

    bool hasText = false;
    int minX = width(), maxX = 0, minY = height(), maxY = 0;
    auto recordText = [&](int x, int y, int w, int h) {
        if (x < minX) minX = x; if (x + w > maxX) maxX = x + w;
        if (y < minY) minY = y; if (y + h > maxY) maxY = y + h;
        hasText = true;
    };

    if (m_lines.empty() || m_currentLine < 0 || m_currentLine >= (int)m_lines.size()) {
        QString msg = "No Lyrics";
        p.setFont(m_font);
        QFontMetrics fm = p.fontMetrics();
        int tw = fm.width(msg) + 16;
        int th = fm.height() + 8;
        int tx = (width() - tw) / 2;
        int ty = (height() - th) / 2;
        QRect bgR(tx, ty, tw, th);
        if (m_transparentBg) {
#ifdef QT3_BUILD
            p.setPen(QColor(0x88, 0x88, 0x88));
            p.drawText(bgR, Qt::AlignCenter, msg);
            if (gLyricsBorderEnabled) {
                p.setPen(QPen(m_playedColor, kLyricsBorder));
                p.setBrush(Qt::NoBrush);
                p.drawRoundRect(QRect(0, 0, width() - 1, height() - 1), 6, 6);
            }
            {
                QBitmap bm(size());
                bm.fill(Qt::color0);
                QPainter bp(&bm);
                bp.setFont(m_font);
                bp.setPen(Qt::color1);
                for (int dx = -1; dx <= 1; dx++)
                    for (int dy = -1; dy <= 1; dy++) {
                        QRect r2(bgR.x() + dx, bgR.y() + dy,
                                 bgR.width(), bgR.height());
                        bp.drawText(r2, Qt::AlignCenter, msg);
                    }
                if (gLyricsBorderEnabled) {
                    bp.setPen(QPen(Qt::color1, kLyricsBorder));
                    bp.setBrush(Qt::NoBrush);
                    bp.drawRoundRect(0, 0, width() - 1, height() - 1, 6, 6);
                }
                bp.end();
                Pixmap xm = (Pixmap)bm.handle();
                XShapeCombineMask(QPaintDevice::x11Display(), winId(),
                                  ShapeBounding, 0, 0, xm, ShapeSet);
                XShapeCombineMask(QPaintDevice::x11Display(), winId(),
                                  ShapeInput, 0, 0, xm, ShapeSet);
                XFlush(QPaintDevice::x11Display());
            }
#else
            QPainterPath path;
            path.addRoundedRect(bgR, 6, 6);
            p.fillPath(path, QColor(0, 0, 0, 180));
            p.setPen(QColor(0x88, 0x88, 0x88));
            p.drawText(bgR, Qt::AlignCenter, msg);
            if (gLyricsBorderEnabled) {
                p.save();
                p.setPen(QPen(QColor(m_playedColor.red(), m_playedColor.green(),
                                     m_playedColor.blue(), 110), kLyricsBorder));
                p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 6, 6);
                p.restore();
            }
#endif
        } else {
            p.setPen(QColor(0x88, 0x88, 0x88));
            p.drawText(bgR, Qt::AlignCenter, msg);
        }
#ifdef QT3_BUILD
        delete pp;
        bitBlt(this, 0, 0, m_backing);
#endif
        return;
    }

    p.setFont(m_font);
    QFontMetrics fm = p.fontMetrics();
    int fh = fm.height();
    bool showNext = (m_lineMode != LineMode::Single)
                    && m_nextLine >= 0
                    && m_nextLine < (int)m_lines.size();

    if (m_lineMode == LineMode::Single || !showNext) {
        const LrcLine& current = m_lines[m_currentLine];
        QString text = current.text;
        int textWidth = fm.width(text);
        int x = (width() - textWidth) / 2;
        if (x < 4) x = 4;
        int y = height() / 2 + fm.ascent() / 2;
        recordText(x, y - fm.ascent(), textWidth, fh);
        drawLine(p, text, x, y, m_animProgress);
    } else {
        const LrcLine& current = m_lines[m_currentLine];
        const LrcLine& next = m_lines[m_nextLine];
        QString curText = current.text;
        QString nextText = next.text;
        int curTextWidth = fm.width(curText);
        int nextTextWidth = fm.width(nextText);
        int curX = (width() - curTextWidth) / 2;
        if (curX < 4) curX = 4;
        int curY = height() / 4 + fh / 2;
        recordText(curX, curY - fm.ascent(), curTextWidth, fh);
        drawLine(p, curText, curX, curY, m_animProgress);
        int nextX = (width() - nextTextWidth) / 2;
        if (nextX < 4) nextX = 4;
        int nextY = height() * 3 / 4 + fh / 2;
        recordText(nextX, nextY - fm.ascent(), nextTextWidth, fh);
        drawTextWithStroke(p, nextText, nextX, nextY, m_unplayedColor);
    }

    // 透明模式：沿窗口边缘画 2px 圆角边框（Qt3 同时并入掩码，Qt4 直接画）
    if (m_transparentBg && gLyricsBorderEnabled) {
#ifdef QT3_BUILD
        p.setPen(QPen(m_playedColor, kLyricsBorder));
        p.setBrush(Qt::NoBrush);
        p.drawRoundRect(QRect(0, 0, width() - 1, height() - 1), 6, 6);
#else
        p.save();
        p.setPen(QPen(QColor(m_playedColor.red(), m_playedColor.green(),
                             m_playedColor.blue(), 110), kLyricsBorder));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 6, 6);
        p.restore();
#endif
    }

    // Background rect behind text (Qt4 only, opaque mode)
#ifndef QT3_BUILD
    if (!m_transparentBg && hasText) {
        QRect bgRect(minX - 8, minY - 4, maxX - minX + 16, maxY - minY + 8);
        bgRect = bgRect.intersected(r);
        QPainterPath path;
        path.addRoundedRect(bgRect, 6, 6);
        p.fillPath(path, QColor(0, 0, 0, 180));
    }
#endif

    // Hover overlay (opaque mode only)
    if (m_hovered && !m_transparentBg) {
#ifdef QT3_BUILD
        p.fillRect(0, 0, width(), 3, QColor(64, 64, 64));
#else
        p.save();
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(64, 64, 64, 32));
        p.drawRoundedRect(r.adjusted(2, 2, -2, -2), 5, 5);
        p.restore();
#endif
    }

#ifdef QT3_BUILD
    delete pp;
    bitBlt(this, 0, 0, m_backing);
    QBitmap bm(size());
    bm.fill(Qt::color0);
    {
        QPainter bp(&bm);
        bp.setFont(m_font);
        QFontMetrics bfm = bp.fontMetrics();
        int fh2 = bfm.height();
        bp.setPen(Qt::color1);
        auto maskAll = [&](int x, int y, const QString& t) {
            for (int dx = -1; dx <= 1; dx++)
                for (int dy = -1; dy <= 1; dy++)
                    bp.drawText(x + dx, y + dy, t);
        };
        if (m_lineMode == LineMode::Single
            || !(m_nextLine >= 0 && m_nextLine < (int)m_lines.size()))
        {
            const QString& t = m_lines[m_currentLine].text;
            int tw = bfm.width(t);
            int x = (width() - tw) / 2;
            if (x < 4) x = 4;
            int y = height() / 2 + bfm.ascent() / 2;
            maskAll(x, y, t);
        } else {
            const QString& ct = m_lines[m_currentLine].text;
            const QString& nt = m_lines[m_nextLine].text;
            int cw = bfm.width(ct), nw = bfm.width(nt);
            int cx = (width() - cw) / 2;
            if (cx < 4) cx = 4;
            int cy = height() / 4 + fh2 / 2;
            maskAll(cx, cy, ct);
            int nx = (width() - nw) / 2;
            if (nx < 4) nx = 4;
            int ny = height() * 3 / 4 + fh2 / 2;
            maskAll(nx, ny, nt);
        }
        if (m_transparentBg && gLyricsBorderEnabled) {
            bp.setPen(QPen(Qt::color1, kLyricsBorder));
            bp.setBrush(Qt::NoBrush);
            bp.drawRoundRect(0, 0, width() - 1, height() - 1, 6, 6);
        }
    }
    Pixmap xm = (Pixmap)bm.handle();
    XShapeCombineMask(QPaintDevice::x11Display(), winId(),
                      ShapeBounding, 0, 0, xm, ShapeSet);
    XShapeCombineMask(QPaintDevice::x11Display(), winId(),
                      ShapeInput, 0, 0, xm, ShapeSet);
    XFlush(QPaintDevice::x11Display());
#endif
}

void DesktopLyrics::resizeEvent(QResizeEvent*)
{
#ifdef QT3_BUILD
    if (m_backing) {
        delete m_backing;
        m_backing = new QPixmap(size());
    }
#endif
}

void DesktopLyrics::mousePressEvent(QMouseEvent* e)
{
    if (m_locked) {
        if (!m_lines.empty() && m_currentLine >= 0 && m_currentLine < (int)m_lines.size()) {
            emit positionClicked(m_lines[m_currentLine].time);
        }
        return;
    }
    m_dragging = true;
#ifdef QT3_BUILD
    m_dragStart = QPoint(e->globalX() - geometry().x(),
                         e->globalY() - geometry().y());
#else
    m_dragStart = e->globalPos() - geometry().topLeft();
#endif
}

void DesktopLyrics::mouseMoveEvent(QMouseEvent* e)
{
    if (!m_dragging || m_locked) return;
#ifdef QT3_BUILD
    move(e->globalX() - m_dragStart.x(),
         e->globalY() - m_dragStart.y());
#else
    move(e->globalPos() - m_dragStart);
#endif
}

void DesktopLyrics::mouseReleaseEvent(QMouseEvent*)
{
    m_dragging = false;
}

void DesktopLyrics::enterEvent(QEvent*)
{
    if (m_hoverTimerId) {
        killTimer(m_hoverTimerId);
        m_hoverTimerId = 0;
    }
    m_hovered = true;
    update();
}

void DesktopLyrics::leaveEvent(QEvent*)
{
    if (m_hoverTimerId) {
        killTimer(m_hoverTimerId);
    }
    m_hoverTimerId = startTimer(300);
}

void DesktopLyrics::timerEvent(QTimerEvent* e)
{
    int id = e->timerId();
#ifdef QT3_BUILD
    if (id == m_animTimerId) {
        float diff = m_animTarget - m_animProgress;
        if (diff < 0.0f) diff = -diff;
        if (diff < 0.005f) {
            m_animProgress = m_animTarget;
            if (m_animTimerId) {
                killTimer(m_animTimerId);
                m_animTimerId = 0;
            }
        } else {
            m_animProgress += (m_animTarget - m_animProgress) * 0.15f;
        }
        update();
        return;
    }
#endif
    if (id == m_hoverTimerId) {
        killTimer(m_hoverTimerId);
        m_hoverTimerId = 0;
        m_hovered = false;
        update();
    }
}

void DesktopLyrics::contextMenuEvent(QContextMenuEvent* e)
{
#ifdef QT3_BUILD
    QPopupMenu menu;

    QPopupMenu* lineMenu = new QPopupMenu(&menu);
    int idSingle = lineMenu->insertItem(qFromUtf8("单行"), 200);
    int idDouble = lineMenu->insertItem(qFromUtf8("双行"), 201);
    int idSuit = lineMenu->insertItem(qFromUtf8("自适应"), 202);
    if (m_lineMode == LineMode::Single) lineMenu->setItemChecked(idSingle, true);
    else if (m_lineMode == LineMode::Double) lineMenu->setItemChecked(idDouble, true);
    else if (m_lineMode == LineMode::Suitable) lineMenu->setItemChecked(idSuit, true);

    QPopupMenu* fontSizeMenu = new QPopupMenu(&menu);
    fontSizeMenu->setMinimumWidth(120);
    for (int sz = 10; sz <= 50; sz += 5) {
        int id = fontSizeMenu->insertItem(QString::number(sz), 1000 + sz);
        if (sz == m_font.pointSize()) fontSizeMenu->setItemChecked(id, true);
    }

    int idPlayed = menu.insertItem(qFromUtf8("已播放颜色"), 300);
    int idUnplayed = menu.insertItem(qFromUtf8("未播放颜色"), 301);
    int idStroke = menu.insertItem(qFromUtf8("描边颜色"), 302);
    menu.insertSeparator();
    menu.insertItem(qFromUtf8("行模式"), lineMenu);
    menu.insertItem(qFromUtf8("字号"), fontSizeMenu);
    menu.insertSeparator();
    int idTrans = menu.insertItem(qFromUtf8("透明模式"), 303);
    if (m_transparentBg) menu.setItemChecked(idTrans, true);
    menu.insertSeparator();
    int idLock = menu.insertItem(qFromUtf8("锁定位置"), 305);
    if (m_locked) menu.setItemChecked(idLock, true);
    menu.insertSeparator();
    int idHide = menu.insertItem(qFromUtf8("隐藏"), 304);

    int choice = menu.exec(e->globalPos());

    if (choice == 300) {
        QColor c = QColorDialog::getColor(m_playedColor, this);
        if (c.isValid()) { m_playedColor = c; update(); }
    } else if (choice == 301) {
        QColor c = QColorDialog::getColor(m_unplayedColor, this);
        if (c.isValid()) { m_unplayedColor = c; update(); }
    } else if (choice == 302) {
        QColor c = QColorDialog::getColor(m_strokeColor, this);
        if (c.isValid()) { m_strokeColor = c; update(); }
    } else if (choice == 200) {
        setLineMode(LineMode::Single);
    } else if (choice == 201) {
        setLineMode(LineMode::Double);
    } else if (choice == 202) {
        setLineMode(LineMode::Suitable);
    } else if (choice >= 1010 && choice <= 1040) {
        setFontSize(choice - 1000);
    } else if (choice == 303) {
        m_transparentBg = !m_transparentBg;
        setupWindow();
        update();
    } else if (choice == 305) {
        m_locked = !m_locked;
    } else if (choice == 304) {
        hide();
        emit hideRequested();
    }
#else
    QMenu menu;
    QMenu* lineMenu = menu.addMenu("行模式");
    QAction* actSingle = lineMenu->addAction("单行");
    QAction* actDouble = lineMenu->addAction("双行");
    QAction* actSuit = lineMenu->addAction("自适应");
    actSingle->setCheckable(true); actSingle->setChecked(m_lineMode == LineMode::Single);
    actDouble->setCheckable(true); actDouble->setChecked(m_lineMode == LineMode::Double);
    actSuit->setCheckable(true); actSuit->setChecked(m_lineMode == LineMode::Suitable);

    QMenu* fontSizeMenu = menu.addMenu("字号");
    QList<QAction*> fontActions;
    for (int sz = 10; sz <= 50; sz += 5) {
        QAction* a = fontSizeMenu->addAction(QString::number(sz));
        a->setCheckable(true);
        a->setChecked(sz == m_font.pointSize());
        a->setData(sz);
        fontActions.append(a);
    }

    QAction* actPlayed = menu.addAction("已播放颜色");
    QAction* actUnplayed = menu.addAction("未播放颜色");
    QAction* actStroke = menu.addAction("描边颜色");
    menu.addSeparator();
    QAction* actTrans = menu.addAction("透明模式");
    actTrans->setCheckable(true);
    actTrans->setChecked(m_transparentBg);
    QAction* actLock = menu.addAction("锁定位置");
    actLock->setCheckable(true);
    actLock->setChecked(m_locked);
    menu.addSeparator();
    QAction* actHide = menu.addAction("隐藏");

    QAction* chosen = menu.exec(e->globalPos());
    if (!chosen) return;

    if (chosen == actPlayed) {
        QColor c = QColorDialog::getColor(m_playedColor, this, "选择已播放颜色");
        if (c.isValid()) { m_playedColor = c; update(); }
    } else if (chosen == actUnplayed) {
        QColor c = QColorDialog::getColor(m_unplayedColor, this, "选择未播放颜色");
        if (c.isValid()) { m_unplayedColor = c; update(); }
    } else if (chosen == actStroke) {
        QColor c = QColorDialog::getColor(m_strokeColor, this, "选择描边颜色");
        if (c.isValid()) { m_strokeColor = c; update(); }
    } else if (chosen == actSingle) {
        setLineMode(LineMode::Single);
    } else if (chosen == actDouble) {
        setLineMode(LineMode::Double);
    } else if (chosen == actSuit) {
        setLineMode(LineMode::Suitable);
    } else if (fontActions.contains(chosen)) {
        int sz = chosen->data().toInt();
        setFontSize(sz);
    } else if (chosen == actTrans) {
        m_transparentBg = !m_transparentBg;
        setupWindow();
        update();
    } else if (chosen == actLock) {
        m_locked = !m_locked;
    } else if (chosen == actHide) {
        hide();
        emit hideRequested();
    }
#endif
}
/* 使用示例：
   lyrics->setSetting(LyricsSettings()
       .withPlayedColor("#FF0000")
       .withFontSize(32)
       .withX(100).withY(200));
   未指定的字段保持当前值不变。
*/
void DesktopLyrics::setSetting(const LyricsSettings& s)
{
    m_playedColor   = s.playedColor();
    m_unplayedColor = s.unplayedColor();
    m_strokeColor   = s.strokeColor();
    m_bgColor       = s.bgColor();
    m_lineMode      = s.lineMode();
    m_transparentBg = s.transparentBg();
    m_locked        = s.locked();
    setFontSize(s.fontSize());
    setupWindow();

    int x = s.windowX(), y = s.windowY();
    if (x >= 0 && y >= 0) move(x, y);

    update();
}

void DesktopLyrics::updateMinSize()
{
    QFontMetrics fm(m_font);
    int fh = fm.height() + 16;
    int lines = (m_lineMode == LineMode::Single) ? 2 : 4;
    setMinimumSize(200, fh * lines / 2);
    setMaximumSize(800, fh * lines);
    updateGeometry();
}
