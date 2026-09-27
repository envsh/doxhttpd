#ifndef DESKTOP_LYRICS_H
#define DESKTOP_LYRICS_H

// Reference: shinyawhy/QooMusicPlayer DesktopLyricWidget
// https://github.com/shinyawhy/QooMusicPlayer/blob/master/desktoplyricwidget.cpp

#include "compat34.h"
#include <qstring.h>
#include <qcolor.h>
#include <qfont.h>
#include <qpoint.h>
#include <qnamespace.h>
#include <qpair.h>
#include <vector>

struct LrcLine {
    long long time;
    long long endTime;
    QString text;
};

enum class LineMode {
    Single,
    Double,
    Suitable
};

class LyricsSettings {
public:
    LyricsSettings() = default;

    LyricsSettings& withPlayedColor(const QColor& c)   { m_playedColor = c;   return *this; }
    LyricsSettings& withUnplayedColor(const QColor& c) { m_unplayedColor = c; return *this; }
    LyricsSettings& withStrokeColor(const QColor& c)   { m_strokeColor = c;   return *this; }
    LyricsSettings& withBgColor(const QColor& c)       { m_bgColor = c;       return *this; }
    LyricsSettings& withFontSize(int pt)                { m_fontSize = pt;    return *this; }
    LyricsSettings& withLineMode(LineMode m)            { m_lineMode = m;     return *this; }
    LyricsSettings& withTransparentBg(bool b)           { m_transparentBg = b;return *this; }
    LyricsSettings& withLocked(bool b)                  { m_locked = b;       return *this; }
    LyricsSettings& withX(int x)                        { m_windowX = x;      return *this; }
    LyricsSettings& withY(int y)                        { m_windowY = y;      return *this; }

    const QColor& playedColor()   const { return m_playedColor; }
    const QColor& unplayedColor() const { return m_unplayedColor; }
    const QColor& strokeColor()   const { return m_strokeColor; }
    const QColor& bgColor()       const { return m_bgColor; }
    int  fontSize()               const { return m_fontSize; }
    LineMode lineMode()           const { return m_lineMode; }
    bool transparentBg()          const { return m_transparentBg; }
    bool locked()                 const { return m_locked; }
    int  windowX()                const { return m_windowX; }
    int  windowY()                const { return m_windowY; }

private:
    QColor m_playedColor   = QColor(0x00, 0xB4, 0xD8);
    QColor m_unplayedColor = QColor(0xBB, 0xBB, 0xBB);
    QColor m_strokeColor   = QColor(0x00, 0x00, 0x00);
    QColor m_bgColor       = QColor(0x00, 0x00, 0x00);
    int    m_fontSize      = 48;
    LineMode m_lineMode    = LineMode::Single;
    bool   m_transparentBg = true;
    bool   m_locked        = false;
    int    m_windowX       = -1;
    int    m_windowY       = -1;
};

class DesktopLyrics : public QWidget {
    Q_OBJECT
    Q_PROPERTY(float progress READ progress WRITE setProgress)
public:
    DesktopLyrics();
    ~DesktopLyrics();

    void showLyrics();
    void hideLyrics();
    void setLocked(bool locked);
    bool isLocked() const { return m_locked; }
    void toggleLocked();

    void setLrcText(const QString& lrcContent);
    void setPosition(long long msec);
    void testShowTime();
    void setPlaying(bool playing);

    void setPlayedColor(const QColor& c) { m_playedColor = c; update(); }
    void setUnplayedColor(const QColor& c) { m_unplayedColor = c; update(); }
    void setStrokeColor(const QColor& c) { m_strokeColor = c; update(); }
    void setBgColor(const QColor& c) { m_bgColor = c; update(); }
    void setFontSize(int pt);
    void setAlignment(int align) { m_align = align; update(); }
    void setLineMode(LineMode mode);
    LineMode lineMode() const { return m_lineMode; }

    QColor playedColor() const { return m_playedColor; }
    QColor unplayedColor() const { return m_unplayedColor; }
    QColor strokeColor() const { return m_strokeColor; }

    float progress() const { return m_animProgress; }
    void setProgress(float p);

    /* 使用示例：
       connect(lyrics, SIGNAL(aboutToShow()), this, SLOT(restoreLyrics()));
       ...
       void MyClass::restoreLyrics() {
           lyrics->setSetting(LyricsSettings()
               .withPlayedColor(savedPlayed)
               .withFontSize(32)
               .withX(100).withY(200));
       }
     */
    void setSetting(const LyricsSettings& s);

signals:
    void aboutToShow();
    void positionClicked(long long msec);
    void hideRequested();

protected:
    void paintEvent(QPaintEvent* e);
    void mousePressEvent(QMouseEvent* e);
    void mouseMoveEvent(QMouseEvent* e);
    void mouseReleaseEvent(QMouseEvent* e);
    void resizeEvent(QResizeEvent* e);
    void timerEvent(QTimerEvent* e);
    void contextMenuEvent(QContextMenuEvent* e);
    void enterEvent(qEnterEventType* e);
    void leaveEvent(QEvent* e);

private:
    void setupWindow();
    void parseLrc(const QString& content, std::vector<LrcLine>& out);
    void updateMinSize();

    void drawLine(QPainter& p, const QString& text, int x, int y, float progress);
    void drawTextWithStroke(QPainter& p, const QString& text, int x, int y,
                            const QColor& fillColor);

    std::vector<LrcLine> m_lines;
    long long m_position;
    int m_currentLine;
    int m_nextLine;

    float m_animProgress;
    float m_animTarget;
    int m_animTimerId;
    int m_hoverTimerId;
#ifdef QT3_BUILD
    class QPixmap* m_backing;
#else
    class QPropertyAnimation* m_anim;
#endif

    QFont m_font;
    QColor m_playedColor;
    QColor m_unplayedColor;
    QColor m_strokeColor;
    QColor m_bgColor;
    int m_align;
    LineMode m_lineMode;

    bool m_locked;
    bool m_hovered;
    bool m_transparentBg;
    bool m_dragging;
    QPoint m_dragStart;
};

#endif
