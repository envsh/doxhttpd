#include "systemtrayicon.h"
#include "compatcore34.h"

#include <stdio.h>
#include <qimage.h>
#include <qpainter.h>
#include <qcolor.h>
#include <qfontmetrics.h>

#ifdef QT3_BUILD
#include "trayicon.h"
#ifdef Q_WS_X11
#include <qapplication.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#endif
#else
#include <qsystemtrayicon.h>
#endif

static const int TRAY_ICON_SIZE = 22;

#ifdef QT3_BUILD

// ============ Qt3：基于 Psi TrayIcon 的适配 ============

// 事件适配器：把 Psi 的 clicked/doubleClicked 信号模型
// 转成 Qt4 的 activated(reason) 模型。右键在弹菜单前先发 Context。
// 无 Q_OBJECT（qlcomp 无 cpp 内 moc 先例），通过友元调用 fireActivated。
class SystemTrayIconAdapter : public TrayIcon {
public:
	SystemTrayIconAdapter(SystemTrayIcon* owner, const QPixmap& pm)
		: TrayIcon(pm, QString(), 0, owner), m_owner(owner) {}

protected:
	virtual void mouseMoveEvent(QMouseEvent* e) {
		e->ignore();
	}

	virtual void mousePressEvent(QMouseEvent* e) {
		switch (e->button()) {
			case RightButton:
				m_owner->fireActivated(SystemTrayIcon::Context);
				if (popup()) {
					popup()->popup(e->globalPos());
					e->accept();
					return;
				}
				break;
			case LeftButton:
				m_owner->fireActivated(SystemTrayIcon::Trigger);
				break;
			case MidButton:
				m_owner->fireActivated(SystemTrayIcon::MiddleClick);
				break;
			default:
				break;
		}
		e->ignore();
	}

	virtual void mouseReleaseEvent(QMouseEvent* e) {
		e->ignore();
	}

	virtual void mouseDoubleClickEvent(QMouseEvent* e) {
		if (e->button() == LeftButton) {
			m_owner->fireActivated(SystemTrayIcon::DoubleClick);
		}
		e->accept();
	}

private:
	SystemTrayIcon* m_owner;
};

class SystemTrayIcon::Private {
public:
	Private() : tray(0), menu(0) {}
	SystemTrayIconAdapter* tray;
	PopupMenu* menu;
};

// Psi 的托盘窗口固定 22x22 且 paintEvent 只居中不缩放，
// 大图标会被裁剪，这里统一预缩放
static QPixmap trayNormalizeIcon(const QPixmap& pm)
{
	if (pm.width() <= TRAY_ICON_SIZE && pm.height() <= TRAY_ICON_SIZE) {
		return pm;
	}
	QImage img = pm.convertToImage();
	img = img.smoothScale(TRAY_ICON_SIZE, TRAY_ICON_SIZE, QImage::ScaleMin);
	QPixmap out;
	out.convertFromImage(img);
	return out;
}

void SystemTrayIcon::initTray(const QPixmap& pm)
{
	m_baseIcon = pm;
	d->tray = new SystemTrayIconAdapter(this, trayNormalizeIcon(m_baseIcon));
}

void SystemTrayIcon::fireActivated(int reason)
{
	emit activated(reason);
}

void SystemTrayIcon::nativeActivated(TrayActivationReason reason)
{
	Q_UNUSED(reason)
}

SystemTrayIcon::SystemTrayIcon(const QPixmap& icon, QObject* parent)
	: QObject(parent), d(new Private()), m_visible(false)
{
	initTray(icon);
}

SystemTrayIcon::SystemTrayIcon(const QString& fileName, QObject* parent)
	: QObject(parent), d(new Private()), m_visible(false)
{
	initTray(QPixmap(fileName));
}

SystemTrayIcon::~SystemTrayIcon()
{
	delete d;
}

void SystemTrayIcon::setIcon(const QPixmap& icon)
{
	m_baseIcon = icon;
	applyIcon();
}

void SystemTrayIcon::setIcon(const QString& fileName)
{
	setIcon(QPixmap(fileName));
}

QPixmap SystemTrayIcon::icon() const
{
	return m_baseIcon;
}

void SystemTrayIcon::setBadgeCount(int count)
{
	m_badgeCount = count;
	applyIcon();
	if (!m_toolTip.isEmpty()) {
		QString t = m_toolTip;
		if (count > 0) {
			t += qFromUtf8(" — 未读 ") + QString::number(count);
		}
		d->tray->setToolTip(t);
	}
}

int SystemTrayIcon::badgeCount() const
{
	return m_badgeCount;
}

void SystemTrayIcon::applyIcon()
{
	d->tray->setIcon(trayNormalizeIcon(renderBadgeIcon(m_baseIcon, m_badgeCount)));
}

QPixmap SystemTrayIcon::renderBadgeIcon(const QPixmap& base, int count) const
{
	QPixmap pm = base;
	if (count <= 0) {
		return pm;
	}
	if (pm.width() > TRAY_ICON_SIZE || pm.height() > TRAY_ICON_SIZE) {
		pm = trayNormalizeIcon(pm);
	}
	QString t = (count > 999) ? QString::fromLatin1("999") : QString::number(count);
	QPainter p(&pm);
	int px = (t.length() >= 3) ? 9 : 10;
	QFont f = p.font();
	f.setBold(true);
	f.setPixelSize(px);
	p.setFont(f);
	QFontMetrics fm(f);
	int tw = fm.width(t);
	int th = fm.height();
	int bw = tw + 6;
	if (bw > 15) {
		bw = 15;
	}
	int bh = 8;
	int bx = pm.width() - 2 - bw;
	int by = 2;
	p.setPen(NoPen);
	p.setBrush(QColor(0xD0, 0x20, 0x20));
	p.drawEllipse(bx, by, bw, bh);
	p.setPen(QColor(255, 255, 255));
	p.drawText(bx + (bw - tw) / 2, by + (bh - th) / 2 + fm.ascent(), t);
	p.end();
	return pm;
}

void SystemTrayIcon::setToolTip(const QString& tip)
{
	m_toolTip = tip;
	if (m_badgeCount > 0) {
		d->tray->setToolTip(m_toolTip + qFromUtf8(" — 未读 ") + QString::number(m_badgeCount));
	} else {
		d->tray->setToolTip(m_toolTip);
	}
}

QString SystemTrayIcon::toolTip() const
{
	return m_toolTip;
}

void SystemTrayIcon::setVisible(bool visible)
{
	if (visible == m_visible) {
		return;
	}
	// 无托盘环境时 Psi 会在安装阶段自行拆除，保持 m_visible=false 与实际一致
	if (visible && !isSystemTrayAvailable()) {
		return;
	}
	m_visible = visible;
	if (visible) {
		d->tray->show();
	} else {
		d->tray->hide();
	}
}

bool SystemTrayIcon::isVisible() const
{
	return m_visible;
}

void SystemTrayIcon::show()
{
	setVisible(true);
}

void SystemTrayIcon::hide()
{
	setVisible(false);
}

void SystemTrayIcon::setContextMenu(PopupMenu* menu)
{
	d->menu = menu;
	d->tray->setPopup(menu);
}

PopupMenu* SystemTrayIcon::contextMenu() const
{
	return d->menu;
}

bool SystemTrayIcon::isSystemTrayAvailable()
{
#if defined(Q_WS_X11)
	Display* dsp = qt_xdisplay();
	Screen* screen = XDefaultScreenOfDisplay(dsp);
	int screen_id = XScreenNumberOfScreen(screen);
	char buf[32];
	snprintf(buf, sizeof(buf), "_NET_SYSTEM_TRAY_S%d", screen_id);
	Atom selection_atom = XInternAtom(dsp, buf, False);
	return XGetSelectionOwner(dsp, selection_atom) != None;
#else
	return false;
#endif
}

#else

// ============ Qt4：原生 QSystemTrayIcon 薄封装 ============

class SystemTrayIcon::Private {
public:
	Private() : native(0), menu(0) {}
	QSystemTrayIcon* native;
	PopupMenu* menu;
};

void SystemTrayIcon::initTray(const QPixmap& pm)
{
	m_baseIcon = pm;
	d->native = new QSystemTrayIcon(QIcon(m_baseIcon), this);
	connect(d->native, SIGNAL(activated(QSystemTrayIcon::ActivationReason)),
		this, SLOT(nativeActivated(TrayActivationReason)));
}

void SystemTrayIcon::fireActivated(int reason)
{
	emit activated(reason);
}

void SystemTrayIcon::nativeActivated(TrayActivationReason reason)
{
	emit activated((int)reason);
}

SystemTrayIcon::SystemTrayIcon(const QPixmap& icon, QObject* parent)
	: QObject(parent), d(new Private()), m_visible(false)
{
	initTray(icon);
}

SystemTrayIcon::SystemTrayIcon(const QString& fileName, QObject* parent)
	: QObject(parent), d(new Private()), m_visible(false)
{
	initTray(QPixmap(fileName));
}

SystemTrayIcon::~SystemTrayIcon()
{
	delete d;
}

void SystemTrayIcon::setIcon(const QPixmap& icon)
{
	m_baseIcon = icon;
	applyIcon();
}

void SystemTrayIcon::setIcon(const QString& fileName)
{
	setIcon(QPixmap(fileName));
}

QPixmap SystemTrayIcon::icon() const
{
	return m_baseIcon;
}

void SystemTrayIcon::setBadgeCount(int count)
{
	m_badgeCount = count;
	applyIcon();
	if (!m_toolTip.isEmpty()) {
		QString t = m_toolTip;
		if (count > 0) {
			t += qFromUtf8(" — 未读 ") + QString::number(count);
		}
		d->native->setToolTip(t);
	}
}

int SystemTrayIcon::badgeCount() const
{
	return m_badgeCount;
}

void SystemTrayIcon::applyIcon()
{
	d->native->setIcon(QIcon(renderBadgeIcon(m_baseIcon, m_badgeCount)));
}

QPixmap SystemTrayIcon::renderBadgeIcon(const QPixmap& base, int count) const
{
	QPixmap pm = base;
	if (count <= 0) {
		return pm;
	}
	if (pm.width() > TRAY_ICON_SIZE || pm.height() > TRAY_ICON_SIZE) {
		pm = pm.scaled(TRAY_ICON_SIZE, TRAY_ICON_SIZE, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	}
	QString t = (count > 999) ? QString::fromLatin1("999") : QString::number(count);
	QPainter p(&pm);
	p.setRenderHint(QPainter::Antialiasing);
	int px = (t.length() >= 3) ? 9 : 10;
	QFont f = p.font();
	f.setBold(true);
	f.setPixelSize(px);
	p.setFont(f);
	QFontMetrics fm(f);
	int tw = fm.boundingRect(t).width();
	int th = fm.boundingRect(t).height();
	int bw = tw + 6;
	if (bw > 15) {
		bw = 15;
	}
	int bh = 8;
	QRect rc(0, 0, bw, bh);
	rc.moveRight(pm.width() - 2);
	rc.moveTop(2);
	p.setPen(Qt::NoPen);
	p.setBrush(QColor(0xD0, 0x20, 0x20));
	p.drawEllipse(QRectF(rc));
	p.setPen(QColor(255, 255, 255));
	p.drawText(rc, Qt::AlignCenter, t);
	p.end();
	return pm;
}

void SystemTrayIcon::setToolTip(const QString& tip)
{
	m_toolTip = tip;
	if (m_badgeCount > 0) {
		d->native->setToolTip(m_toolTip + qFromUtf8(" — 未读 ") + QString::number(m_badgeCount));
	} else {
		d->native->setToolTip(m_toolTip);
	}
}

QString SystemTrayIcon::toolTip() const
{
	return m_toolTip;
}

void SystemTrayIcon::setVisible(bool visible)
{
	if (visible == m_visible) {
		return;
	}
	m_visible = visible;
	d->native->setVisible(visible);
}

bool SystemTrayIcon::isVisible() const
{
	return m_visible;
}

void SystemTrayIcon::show()
{
	setVisible(true);
}

void SystemTrayIcon::hide()
{
	setVisible(false);
}

void SystemTrayIcon::setContextMenu(PopupMenu* menu)
{
	d->menu = menu;
	d->native->setContextMenu(menu);
}

PopupMenu* SystemTrayIcon::contextMenu() const
{
	return d->menu;
}

bool SystemTrayIcon::isSystemTrayAvailable()
{
	return QSystemTrayIcon::isSystemTrayAvailable();
}

#endif
