#ifndef LIMESTYLE_H
#define LIMESTYLE_H

#include "StyleParams.h"

#ifdef QT3_BUILD
#include <qapplication.h>
#include <qpalette.h>
#include <qstyle.h>
#include <qcommonstyle.h>
#include <qpainter.h>
#include <qscrollbar.h>
#include <qcombobox.h>
#include <qlineedit.h>
#include <qprogressbar.h>
#else
#include <QApplication>
#include <QPalette>
#include <QProxyStyle>
#include <QPainter>
#include <QStyleOption>
#include <QScrollBar>
#include <QComboBox>
#include <QLineEdit>
#include <QStyleOptionViewItem>
#if QT_VERSION < 0x060000
#include <QStyleOptionViewItemV4>
#endif
#endif

QColor lerpColor(const QColor& a, const QColor& b, float t);

const StyleParams::Palette& currentPalette();
StyleParams makeCurrentParams();

#ifdef QT3_BUILD

class LimeStyle : public QCommonStyle {
    // no Q_OBJECT - Qt3 3.5.0 moc can't handle #ifdef inside class body
public:
    LimeStyle();

    void drawPrimitive(PrimitiveElement pe, QPainter *p, const QRect &r,
                       const QColorGroup &cg, SFlags flags = Style_Default,
                       const QStyleOption &opt = QStyleOption()) const;

    void drawControl(ControlElement ce, QPainter *p, const QWidget *widget,
                     const QRect &r, SFlags flags = Style_Default,
                     const QStyleOption &opt = QStyleOption()) const;

    void drawComplexControl(ComplexControl cc, QPainter *p, const QWidget *widget,
                            const QRect &r, const QColorGroup &cg,
                            SFlags flags = Style_Default,
                            SCFlags controls = SC_All,
                            SCFlags active = SC_None,
                            const QStyleOption &opt = QStyleOption()) const;

    int pixelMetric(PixelMetric m, const QWidget *w = 0) const;
    int styleHint(StyleHint sh, const QStyleOption &opt = QStyleOption(),
                  const QWidget *w = 0, QStyleHintReturn *hret = 0) const;

    void polish(QWidget *w);
    void unPolish(QWidget *w);
    bool eventFilter(QObject *o, QEvent *e);

    // New API pure virtuals (forked Qt3 with QStyleControlElementData)
    void polishPopupMenu(const QStyleControlElementData &ceData,
                         ControlElementFlags elementFlags, void *ptr);
    void drawPrimitive(PrimitiveElement pe, QPainter *p,
                       const QStyleControlElementData &ceData,
                       ControlElementFlags elementFlags,
                       const QRect &r, const QColorGroup &cg,
                       SFlags flags = Style_Default,
                       const QStyleOption &opt = QStyleOption()) const;
    void drawControl(ControlElement ce, QPainter *p,
                     const QStyleControlElementData &ceData,
                     ControlElementFlags elementFlags,
                     const QRect &r, const QColorGroup &cg,
                     SFlags flags = Style_Default,
                     const QStyleOption &opt = QStyleOption(),
                     const QWidget *widget = 0) const;
    void drawControlMask(ControlElement ce, QPainter *p,
                         const QStyleControlElementData &ceData,
                         ControlElementFlags elementFlags,
                         const QRect &r,
                         const QStyleOption &opt = QStyleOption(),
                         const QWidget *widget = 0) const;
    QRect subRect(SubRect r, const QStyleControlElementData &ceData,
                  const ControlElementFlags elementFlags,
                  const QWidget *widget) const;
    void drawComplexControl(ComplexControl cc, QPainter *p,
                            const QStyleControlElementData &ceData,
                            ControlElementFlags elementFlags,
                            const QRect &r, const QColorGroup &cg,
                            SFlags flags = Style_Default,
                            SCFlags controls = SC_All,
                            SCFlags active = SC_None,
                            const QStyleOption &opt = QStyleOption(),
                            const QWidget *widget = 0) const;
    void drawComplexControlMask(ComplexControl cc, QPainter *p,
                                const QStyleControlElementData &ceData,
                                const ControlElementFlags elementFlags,
                                const QRect &r,
                                const QStyleOption &opt = QStyleOption(),
                                const QWidget *widget = 0) const;
    QRect querySubControlMetrics(ComplexControl cc,
                                 const QStyleControlElementData &ceData,
                                 ControlElementFlags elementFlags,
                                 SubControl sc,
                                 const QStyleOption &opt = QStyleOption(),
                                 const QWidget *widget = 0) const;
    SubControl querySubControl(ComplexControl cc,
                               const QStyleControlElementData &ceData,
                               ControlElementFlags elementFlags,
                               const QPoint &pos,
                               const QStyleOption &opt = QStyleOption(),
                               const QWidget *widget = 0) const;
    int pixelMetric(PixelMetric m, const QStyleControlElementData &ceData,
                    ControlElementFlags elementFlags,
                    const QWidget *widget = 0) const;
    QSize sizeFromContents(ContentsType ct,
                           const QStyleControlElementData &ceData,
                           ControlElementFlags elementFlags,
                           const QSize &contentsSize,
                           const QStyleOption &opt = QStyleOption(),
                           const QWidget *widget = 0) const;
    int styleHint(StyleHint sh, const QStyleControlElementData &ceData,
                  ControlElementFlags elementFlags,
                  const QStyleOption &opt = QStyleOption(),
                  QStyleHintReturn *returnData = 0,
                  const QWidget *widget = 0) const;
    QPixmap stylePixmap(StylePixmap sp,
                        const QStyleControlElementData &ceData,
                        ControlElementFlags elementFlags,
                        const QStyleOption &opt = QStyleOption(),
                        const QWidget *widget = 0) const;

};

#else

class LimeStyle : public QProxyStyle {
public:
    LimeStyle();

    void drawPrimitive(PrimitiveElement pe, const QStyleOption *opt,
                       QPainter *p, const QWidget *w = 0) const;

    void drawControl(ControlElement ce, const QStyleOption *opt,
                     QPainter *p, const QWidget *w = 0) const;

    void drawComplexControl(ComplexControl cc, const QStyleOptionComplex *opt,
                            QPainter *p, const QWidget *w = 0) const;

    int pixelMetric(PixelMetric m, const QStyleOption *opt = 0,
                    const QWidget *w = 0) const;

    int styleHint(StyleHint sh, const QStyleOption *opt = 0,
                  const QWidget *w = 0, QStyleHintReturn *hret = 0) const;

    QRect subControlRect(ComplexControl cc, const QStyleOptionComplex *opt,
                         SubControl sc, const QWidget *w = 0) const;
};

#endif

#endif
