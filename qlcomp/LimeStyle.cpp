#include "LimeStyle.h"
#include "ThemeManager.h"
#include <algorithm>

#ifdef QT3_BUILD
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <qradiobutton.h>
#include <qpopupmenu.h>
#include <qcommonstyle.h>
#include <qobjectlist.h>
#endif

const StyleParams* g_activeParams = nullptr;

// ========== Color tables ==========

static StyleParams makeQtFusion(bool dark) {
    StyleParams p;
    auto& c = dark ? p.dark : p.light;
    if (dark) {
        c.windowBg       = QColor("#353535");
        c.surfaceBg      = QColor("#353535");
        c.baseBg         = QColor("#232323");
        c.hoverBg        = QColor("#404040");
        c.activeBg       = QColor("#484848");
        c.textPrimary    = QColor("#dcdcdc");
        c.textMuted      = QColor("#a0a0a0");
        c.textDisabled   = QColor("#7a7a7a");
        c.accent         = QColor("#2a82da");
        c.accentText     = QColor("#ffffff");
        c.border         = QColor("#505050");
        c.borderFocus    = QColor("#2a82da");
        c.link           = QColor("#2a82da");
        c.scrollbarSlider= QColor("#606060");
        c.scrollbarHover = QColor("#707070");
    } else {
        c.windowBg       = QColor("#f0f0f0");
        c.surfaceBg      = QColor("#ffffff");
        c.baseBg         = QColor("#ffffff");
        c.hoverBg        = QColor("#e5e5e5");
        c.activeBg       = QColor("#d0d0d0");
        c.textPrimary    = QColor("#1a1a1a");
        c.textMuted      = QColor("#666666");
        c.textDisabled   = QColor("#999999");
        c.accent         = QColor("#0969da");
        c.accentText     = QColor("#ffffff");
        c.border         = QColor("#c0c0c0");
        c.borderFocus    = QColor("#0969da");
        c.link           = QColor("#0550ae");
        c.scrollbarSlider= QColor("#c0c0c0");
        c.scrollbarHover = QColor("#a0a0a0");
    }
    p.buttonRadius    = 6;
    p.inputRadius     = 6;
    p.scrollbarWidth  = 8;
    p.spacing         = 8;
    p.touchTarget     = 32;
    p.scrollbarMode   = StyleParams::AlwaysFaint;
    p.buttonStyle     = StyleParams::Flat;
    p.compositingMode = ColorBlend;
    return p;
}

static StyleParams makeMacOS(bool dark) {
    StyleParams p;
    auto& c = dark ? p.dark : p.light;
    if (dark) {
        c.windowBg       = QColor("#323232");
        c.surfaceBg      = QColor("#3a3a3c");
        c.baseBg         = QColor("#1e1e1e");
        c.hoverBg        = QColor("#48484a");
        c.activeBg       = QColor("#555557");
        c.textPrimary    = QColor("#ffffff");
        c.textMuted      = QColor("#8e8e93");
        c.textDisabled   = QColor("#636366");
        c.accent         = QColor("#007aff");
        c.accentText     = QColor("#ffffff");
        c.border         = QColor("#48484a");
        c.borderFocus    = QColor("#007aff");
        c.link           = QColor("#007aff");
        c.scrollbarSlider= QColor("#8e8e93");
        c.scrollbarHover = QColor("#a0a0a5");
    } else {
        c.windowBg       = QColor("#ededed");
        c.surfaceBg      = QColor("#ffffff");
        c.baseBg         = QColor("#ffffff");
        c.hoverBg        = QColor("#e8e8e8");
        c.activeBg       = QColor("#d4d4d4");
        c.textPrimary    = QColor("#1d1d1f");
        c.textMuted      = QColor("#6c6c70");
        c.textDisabled   = QColor("#a1a1a6");
        c.accent         = QColor("#007aff");
        c.accentText     = QColor("#ffffff");
        c.border         = QColor("#d2d2d7");
        c.borderFocus    = QColor("#007aff");
        c.link           = QColor("#007aff");
        c.scrollbarSlider= QColor("#c6c6c8");
        c.scrollbarHover = QColor("#a1a1a3");
    }
    p.buttonRadius    = 8;
    p.inputRadius     = 8;
    p.scrollbarWidth  = 4;
    p.spacing         = 8;
    p.touchTarget     = 32;
    p.scrollbarMode   = StyleParams::OverlayFade;
    p.buttonStyle     = StyleParams::Capsule;
    p.compositingMode = AlphaBlend;
    return p;
}

static StyleParams makeWindows11(bool dark) {
    StyleParams p;
    auto& c = dark ? p.dark : p.light;
    if (dark) {
        c.windowBg       = QColor("#202020");
        c.surfaceBg      = QColor("#2c2c2c");
        c.baseBg         = QColor("#1f1f1f");
        c.hoverBg        = QColor("#3d3d3d");
        c.activeBg       = QColor("#484848");
        c.textPrimary    = QColor("#ffffff");
        c.textMuted      = QColor("#a0a0a0");
        c.textDisabled   = QColor("#5a5a5a");
        c.accent         = QColor("#60cdff");
        c.accentText     = QColor("#000000");
        c.border         = QColor("#555555");
        c.borderFocus    = QColor("#60cdff");
        c.link           = QColor("#60cdff");
        c.scrollbarSlider= QColor("#555555");
        c.scrollbarHover = QColor("#757575");
    } else {
        c.windowBg       = QColor("#f9f9f9");
        c.surfaceBg      = QColor("#ffffff");
        c.baseBg         = QColor("#ffffff");
        c.hoverBg        = QColor("#eaeaea");
        c.activeBg       = QColor("#d6d6d6");
        c.textPrimary    = QColor("#1a1a1a");
        c.textMuted      = QColor("#616161");
        c.textDisabled   = QColor("#a0a0a0");
        c.accent         = QColor("#0078d4");
        c.accentText     = QColor("#ffffff");
        c.border         = QColor("#d2d2d2");
        c.borderFocus    = QColor("#0078d4");
        c.link           = QColor("#0078d4");
        c.scrollbarSlider= QColor("#c0c0c0");
        c.scrollbarHover = QColor("#a0a0a0");
    }
    p.buttonRadius    = 4;
    p.inputRadius     = 4;
    p.scrollbarWidth  = 7;
    p.spacing         = 8;
    p.touchTarget     = 32;
    p.scrollbarMode   = StyleParams::AlwaysFaint;
    p.buttonStyle     = StyleParams::Border;
    p.compositingMode = ColorBlend;
    return p;
}

static StyleParams makeMaterialYou(bool dark) {
    StyleParams p;
    auto& c = dark ? p.dark : p.light;
    if (dark) {
        c.windowBg       = QColor("#1c1b1f");
        c.surfaceBg      = QColor("#2b2930");
        c.baseBg         = QColor("#1c1b1f");
        c.hoverBg        = QColor("#3b3940");
        c.activeBg       = QColor("#48464d");
        c.textPrimary    = QColor("#e6e1e5");
        c.textMuted      = QColor("#cac4d0");
        c.textDisabled   = QColor("#938f99");
        c.accent         = QColor("#d0bcff");
        c.accentText     = QColor("#381e72");
        c.border         = QColor("#938f99");
        c.borderFocus    = QColor("#d0bcff");
        c.link           = QColor("#d0bcff");
        c.scrollbarSlider= QColor("#938f99");
        c.scrollbarHover = QColor("#cac4d0");
    } else {
        c.windowBg       = QColor("#fffbfe");
        c.surfaceBg      = QColor("#f3edf7");
        c.baseBg         = QColor("#fffbfe");
        c.hoverBg        = QColor("#e8e3ec");
        c.activeBg       = QColor("#dbd6e0");
        c.textPrimary    = QColor("#1c1b1f");
        c.textMuted      = QColor("#79747e");
        c.textDisabled   = QColor("#cac4d0");
        c.accent         = QColor("#6750a4");
        c.accentText     = QColor("#ffffff");
        c.border         = QColor("#79747e");
        c.borderFocus    = QColor("#6750a4");
        c.link           = QColor("#6750a4");
        c.scrollbarSlider= QColor("#cac4d0");
        c.scrollbarHover = QColor("#49454f");
    }
    p.buttonRadius    = 16;
    p.inputRadius     = 12;
    p.scrollbarWidth  = 4;
    p.spacing         = 16;
    p.touchTarget     = 44;
    p.scrollbarMode   = StyleParams::OverlayFade;
    p.buttonStyle     = StyleParams::Capsule;
    p.compositingMode = ColorBlend;
    return p;
}

static StyleParams makeGitHub(bool dark) {
    StyleParams p = makeQtFusion(dark);
    auto& c = dark ? p.dark : p.light;
    if (dark) {
        c.windowBg       = QColor("#0d1117");
        c.surfaceBg      = QColor("#21262d");
        c.baseBg         = QColor("#161b22");
        c.hoverBg        = QColor("#30363d");
        c.activeBg       = QColor("#3d444d");
        c.textPrimary    = QColor("#c9d1d9");
        c.textMuted      = QColor("#8b949e");
        c.textDisabled   = QColor("#484f58");
        c.accent         = QColor("#1f6feb");
        c.accentText     = QColor("#ffffff");
        c.border         = QColor("#30363d");
        c.borderFocus    = QColor("#1f6feb");
        c.link           = QColor("#58a6ff");
        c.scrollbarSlider= QColor("#30363d");
        c.scrollbarHover = QColor("#3d444d");
    } else {
        c.windowBg       = QColor("#f0f0f0");
        c.surfaceBg      = QColor("#ffffff");
        c.baseBg         = QColor("#ffffff");
        c.hoverBg        = QColor("#e5e5e5");
        c.activeBg       = QColor("#d0d0d0");
        c.textPrimary    = QColor("#1a1a1a");
        c.textMuted      = QColor("#666666");
        c.textDisabled   = QColor("#999999");
        c.accent         = QColor("#0969da");
        c.accentText     = QColor("#ffffff");
        c.border         = QColor("#c0c0c0");
        c.borderFocus    = QColor("#0969da");
        c.link           = QColor("#0550ae");
        c.scrollbarSlider= QColor("#c0c0c0");
        c.scrollbarHover = QColor("#a0a0a0");
    }
    p.buttonRadius    = 6;
    p.inputRadius     = 6;
    p.scrollbarWidth  = 8;
    p.spacing         = 8;
    p.touchTarget     = 32;
    p.scrollbarMode   = StyleParams::AlwaysFaint;
    p.buttonStyle     = StyleParams::Flat;
    p.compositingMode = ColorBlend;
    return p;
}

static bool s_stylesRegistered = ([]() -> bool {
    StyleParams::registerStyle({"qtFusion",  "style.fusion",  &makeQtFusion});
    StyleParams::registerStyle({"macOS",     "style.macos",   &makeMacOS});
    StyleParams::registerStyle({"windows",   "style.windows", &makeWindows11});
    StyleParams::registerStyle({"material",  "style.material", &makeMaterialYou});
    StyleParams::registerStyle({"github",    "style.github",  &makeGitHub});
    return true;
})();

// ========== Helpers ==========

QColor lerpColor(const QColor& a, const QColor& b, float t) {
    if (t <= 0.0f) return a;
    if (t >= 1.0f) return b;
#ifdef QT3_BUILD
    return QColor(
        a.red()   + (int)((b.red()   - a.red())   * t),
        a.green() + (int)((b.green() - a.green()) * t),
        a.blue()  + (int)((b.blue()  - a.blue())  * t)
    );
#else
    return QColor(
        a.red()   + (int)((b.red()   - a.red())   * t),
        a.green() + (int)((b.green() - a.green()) * t),
        a.blue()  + (int)((b.blue()  - a.blue())  * t),
        a.alpha() + (int)((b.alpha() - a.alpha()) * t)
    );
#endif
}

static QColor safeColor(const QColor& c) {
    return c.isValid() ? c : QColor("#0d1117");
}

static QColor compositeColor(const QColor& bg, const QColor& fg, float ratio,
                             CompositingMode mode) {
    switch (mode) {
        case AlphaBlend: {
#ifdef QT3_BUILD
            return lerpColor(bg, fg, ratio);
#else
            QColor c = fg;
            c.setAlphaF(ratio);
            return c;
#endif
        }
        case ColorBlend:
            return lerpColor(bg, fg, ratio);
        case JumpCut:
            return fg;
    }
    return fg;
}

static bool isGlobalDark() {
    return ThemeManager::isDarkMode();
}

const StyleParams::Palette& currentPalette() {
    if (!g_activeParams) {
        static StyleParams::Palette fallback;
        return fallback;
    }
    return isGlobalDark() ? g_activeParams->dark : g_activeParams->light;
}

StyleParams makeCurrentParams() {
    if (!g_activeParams) return StyleParams::make("qtFusion", isGlobalDark());
    return *g_activeParams;
}

static QColor buttonColor(const StyleParams::Palette& pal, const StyleParams& params,
                          bool hover, bool down, bool disabled) {
    if (disabled) return pal.windowBg;
    if (down) return compositeColor(pal.windowBg, pal.activeBg, 1.0f, params.compositingMode);
    if (hover) return compositeColor(pal.windowBg, pal.hoverBg, 1.0f, params.compositingMode);
    return compositeColor(pal.windowBg, pal.surfaceBg, 1.0f, params.compositingMode);
}

static int buttonRadiusFor(const StyleParams& params, const QRect& r) {
    if (params.buttonStyle == StyleParams::Capsule)
        return r.height() / 2;
    return params.buttonRadius;
}

// ========== Constructor ==========

LimeStyle::LimeStyle() :
#ifdef QT3_BUILD
    QCommonStyle()
#else
    QProxyStyle()
#endif
{
}

// ========== drawPrimitive ==========

#ifdef QT3_BUILD

void LimeStyle::drawPrimitive(PrimitiveElement pe, QPainter *p, const QRect &r,
                              const QColorGroup &cg, SFlags flags,
                              const QStyleOption &opt) const {
    QStyleControlElementData ceData;
    ControlElementFlags elementFlags = CEF_None;
    drawPrimitive(pe, p, ceData, elementFlags, r, cg, flags, opt);
}

#else // Qt4

void LimeStyle::drawPrimitive(PrimitiveElement pe, const QStyleOption *opt,
                              QPainter *p, const QWidget *w) const {
    const StyleParams::Palette& pal = currentPalette();
    const StyleParams& params = makeCurrentParams();
    CompositingMode mode = params.compositingMode;

    switch (pe) {
    case PE_PanelLineEdit:
    case PE_FrameLineEdit: {
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QRect r = opt->rect;
        int rad = params.inputRadius;
        p->setBrush(pal.baseBg);
        p->setPen(Qt::NoPen);
        p->drawRoundedRect(r.adjusted(1, 1, -1, -1), rad, rad);
        bool hasFocus = opt->state & State_HasFocus;
        QColor borderColor = hasFocus ? pal.borderFocus : pal.border;
        QPen borderPen(borderColor, 1);
        p->setPen(borderPen);
        p->setBrush(Qt::NoBrush);
        p->drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), rad, rad);
        p->restore();
        return;
    }
#ifdef QT3_BUILD
    case PE_FocusRect:
#endif
    case PE_FrameFocusRect: {
        p->save();
        p->setPen(QPen(pal.borderFocus, 1, Qt::DotLine));
        p->setBrush(Qt::NoBrush);
        p->drawRect(QRect(opt->rect.x()+1, opt->rect.y()+1,
                          opt->rect.width()-2, opt->rect.height()-2));
        p->restore();
        return;
    }
#ifdef QT3_BUILD
    case PE_ButtonCommand:
#endif
    case PE_PanelButtonCommand: {
        bool hover = opt->state & State_MouseOver;
        bool down = opt->state & State_Sunken;
        bool disabled = !(opt->state & State_Enabled);
        QColor bg = buttonColor(pal, params, hover, down, disabled);
        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        p->setBrush(bg);
        p->setPen(Qt::NoPen);
        QRect r = opt->rect;
        int rad = buttonRadiusFor(params, r);
        p->drawRoundedRect(r, rad, rad);
        p->restore();
        return;
    }
    default:
        break;
    }

    QProxyStyle::drawPrimitive(pe, opt, p, w);
}

#endif

// ========== drawControl ==========

#ifdef QT3_BUILD

void LimeStyle::drawControl(ControlElement ce, QPainter *p, const QWidget *widget,
                            const QRect &r, SFlags flags,
                            const QStyleOption &opt) const {
    QStyleControlElementData ceData;
    ControlElementFlags elementFlags = CEF_None;
    if (widget) {
        ceData = populateControlElementDataFromWidget(widget, opt, true);
        elementFlags = getControlElementFlagsForObject(widget, opt, true);
    }
    QColorGroup cg = widget ? widget->colorGroup() : QColorGroup();
    drawControl(ce, p, ceData, elementFlags, r, cg, flags, opt, widget);
}

#else // Qt4

void LimeStyle::drawControl(ControlElement ce, const QStyleOption *opt,
                            QPainter *p, const QWidget *w) const {
    const StyleParams::Palette& pal = currentPalette();
    const StyleParams& params = makeCurrentParams();
    CompositingMode mode = params.compositingMode;

    switch (ce) {
    case CE_PushButton: {
        bool hover = opt->state & State_MouseOver;
        bool down = opt->state & State_Sunken;
        bool disabled = !(opt->state & State_Enabled);
        QColor bg = buttonColor(pal, params, hover, down, disabled);
        QColor fg = disabled
                    ? pal.textDisabled
                    : (down ? pal.accentText : pal.textPrimary);

        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QRect r = opt->rect;
        p->setBrush(bg);
        p->setPen(Qt::NoPen);
        int rad = buttonRadiusFor(params, r);
        p->drawRoundedRect(r, rad, rad);
        p->setPen(fg);
        if (const QStyleOptionButton* bopt =
            qstyleoption_cast<const QStyleOptionButton*>(opt)) {
            p->drawText(r, Qt::AlignCenter, bopt->text);
        }
        p->restore();
        return;
    }
#ifdef QT3_BUILD
    case CE_PopupMenuItem:
#endif
    case CE_MenuItem: {
        bool selected = opt->state & State_Selected;
        bool separator = false;
        bool enabled = opt->state & State_Enabled;
        QString text;

        if (const QStyleOptionMenuItem* mopt =
            qstyleoption_cast<const QStyleOptionMenuItem*>(opt)) {
            separator = mopt->menuItemType == QStyleOptionMenuItem::Separator;
            text = mopt->text;
            enabled = mopt->state & State_Enabled;
        }

        if (separator) {
            p->save();
            p->setPen(pal.border);
            p->drawLine(opt->rect.left() + 8, opt->rect.center().y(),
                         opt->rect.right() - 8, opt->rect.center().y());
            p->restore();
            return;
        }

        p->save();
        if (selected) {
            p->fillRect(opt->rect, pal.accent);
        } else {
            p->fillRect(opt->rect, pal.windowBg);
        }
        QColor fg = selected ? pal.accentText
                    : (enabled ? pal.textPrimary : pal.textDisabled);
        p->setPen(fg);
        QRect textR = opt->rect.adjusted(16, 0, -16, 0);
        int ampPos = text.indexOf(QLatin1Char('&'));
        if (ampPos >= 0) text.remove(ampPos, 1);
        p->drawText(textR, Qt::AlignVCenter | Qt::AlignLeft, text);
        p->restore();
        return;
    }
    case CE_CheckBox:
    case CE_CheckBoxLabel:
        break;
    case CE_ItemViewItem: {
        if (const QStyleOptionViewItemV4* vopt =
            qstyleoption_cast<const QStyleOptionViewItemV4*>(opt)) {
            QRect r = opt->rect;
            bool sel = opt->state & State_Selected;
            bool hover = opt->state & State_MouseOver;

            p->save();
            if (sel) {
                p->fillRect(r, pal.accent);
            } else if (hover) {
                p->fillRect(r, pal.hoverBg);
            }

            p->setPen(sel ? pal.accentText : pal.textPrimary);
            QRect textRect = r.adjusted(12, 0, -12, 0);
            p->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, vopt->text);
            p->restore();
            return;
        }
        break;
    }
    case CE_ProgressBarGroove: {
        p->fillRect(opt->rect, pal.baseBg);
        return;
    }
    case CE_ProgressBarContents: {
        if (const QStyleOptionProgressBar* pb =
            qstyleoption_cast<const QStyleOptionProgressBar*>(opt)) {
            int total = pb->maximum - pb->minimum;
            int cur = pb->progress - pb->minimum;
            if (total > 0) {
                int fillW = opt->rect.width() * cur / total;
                if (fillW > 0)
                    p->fillRect(opt->rect.x(), opt->rect.y(),
                                fillW, opt->rect.height(), pal.accent);
            }
        }
        return;
    }
    case CE_ProgressBarLabel: {
        if (const QStyleOptionProgressBar* pb =
            qstyleoption_cast<const QStyleOptionProgressBar*>(opt)) {
            p->setPen(pal.textPrimary);
            p->drawText(opt->rect, Qt::AlignCenter, pb->text);
        }
        return;
    }
    case CE_MenuBarItem: {
        bool hover = opt->state & State_MouseOver;
        if (hover)
            p->fillRect(opt->rect, pal.hoverBg);
        p->save();
        p->setPen(pal.textPrimary);
        if (const QStyleOptionMenuItem* mbi =
            qstyleoption_cast<const QStyleOptionMenuItem*>(opt)) {
            QString text = mbi->text;
            int ampPos = text.indexOf(QLatin1Char('&'));
            if (ampPos >= 0) text.remove(ampPos, 1);
            p->drawText(opt->rect, Qt::AlignCenter, text);
        }
        p->restore();
        return;
    }
    default:
        break;
    }

    QProxyStyle::drawControl(ce, opt, p, w);
}

#endif

// ========== drawComplexControl ==========

#ifdef QT3_BUILD

void LimeStyle::drawComplexControl(ComplexControl cc, QPainter *p,
                                   const QWidget *widget, const QRect &r,
                                   const QColorGroup &cg, SFlags flags,
                                   SCFlags controls, SCFlags active,
                                   const QStyleOption &opt) const {
    QStyleControlElementData ceData;
    ControlElementFlags elementFlags = CEF_None;
    if (widget) {
        ceData = populateControlElementDataFromWidget(widget, opt, true);
        elementFlags = getControlElementFlagsForObject(widget, opt, true);
    }
    drawComplexControl(cc, p, ceData, elementFlags, r, cg, flags, controls, active, opt, widget);
}

#else // Qt4

void LimeStyle::drawComplexControl(ComplexControl cc,
                                   const QStyleOptionComplex *opt,
                                   QPainter *p, const QWidget *w) const {
    const StyleParams::Palette& pal = currentPalette();
    const StyleParams& params = makeCurrentParams();
    CompositingMode mode = params.compositingMode;

    switch (cc) {
    case CC_ScrollBar: {
        QRect sliderRect = subControlRect(CC_ScrollBar, opt,
                                          SC_ScrollBarSlider, w);
        if (sliderRect.isValid()) {
            p->save();
            p->setRenderHint(QPainter::Antialiasing);
            QColor sc = pal.scrollbarSlider;
            if (params.scrollbarMode == StyleParams::AlwaysFaint)
                sc = compositeColor(pal.windowBg, sc, 0.4f, mode);
            p->setBrush(sc);
            p->setPen(Qt::NoPen);
            p->drawRoundedRect(sliderRect, 4, 4);
            p->restore();
        }
        return;
    }
    case CC_ComboBox: {
        bool hover = opt->state & State_MouseOver;
        bool down = opt->state & State_Sunken;
        bool disabled = !(opt->state & State_Enabled);
        QColor bg = buttonColor(pal, params, hover, down, disabled);
        QColor fg = disabled ? pal.textDisabled : pal.textPrimary;

        p->save();
        p->setRenderHint(QPainter::Antialiasing);
        QRect r = opt->rect;
        p->setBrush(bg);
        p->setPen(pal.border);
        int rad = buttonRadiusFor(params, r);
        p->drawRoundedRect(r, rad, rad);

        QRect arrowRect = subControlRect(CC_ComboBox, opt,
                                         SC_ComboBoxArrow, w);
        bool popupOpen = opt->state & State_On;
        QString arrow = popupOpen ? QString(QChar(0x25B2)) : QString(QChar(0x25BC));
        p->setPen(fg);
        p->drawText(arrowRect, Qt::AlignCenter, arrow);
        p->restore();
        return;
    }
    default:
        break;
    }

    QProxyStyle::drawComplexControl(cc, opt, p, w);
}

#endif

// ========== pixelMetric ==========

#ifdef QT3_BUILD

int LimeStyle::pixelMetric(PixelMetric m, const QWidget *w) const {
    const StyleParams& params = makeCurrentParams();
    int sbw = params.scrollbarWidth;

    switch (m) {
    case PM_ScrollBarExtent:      return sbw;
    case PM_ScrollBarSliderMin:   return 16;
    case PM_ButtonMargin:         return 6;
    case PM_DefaultFrameWidth:    return 1;
    case PM_DockWindowHandleExtent: return 14;
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:      return 14;
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight: return 14;
    case PM_ButtonDefaultIndicator: return 0;
    case PM_MenuButtonIndicator:  return 0;
    case PM_ButtonShiftHorizontal:
    case PM_ButtonShiftVertical:  return 0;
    case PM_SpinBoxFrameWidth:    return 2;
    case PM_SliderThickness:      return 20;
    case PM_SliderControlThickness: return 20;
    case PM_SliderLength:         return 20;
    case PM_TabBarTabOverlap:     return 2;
    case PM_TabBarTabHSpace:      return 12;
    case PM_TabBarTabVSpace:      return 4;
    case PM_TabBarBaseHeight:     return 0;
    case PM_TabBarBaseOverlap:    return 0;
    case PM_ProgressBarChunkWidth: return 8;
    case PM_SplitterWidth:        return 6;
    case PM_TitleBarHeight:       return 20;
    case PM_ArrowSize:            return 12;
    case PM_MenuBarFrameWidth:    return 2;
    case PM_MenuBarItemSpacing:   return 8;
    case PM_HeaderMargin:         return 4;
    case PM_HeaderMarkSize:       return 12;
    case PM_HeaderGripMargin:     return 2;
    case PM_DockWindowSeparatorExtent: return 6;
    case PM_DockWindowFrameWidth: return 1;
    case PM_MDIFrameWidth: return 2;
    case PM_MDIMinimizedWidth: return 200;
    case PM_MaximumDragDistance:  return -1;
    case PM_ToolBarItemSpacing:   return 0;
    case PM_SliderSpaceAvailable: return 0;
    case PM_SliderTickmarkOffset: return 0;
    case PM_PopupMenuScrollerHeight: return 20;
    case PM_CheckListButtonSize:  return 16;
    case PM_CheckListControllerSize: return 16;
    case PM_MenuIconIndicatorFrameHBorder:
    case PM_MenuIconIndicatorFrameVBorder:
    case PM_MenuIndicatorFrameHBorder:
    case PM_MenuIndicatorFrameVBorder: return 2;
    case PM_PopupMenuFrameHorizontalExtra:
    case PM_PopupMenuFrameVerticalExtra: return 0;
    case PM_TabBarScrollButtonWidth: return 16;
    case PM_TabBarTabShiftHorizontal:
    case PM_TabBarTabShiftVertical: return 0;
    case PM_DialogButtonsSeparator: return 6;
    case PM_DialogButtonsButtonWidth: return 80;
    case PM_DialogButtonsButtonHeight: return 30;
    default: return 0;
    }
}

#else

int LimeStyle::pixelMetric(PixelMetric m, const QStyleOption *opt,
                            const QWidget *w) const {
    const StyleParams& params = makeCurrentParams();

    switch (m) {
    case PM_ScrollBarExtent: return params.scrollbarWidth;
    case PM_ButtonMargin:    return 6;
    case PM_DefaultFrameWidth: return 1;
    default: break;
    }

    return QProxyStyle::pixelMetric(m, opt, w);
}

#endif

// ========== styleHint ==========

#ifdef QT3_BUILD

int LimeStyle::styleHint(StyleHint sh, const QStyleOption &opt,
                         const QWidget *w, QStyleHintReturn *hret) const {
    const StyleParams& params = makeCurrentParams();

    switch (sh) {
    case SH_EtchDisabledText:      return 0;
    case SH_GUIStyle:              return 0;
    case SH_GroupBox_TextLabelVerticalAlignment: return Qt::AlignVCenter;
    case SH_ScrollBar_LeftClickAbsolutePosition: return 1;
    case SH_Slider_SnapToValue: return 1;
    case SH_UnderlineAccelerator: return 1;
    default: return 0;
    }
}

#else

int LimeStyle::styleHint(StyleHint sh, const QStyleOption *opt,
                         const QWidget *w, QStyleHintReturn *hret) const {
    const StyleParams& params = makeCurrentParams();

    switch (sh) {
    case SH_EtchDisabledText:      return 0;
    case SH_GroupBox_TextLabelVerticalAlignment: return Qt::AlignVCenter;
    case SH_ScrollBar_LeftClickAbsolutePosition: return 1;
    case SH_Slider_SnapToValue: return 1;
    default: break;
    }

    return QProxyStyle::styleHint(sh, opt, w, hret);
}

// ========== subControlRect (Qt4) ==========

QRect LimeStyle::subControlRect(ComplexControl cc, const QStyleOptionComplex *opt,
                                SubControl sc, const QWidget *w) const {
    const StyleParams& params = makeCurrentParams();

    switch (cc) {
    case CC_ScrollBar: {
        if (const QStyleOptionSlider *sb = qstyleoption_cast<const QStyleOptionSlider*>(opt)) {
            QRect r = opt->rect;
            int min = sb->minimum;
            int max = sb->maximum;
            int val = sb->sliderValue;
            bool horiz = sb->orientation == Qt::Horizontal;
            int viewSize = horiz ? r.width() : r.height();
            int sliderSize = std::max(16, viewSize * viewSize / (viewSize + (max - min)));
            int avail = viewSize - sliderSize;
            int pos = (max > min) ? (val - min) * avail / (max - min) : 0;

            switch (sc) {
            case SC_ScrollBarSlider:
                if (horiz) return QRect(r.x() + pos, r.y(), sliderSize, r.height());
                else return QRect(r.x(), r.y() + pos, r.width(), sliderSize);
            case SC_ScrollBarGroove: return r;
            case SC_ScrollBarAddLine: {
                int sz = horiz ? r.height() : r.width();
                if (horiz) return QRect(r.right() - sz, r.y(), sz, r.height());
                else return QRect(r.x(), r.bottom() - sz, r.width(), sz);
            }
            case SC_ScrollBarSubLine: {
                int sz = horiz ? r.height() : r.width();
                if (horiz) return QRect(r.x(), r.y(), sz, r.height());
                else return QRect(r.x(), r.y(), r.width(), sz);
            }
            default: break;
            }
        }
        break;
    }
    case CC_ComboBox: {
        QRect r = opt->rect;
        int fw = pixelMetric(PM_DefaultFrameWidth, opt, w);
        switch (sc) {
        case SC_ComboBoxFrame: return r;
        case SC_ComboBoxEditField:
            return QRect(r.x() + fw, r.y() + fw,
                         r.width() - 2*fw - 20, r.height() - 2*fw);
        case SC_ComboBoxArrow:
            return QRect(r.right() - 20, r.y(), 20, r.height());
        default: break;
        }
        break;
    }
    case CC_Slider: {
        QRect r = opt->rect;
        int handleSize = std::min(r.width(), r.height()) - 4;
        switch (sc) {
        case SC_SliderGroove: return QRect(r.x()+4, r.y(), r.width()-8, r.height());
        case SC_SliderHandle:
            return QRect(r.center().x() - handleSize/2,
                         r.center().y() - handleSize/2,
                         handleSize, handleSize);
        default: break;
        }
        break;
    }
    default:
        break;
    }

    return QProxyStyle::subControlRect(cc, opt, sc, w);
}

#endif

// ========== Polish / UnPolish (Qt3 hover tracking) ==========

#ifdef QT3_BUILD

void LimeStyle::polish(QWidget *w) {
    w->installEventFilter(this);
    w->setMouseTracking(true);
    QStyle::polish(w);
}

void LimeStyle::unPolish(QWidget *w) {
    w->removeEventFilter(this);
    QStyle::unPolish(w);
}

bool LimeStyle::eventFilter(QObject *o, QEvent *e) {
    if (e->type() == QEvent::Enter || e->type() == QEvent::Leave) {
        if (o->isWidgetType()) {
            static_cast<QWidget*>(o)->update();
        }
    }
    return QStyle::eventFilter(o, e);
}

#endif

// ========== New API pure virtuals (forked Qt3 only) ==========
// These forward to old API overloads to avoid passing potentially null
// QStyleControlElementData references during early widget construction.

#ifdef QT3_BUILD

void LimeStyle::polishPopupMenu(const QStyleControlElementData &ceData,
                                ControlElementFlags elementFlags, void *ptr) {
}

void LimeStyle::drawPrimitive(PrimitiveElement pe, QPainter *p,
                              const QStyleControlElementData &ceData,
                              ControlElementFlags elementFlags,
                              const QRect &r, const QColorGroup &cg,
                              SFlags flags,
                              const QStyleOption &opt) const {
    const StyleParams::Palette& pal = currentPalette();
    const StyleParams& params = makeCurrentParams();
    CompositingMode mode = params.compositingMode;

    switch (pe) {
    case PE_PanelLineEdit: {
        p->save();
        int rad = params.inputRadius;
        QColor bg = pal.baseBg;
        p->setBrush(bg);
        p->setPen(Qt::NoPen);
        p->drawRoundRect(QRect(r.x()+1, r.y()+1, r.width()-2, r.height()-2),
                         rad * 200 / r.width(), rad * 200 / r.height());
        QColor borderColor = (flags & Style_HasFocus) ? pal.borderFocus : pal.border;
        QPen borderPen(borderColor, 1);
        p->setPen(borderPen);
        p->setBrush(Qt::NoBrush);
        p->drawRoundRect(r, rad * 200 / r.width(), rad * 200 / r.height());
        p->restore();
        return;
    }
    case PE_FocusRect: {
        p->save();
        p->setPen(QPen(pal.borderFocus, 1, Qt::DotLine));
        p->setBrush(Qt::NoBrush);
        p->drawRect(r.x()+1, r.y()+1, r.width()-2, r.height()-2);
        p->restore();
        return;
    }
    default:
        break;
    }

    QCommonStyle::drawPrimitive(pe, p, ceData, elementFlags, r, cg, flags, opt);
}

void LimeStyle::drawControl(ControlElement ce, QPainter *p,
                            const QStyleControlElementData &ceData,
                            ControlElementFlags elementFlags,
                            const QRect &r, const QColorGroup &cg,
                            SFlags flags, const QStyleOption &opt,
                            const QWidget *widget) const {
    const StyleParams::Palette& pal = currentPalette();
    const StyleParams& params = makeCurrentParams();
    CompositingMode mode = params.compositingMode;

    switch (ce) {
    case CE_PushButton: {
        bool hover = (flags & Style_MouseOver);
        bool down = (flags & Style_Down);
        bool disabled = !(flags & Style_Enabled);
        QColor bg = buttonColor(pal, params, hover, down, disabled);
        QColor fg = disabled ? pal.textDisabled : pal.textPrimary;

        p->save();
        p->setBrush(bg);
        p->setPen(Qt::NoPen);
        int rad = buttonRadiusFor(params, r);
        if (rad > 0)
            p->drawRoundRect(r, rad * 200 / r.width(), rad * 200 / r.height());
        else
            p->drawRect(r);
        p->setPen(fg);
        if (const QPushButton* btn = dynamic_cast<const QPushButton*>(widget)) {
            p->drawText(r, Qt::AlignCenter, btn->text());
        }
        p->restore();
        return;
    }
    case CE_PopupMenuItem: {
        if (const QMenuItem* mi = opt.menuItem()) {
            // Qt3 QPopupMenu 悬停/键盘选中的项由 drawItem 传 Style_Active（fork
            // qpopupmenu.cpp:1491），只查 Style_Selected 会导致所有菜单无悬停高亮
            bool selected = (flags & Style_Active) || (flags & Style_Selected);
            bool disabled = !(flags & Style_Enabled);
            QString text = mi->text();
            bool isSep = mi->isSeparator();

            if (isSep) {
                p->save();
                p->setPen(pal.border);
                p->drawLine(r.left() + 8, r.center().y(),
                            r.right() - 8, r.center().y());
                p->restore();
                return;
            }

            p->save();
            if (selected)
                p->fillRect(r, pal.accent);
            else
                p->fillRect(r, pal.windowBg);

            QColor fg = selected ? pal.accentText
                        : (disabled ? pal.textDisabled : pal.textPrimary);
            p->setPen(fg);

            int ampPos = text.find('&');
            if (ampPos >= 0)
                text.remove(ampPos, 1);

            QRect textR(r.x() + 20, r.y(), r.width() - 36, r.height());
            int tabPos = text.find('\t');
            if (tabPos >= 0) {
                QString label = text.left(tabPos);
                p->drawText(textR, Qt::AlignLeft | Qt::AlignVCenter, label);
                p->drawText(textR, Qt::AlignRight | Qt::AlignVCenter, text.mid(tabPos + 1));
                if (ampPos >= 0 && ampPos < tabPos) {
                    QFontMetrics fm = p->fontMetrics();
                    int beforeW = fm.width(label.left(ampPos));
                    int charW = fm.width(label[ampPos]);
                    int baselineY = r.y() + (r.height() + fm.ascent() - fm.descent()) / 2;
                    p->drawLine(textR.x() + beforeW, baselineY + 1,
                                textR.x() + beforeW + charW, baselineY + 1);
                }
            } else {
                p->drawText(textR, Qt::AlignLeft | Qt::AlignVCenter, text);
                if (ampPos >= 0) {
                    QFontMetrics fm = p->fontMetrics();
                    int beforeW = fm.width(text.left(ampPos));
                    int charW = fm.width(text[ampPos]);
                    int baselineY = r.y() + (r.height() + fm.ascent() - fm.descent()) / 2;
                    p->drawLine(textR.x() + beforeW, baselineY + 1,
                                textR.x() + beforeW + charW, baselineY + 1);
                }
            }
            p->restore();
        }
        return;
    }
    case CE_CheckBox: {
        int indW = pixelMetric(PM_IndicatorWidth, widget);
        int indH = pixelMetric(PM_IndicatorHeight, widget);
        QRect indR(r.x(), r.y() + (r.height() - indH) / 2, indW, indH);
        p->save();
        p->setBrush(pal.baseBg);
        p->setPen(pal.border);
        p->drawRect(indR);
        if (flags & Style_On) {
            p->setPen(QPen(pal.textPrimary, 2));
            int cx = indR.center().x(), cy = indR.center().y();
            p->drawLine(cx - 3, cy, cx, cy + 3);
            p->drawLine(cx, cy + 3, cx + 4, cy - 2);
        }
        p->restore();
        if (const QCheckBox* cb = dynamic_cast<const QCheckBox*>(widget)) {
            p->save();
            p->setPen(pal.textPrimary);
            QRect textR(r.x() + indW + 4, r.y(), r.width() - indW - 4, r.height());
            p->drawText(textR, Qt::AlignLeft | Qt::AlignVCenter, cb->text());
            p->restore();
        }
        return;
    }
    case CE_MenuBarItem: {
        bool hover = (flags & Style_MouseOver);
        if (hover)
            p->fillRect(r, pal.hoverBg);
        p->save();
        p->setPen(pal.textPrimary);
        if (const QMenuItem* mi = opt.menuItem()) {
            QString text = mi->text();
            int ampPos = text.find('&');
            if (ampPos >= 0) {
                text.remove(ampPos, 1);
                p->drawText(r, Qt::AlignCenter, text);
                QFontMetrics fm = p->fontMetrics();
                int beforeW = fm.width(text.left(ampPos));
                int charW = fm.width(text[ampPos]);
                int textW = fm.width(text);
                int x0 = r.x() + (r.width() - textW) / 2;
                int baselineY = r.y() + (r.height() + fm.ascent() - fm.descent()) / 2;
                p->drawLine(x0 + beforeW, baselineY + 1,
                            x0 + beforeW + charW, baselineY + 1);
            } else {
                p->drawText(r, Qt::AlignCenter, text);
            }
        }
        p->restore();
        return;
    }
    case CE_MenuBarEmptyArea: {
        p->fillRect(r, pal.windowBg);
        return;
    }
    case CE_RadioButton:
    case CE_RadioButtonLabel: {
        int indW = pixelMetric(PM_ExclusiveIndicatorWidth, widget);
        int indH = pixelMetric(PM_ExclusiveIndicatorHeight, widget);
        QRect indR(r.x(), r.y() + (r.height() - indH) / 2, indW, indH);
        p->save();
        p->setBrush(pal.baseBg);
        p->setPen(pal.border);
        p->drawEllipse(indR);
        if (flags & Style_On) {
            p->setBrush(pal.accent);
            p->setPen(Qt::NoPen);
            int ds = std::min(indR.width(), indR.height()) / 3;
            p->drawEllipse(indR.center().x() - ds/2, indR.center().y() - ds/2, ds, ds);
        }
        p->restore();
        if (const QRadioButton* rb = dynamic_cast<const QRadioButton*>(widget)) {
            p->save();
            p->setPen(pal.textPrimary);
            QRect textR(r.x() + indW + 4, r.y(), r.width() - indW - 4, r.height());
            p->drawText(textR, Qt::AlignLeft | Qt::AlignVCenter, rb->text());
            p->restore();
        }
        return;
    }
    case CE_TabBarTab: {
        bool selected = (flags & Style_Selected);
        if (selected) {
            p->fillRect(r, pal.baseBg);
        } else {
            p->fillRect(r, pal.surfaceBg);
        }
        p->setPen(pal.border);
        p->drawLine(r.bottomLeft(), r.bottomRight());
        return;
    }
    case CE_ProgressBarGroove: {
        p->fillRect(r, pal.baseBg);
        return;
    }
    case CE_ProgressBarContents: {
        if (const QProgressBar* pb = dynamic_cast<const QProgressBar*>(widget)) {
            int total = pb->totalSteps();
            int cur = pb->progress();
            if (total > 0) {
                int fillW = r.width() * cur / total;
                if (fillW > 0)
                    p->fillRect(r.x(), r.y(), fillW, r.height(), pal.accent);
            }
        }
        return;
    }
    case CE_ProgressBarLabel: {
        if (const QProgressBar* pb = dynamic_cast<const QProgressBar*>(widget)) {
            p->setPen(pal.textPrimary);
            p->drawText(r, Qt::AlignCenter, pb->progressString());
        }
        return;
    }
    default:
        break;
    }

    QCommonStyle::drawControl(ce, p, ceData, elementFlags, r, cg, flags, opt, widget);
}

void LimeStyle::drawControlMask(ControlElement ce, QPainter *p,
                                const QStyleControlElementData &,
                                ControlElementFlags,
                                const QRect &r, const QStyleOption &opt,
                                const QWidget *widget) const {
    Q_UNUSED(ce); Q_UNUSED(opt); Q_UNUSED(widget);
    p->fillRect(r, Qt::color0);
    p->setBrush(Qt::color1);
    p->setPen(Qt::NoPen);
    {
        int w = r.width(), h = r.height();
        int rad = (w < h ? w : h) / 6;
        if (rad > 0)
            p->drawRoundRect(r, rad * 200 / w, rad * 200 / h);
        else
            p->fillRect(r, Qt::color1);
    }
}

QRect LimeStyle::subRect(SubRect r, const QStyleControlElementData &,
                         const ControlElementFlags,
                         const QWidget *widget) const {
    if (!widget) return QRect();
    QRect wr = widget->rect();
    int margin = pixelMetric(PM_ButtonMargin, widget);
    int indW = pixelMetric(PM_IndicatorWidth, widget);
    int indH = pixelMetric(PM_IndicatorHeight, widget);
    int exW = pixelMetric(PM_ExclusiveIndicatorWidth, widget);
    int fw = pixelMetric(PM_DefaultFrameWidth, widget);

    switch (r) {
    case SR_PushButtonContents:
    case SR_PushButtonFocusRect:
        return QRect(wr.x() + margin, wr.y() + margin,
                     wr.width() - 2*margin, wr.height() - 2*margin);
    case SR_CheckBoxIndicator:
    case SR_RadioButtonIndicator:
        return QRect(wr.x(), wr.y() + (wr.height() - indH) / 2,
                     indW, indH);
    case SR_CheckBoxContents:
    case SR_RadioButtonContents:
        return QRect(wr.x() + indW + 4, wr.y(),
                     wr.width() - indW - 4, wr.height());
    case SR_CheckBoxFocusRect:
    case SR_RadioButtonFocusRect:
        return wr;
    case SR_ComboBoxFocusRect:
        return QRect(wr.x() + fw, wr.y() + fw,
                     wr.width() - 2*fw - 20, wr.height() - 2*fw);
    case SR_SliderFocusRect:
        return wr;
    case SR_ProgressBarGroove:
        return wr;
    case SR_ProgressBarContents:
        return QRect(wr.x() + fw, wr.y() + fw,
                     wr.width() - 2*fw, wr.height() - 2*fw);
    case SR_ProgressBarLabel:
        return wr;
    case SR_ToolButtonContents:
        return QRect(wr.x() + 2, wr.y() + 2,
                     wr.width() - 4, wr.height() - 4);
    case SR_ToolBoxTabContents:
        return wr;
    case SR_DockWindowHandleRect:
        return QRect(wr.x(), wr.y(), 14, wr.height());
    case SR_DialogButtonAbort:
    case SR_DialogButtonAccept:
    case SR_DialogButtonAll:
    case SR_DialogButtonApply:
    case SR_DialogButtonHelp:
    case SR_DialogButtonIgnore:
    case SR_DialogButtonReject:
    case SR_DialogButtonRetry:
    case SR_DialogButtonCustom: {
        int bw = pixelMetric(PM_DialogButtonsButtonWidth, widget);
        int bh = pixelMetric(PM_DialogButtonsButtonHeight, widget);
        return QRect(0, 0, bw, bh);
    }
    default:
        return QRect();
    }
}

void LimeStyle::drawComplexControl(ComplexControl cc, QPainter *p,
                                   const QStyleControlElementData &ceData,
                                   ControlElementFlags elementFlags,
                                   const QRect &r, const QColorGroup &cg,
                                   SFlags flags, SCFlags controls,
                                   SCFlags active, const QStyleOption &opt,
                                   const QWidget *widget) const {
    const StyleParams::Palette& pal = currentPalette();
    const StyleParams& params = makeCurrentParams();
    CompositingMode mode = params.compositingMode;

    switch (cc) {
    case CC_ScrollBar: {
        p->save();
        p->fillRect(r, pal.windowBg);
        int sliderMin = 16;
        QRect sliderRect;
        bool horiz = false;
        if (widget) {
            const QScrollBar *sb = static_cast<const QScrollBar*>(widget);
            horiz = (sb->orientation() == Qt::Horizontal);
            int range = sb->maxValue() - sb->minValue();
            int viewSize = horiz ? r.width() : r.height();
            int sliderSize = range > 0
                ? std::max(sliderMin, viewSize * viewSize / (viewSize + range))
                : viewSize;
            int pos = range > 0
                ? (sb->value() - sb->minValue()) * (viewSize - sliderSize) / range
                : 0;
            if (horiz) {
                sliderRect = QRect(r.x() + pos, r.y(), sliderSize, r.height());
            } else {
                sliderRect = QRect(r.x(), r.y() + pos, r.width(), sliderSize);
            }
        }
        QRect groove = horiz
            ? QRect(r.x(), r.center().y() - 1, r.width(), 3)
            : QRect(r.center().x() - 1, r.y(), 3, r.height());
        p->setBrush(pal.surfaceBg);
        p->setPen(Qt::NoPen);
        p->drawRect(groove);
        if (sliderRect.isValid()) {
            QColor sc = pal.scrollbarSlider;
            if (params.scrollbarMode == StyleParams::AlwaysFaint) {
                bool hovered = widget && widget->hasMouse();
                float ratio = hovered ? 1.0f : 0.4f;
                sc = compositeColor(pal.windowBg, sc, ratio, mode);
            }
            p->setBrush(sc);
            p->drawRoundRect(sliderRect, 4, 4);
        }
        p->restore();
        return;
    }
    case CC_ComboBox: {
        bool hover = (flags & Style_MouseOver);
        bool down = (flags & Style_Down);
        bool disabled = !(flags & Style_Enabled);
        QColor bg = buttonColor(pal, params, hover, down, disabled);
        QColor fg = disabled ? pal.textDisabled : pal.textPrimary;

        p->save();
        int rad = buttonRadiusFor(params, r);

        p->setBrush(bg);
        p->setPen(Qt::NoPen);
        p->drawRoundRect(r, rad * 200 / r.width(), rad * 200 / r.height());

        QRect editRect = querySubControlMetrics(CC_ComboBox, ceData, elementFlags,
                                                 SC_ComboBoxEditField, opt, widget);
        p->setBrush(pal.baseBg);
        p->setPen(Qt::NoPen);
        p->drawRect(editRect);

        p->setPen(pal.border);
        p->setBrush(Qt::NoBrush);
        p->drawRoundRect(r, rad * 200 / r.width(), rad * 200 / r.height());

        QRect arrowRect = querySubControlMetrics(CC_ComboBox, ceData, elementFlags,
                                                  SC_ComboBoxArrow, opt, widget);
        QString arrow = (active & SC_ComboBoxArrow)
                        ? QString(QChar(0x25B2)) : QString(QChar(0x25BC));
        p->setPen(fg);
        p->drawText(arrowRect, Qt::AlignCenter, arrow);
        p->restore();
        return;
    }
    case CC_Slider: {
        bool disabled = !(flags & Style_Enabled);
        p->save();
        QRect groove(r.x()+4, r.y(), r.width()-8, r.height());
        p->fillRect(groove, pal.baseBg);
        QColor handleColor = disabled ? pal.textDisabled : pal.accent;
        int handleSize = (r.width() < r.height() ? r.width() : r.height()) - 4;
        QRect handleRect(r.center().x() - handleSize/2,
                         r.center().y() - handleSize/2,
                         handleSize, handleSize);
        p->setBrush(handleColor);
        p->setPen(pal.border);
        p->drawRoundRect(handleRect, 4, 4);
        p->restore();
        return;
    }
    default:
        break;
    }

    QCommonStyle::drawComplexControl(cc, p, ceData, elementFlags, r, cg, flags, controls, active, opt, widget);
}

void LimeStyle::drawComplexControlMask(ComplexControl cc, QPainter *p,
                                        const QStyleControlElementData &,
                                        const ControlElementFlags,
                                        const QRect &r, const QStyleOption &opt,
                                        const QWidget *widget) const {
    Q_UNUSED(cc); Q_UNUSED(opt); Q_UNUSED(widget);
    p->fillRect(r, Qt::color0);
    p->setBrush(Qt::color1);
    p->setPen(Qt::NoPen);
    p->drawRoundRect(r, 20, 20);
}

QRect LimeStyle::querySubControlMetrics(ComplexControl cc,
                                        const QStyleControlElementData &ceData,
                                        ControlElementFlags,
                                        SubControl sc, const QStyleOption &opt,
                                        const QWidget *widget) const {
    if (!widget) return QRect();
    const StyleParams& params = makeCurrentParams();
    int sbw = params.scrollbarWidth;
    QRect wr = ceData.rect.isValid() ? ceData.rect : widget->rect();

    switch (cc) {
    case CC_ScrollBar: {
        QRect r = wr;
        const QScrollBar *sb = static_cast<const QScrollBar*>(widget);
        int min = sb->minValue();
        int max = sb->maxValue();
        int val = sb->value();
        int page = sb->pageStep();
        bool horiz = sb->orientation() == Qt::Horizontal;
        int viewSize = horiz ? r.width() : r.height();
        int sliderSize = std::max(16, viewSize * viewSize / (viewSize + (max - min)));
        int avail = viewSize - sliderSize;
        int pos = (max > min) ? (val - min) * avail / (max - min) : 0;

        switch (sc) {
        case SC_ScrollBarSlider:
            if (horiz)
                return QRect(r.x() + pos, r.y(), sliderSize, r.height());
            else
                return QRect(r.x(), r.y() + pos, r.width(), sliderSize);
        case SC_ScrollBarGroove:
            return r;
        case SC_ScrollBarAddLine: {
            int sz = horiz ? r.height() : r.width();
            if (horiz) return QRect(r.right() - sz, r.y(), sz, r.height());
            else return QRect(r.x(), r.bottom() - sz, r.width(), sz);
        }
        case SC_ScrollBarSubLine: {
            int sz = horiz ? r.height() : r.width();
            if (horiz) return QRect(r.x(), r.y(), sz, r.height());
            else return QRect(r.x(), r.y(), r.width(), sz);
        }
        case SC_ScrollBarAddPage:
            if (horiz) return QRect(r.x() + pos + sliderSize, r.y(), viewSize - pos - sliderSize, r.height());
            else return QRect(r.x(), r.y() + pos + sliderSize, r.width(), viewSize - pos - sliderSize);
        case SC_ScrollBarSubPage:
            if (horiz) return QRect(r.x(), r.y(), pos, r.height());
            else return QRect(r.x(), r.y(), r.width(), pos);
        default: return QRect();
        }
    }
    case CC_ComboBox: {
        QRect r = wr;
        int fw = pixelMetric(PM_DefaultFrameWidth, widget);
        switch (sc) {
        case SC_ComboBoxFrame: return r;
        case SC_ComboBoxEditField:
            return QRect(r.x() + fw, r.y() + fw,
                         r.width() - 2*fw - 20, r.height() - 2*fw);
        case SC_ComboBoxArrow:
            return QRect(r.right() - 20, r.y(), 20, r.height());
        default: return QRect();
        }
    }
    case CC_Slider: {
        QRect r = wr;
        int handleSize = (r.width() < r.height() ? r.width() : r.height()) - 4;
        switch (sc) {
        case SC_SliderGroove: return QRect(r.x()+4, r.y(), r.width()-8, r.height());
        case SC_SliderHandle:
            return QRect(r.center().x() - handleSize/2,
                         r.center().y() - handleSize/2,
                         handleSize, handleSize);
        default: return QRect();
        }
    }
    case CC_SpinWidget: {
        QRect r = wr;
        int btnH = r.height() / 2;
        switch (sc) {
        case SC_SpinWidgetFrame: return r;
        case SC_SpinWidgetEditField:
            return QRect(r.x()+2, r.y()+2, r.width()-22, r.height()-4);
        case SC_SpinWidgetUp:
            return QRect(r.right()-20, r.y(), 20, btnH);
        case SC_SpinWidgetDown:
            return QRect(r.right()-20, r.y()+btnH, 20, r.height()-btnH);
        case SC_SpinWidgetButtonField:
            return QRect(r.right()-20, r.y(), 20, r.height());
        default: return QRect();
        }
    }
    default:
        return QRect();
    }
}

QStyle::SubControl LimeStyle::querySubControl(ComplexControl cc,
                                      const QStyleControlElementData &,
                                      ControlElementFlags,
                                      const QPoint &pos,
                                      const QStyleOption &opt,
                                      const QWidget *widget) const {
    if (!widget) return SC_None;

    switch (cc) {
    case CC_ScrollBar: {
        QRect sliderR = querySubControlMetrics(cc, QStyleControlElementData(),
                                               ControlElementFlags(0),
                                               SC_ScrollBarSlider, opt, widget);
        if (sliderR.contains(pos)) return SC_ScrollBarSlider;
        QRect addLineR = querySubControlMetrics(cc, QStyleControlElementData(),
                                                ControlElementFlags(0),
                                                SC_ScrollBarAddLine, opt, widget);
        if (addLineR.contains(pos)) return SC_ScrollBarAddLine;
        QRect subLineR = querySubControlMetrics(cc, QStyleControlElementData(),
                                                ControlElementFlags(0),
                                                SC_ScrollBarSubLine, opt, widget);
        if (subLineR.contains(pos)) return SC_ScrollBarSubLine;
        QRect addPageR = querySubControlMetrics(cc, QStyleControlElementData(),
                                                ControlElementFlags(0),
                                                SC_ScrollBarAddPage, opt, widget);
        if (addPageR.contains(pos)) return SC_ScrollBarAddPage;
        QRect subPageR = querySubControlMetrics(cc, QStyleControlElementData(),
                                                ControlElementFlags(0),
                                                SC_ScrollBarSubPage, opt, widget);
        if (subPageR.contains(pos)) return SC_ScrollBarSubPage;
        return SC_ScrollBarGroove;
    }
    case CC_ComboBox: {
        QRect arrowR = querySubControlMetrics(cc, QStyleControlElementData(),
                                              ControlElementFlags(0),
                                              SC_ComboBoxArrow, opt, widget);
        if (arrowR.contains(pos)) return SC_ComboBoxArrow;
        return SC_ComboBoxEditField;
    }
    case CC_Slider: {
        QRect handleR = querySubControlMetrics(cc, QStyleControlElementData(),
                                               ControlElementFlags(0),
                                               SC_SliderHandle, opt, widget);
        if (handleR.contains(pos)) return SC_SliderHandle;
        return SC_SliderGroove;
    }
    case CC_SpinWidget: {
        QRect upR = querySubControlMetrics(cc, QStyleControlElementData(),
                                           ControlElementFlags(0),
                                           SC_SpinWidgetUp, opt, widget);
        if (upR.contains(pos)) return SC_SpinWidgetUp;
        QRect downR = querySubControlMetrics(cc, QStyleControlElementData(),
                                             ControlElementFlags(0),
                                             SC_SpinWidgetDown, opt, widget);
        if (downR.contains(pos)) return SC_SpinWidgetDown;
        return SC_SpinWidgetEditField;
    }
    default:
        return SC_None;
    }
}

int LimeStyle::pixelMetric(PixelMetric m,
                           const QStyleControlElementData &,
                           ControlElementFlags,
                           const QWidget *widget) const {
    return pixelMetric(m, widget);
}

QSize LimeStyle::sizeFromContents(ContentsType ct,
                                  const QStyleControlElementData &,
                                  ControlElementFlags,
                                  const QSize &contentsSize,
                                  const QStyleOption &opt,
                                  const QWidget *widget) const {
    int margin = pixelMetric(PM_ButtonMargin, widget);
    int fw = pixelMetric(PM_DefaultFrameWidth, widget);
    int indW = pixelMetric(PM_IndicatorWidth, widget);

    switch (ct) {
    case CT_PushButton:
        return QSize(contentsSize.width() + 2*margin,
                     contentsSize.height() + 2*margin);
    case CT_CheckBox:
        return QSize(contentsSize.width() + indW + 8,
                     contentsSize.height() + 4);
    case CT_RadioButton:
        return QSize(contentsSize.width() + indW + 8,
                     contentsSize.height() + 4);
    case CT_ComboBox: {
        int arrowW = 20;
        return QSize(contentsSize.width() + 2*fw + arrowW + 4,
                     contentsSize.height() + 2*fw + 4);
    }
    case CT_LineEdit:
        return QSize(contentsSize.width() + 2*fw,
                     contentsSize.height() + 2*fw);
    case CT_MenuBar:
        return QSize(contentsSize.width() + 2*fw,
                     contentsSize.height() + 2*fw);
    case CT_Slider:
        return QSize(contentsSize.width() + pixelMetric(PM_SliderThickness, widget),
                     contentsSize.height() + pixelMetric(PM_SliderThickness, widget));
    case CT_TabBarTab:
        return QSize(contentsSize.width() + pixelMetric(PM_TabBarTabHSpace, widget),
                     contentsSize.height() + pixelMetric(PM_TabBarTabVSpace, widget));
    case CT_ProgressBar:
        return QSize(contentsSize.width() + 2*fw,
                     contentsSize.height() + 2*fw);
    case CT_Splitter:
        return QSize(pixelMetric(PM_SplitterWidth, widget),
                     pixelMetric(PM_SplitterWidth, widget));
    case CT_SpinBox:
        return QSize(contentsSize.width() + 2*20,
                     contentsSize.height() + 2*fw);
    case CT_PopupMenuItem: {
        int h = contentsSize.height() + 4;
        return QSize(contentsSize.width() + 28, h < 20 ? 20 : h);
    }
    case CT_CustomBase:
    case CT_DockWindow:
    case CT_Header:
    case CT_TabWidget:
    case CT_ToolButton:
    case CT_SizeGrip:
    case CT_DialogButtons:
    default:
        return contentsSize;
    }
}

int LimeStyle::styleHint(StyleHint sh,
                         const QStyleControlElementData &,
                         ControlElementFlags,
                         const QStyleOption &opt,
                         QStyleHintReturn *returnData,
                         const QWidget *widget) const {
    return styleHint(sh, opt, widget, returnData);
}

QPixmap LimeStyle::stylePixmap(StylePixmap sp,
                               const QStyleControlElementData &,
                               ControlElementFlags,
                               const QStyleOption &opt,
                               const QWidget *widget) const {
    Q_UNUSED(opt); Q_UNUSED(widget);
    const StyleParams::Palette& pal = currentPalette();
    QColor bg = pal.baseBg;
    QColor fg = pal.textPrimary;
    QColor accent = pal.accent;

    auto makePm = [](int w, int h, QColor bg) -> QPixmap {
        QPixmap pm(w, h);
        pm.fill(bg);
        return pm;
    };

    auto drawCircle = [](QPixmap &pm, int x, int y, int r, QColor c) {
        QPainter p(&pm);
        p.setBrush(c);
        p.setPen(Qt::NoPen);
        p.drawEllipse(x - r, y - r, r*2, r*2);
    };

    auto drawRect = [](QPixmap &pm, int x, int y, int w, int h, QColor c) {
        QPainter p(&pm);
        p.fillRect(x, y, w, h, c);
    };

    auto drawLine = [](QPixmap &pm, int x1, int y1, int x2, int y2, QColor c, int lw) {
        QPainter p(&pm);
        p.setPen(QPen(c, lw));
        p.drawLine(x1, y1, x2, y2);
    };

    switch (sp) {
    case SP_TitleBarNormalButton:
    case SP_TitleBarMaxButton: {
        QPixmap pm = makePm(16, 16, bg);
        drawRect(pm, 2, 3, 12, 10, fg);
        drawRect(pm, 3, 4, 10, 8, bg);
        drawRect(pm, 2, 3, 12, 10, fg);
        return pm;
    }
    case SP_TitleBarMinButton: {
        QPixmap pm = makePm(16, 16, bg);
        drawRect(pm, 2, 12, 12, 2, fg);
        return pm;
    }
    case SP_TitleBarCloseButton: {
        QPixmap pm = makePm(16, 16, bg);
        drawLine(pm, 3, 3, 13, 13, fg, 2);
        drawLine(pm, 13, 3, 3, 13, fg, 2);
        return pm;
    }
    case SP_TitleBarShadeButton:
    case SP_TitleBarUnshadeButton: {
        QPixmap pm = makePm(16, 16, bg);
        bool down = (sp == SP_TitleBarShadeButton);
        int cy = down ? 10 : 6;
        QPainter p(&pm);
        p.setBrush(fg);
        p.setPen(Qt::NoPen);
        QPointArray pa(3);
        pa.setPoint(0, 3, cy - 3);
        pa.setPoint(1, 13, cy - 3);
        pa.setPoint(2, 8, cy + 3);
        p.drawPolygon(pa);
        return pm;
    }
    case SP_DockWindowCloseButton: {
        QPixmap pm = makePm(12, 12, bg);
        drawLine(pm, 2, 2, 10, 10, fg, 2);
        drawLine(pm, 10, 2, 2, 10, fg, 2);
        return pm;
    }
    case SP_MessageBoxInformation: {
        QPixmap pm = makePm(32, 32, bg);
        drawCircle(pm, 16, 16, 14, accent);
        QPainter p(&pm);
        p.setPen(QPen(bg, 3));
        p.drawText(QRect(8, 10, 16, 20), Qt::AlignCenter, "i");
        return pm;
    }
    case SP_MessageBoxWarning: {
        QPixmap pm = makePm(32, 32, bg);
        QPainter p(&pm);
        p.setBrush(QColor(255, 200, 0));
        p.setPen(Qt::NoPen);
        QPointArray pa(3);
        pa.setPoint(0, 16, 2);
        pa.setPoint(1, 2, 28);
        pa.setPoint(2, 30, 28);
        p.drawPolygon(pa);
        p.setPen(QPen(bg, 3));
        p.drawText(QRect(8, 12, 16, 16), Qt::AlignCenter, "!");
        return pm;
    }
    case SP_MessageBoxCritical: {
        QPixmap pm = makePm(32, 32, bg);
        drawCircle(pm, 16, 16, 14, QColor(220, 40, 40));
        QPainter p(&pm);
        p.setPen(QPen(bg, 3));
        p.drawLine(8, 8, 24, 24);
        p.drawLine(24, 8, 8, 24);
        return pm;
    }
    case SP_MessageBoxQuestion: {
        QPixmap pm = makePm(32, 32, bg);
        drawCircle(pm, 16, 16, 14, accent);
        QPainter p(&pm);
        p.setPen(QPen(bg, 3));
        QFont f = p.font();
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(8, 10, 16, 20), Qt::AlignCenter, "?");
        return pm;
    }
    default:
        return QPixmap(1, 1);
    }
}

#endif
