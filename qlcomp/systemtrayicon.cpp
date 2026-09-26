#include "systemtrayicon.h"
#include "compatcore34.h"

#include <stdio.h>
#include <qimage.h>
#include <qpainter.h>
#include <qcolor.h>
#include <qfontmetrics.h>

#ifdef QT3_BUILD
#include "trayicon.h"
#include <qwidget.h>
#include <qdesktopwidget.h>
#include <qvaluelist.h>
#include <qstringlist.h>
#include <qpointarray.h>
#include <qbitmap.h>
#include <qapplication.h>
#ifdef Q_WS_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#endif
#else
#include <qsystemtrayicon.h>
#endif

static const int TRAY_ICON_SIZE = 22;

#ifdef QT3_BUILD

// ============ Qt3：自绘托盘气泡（showMessage） ============
// 无 Q_OBJECT（用 startTimer/timerEvent），不产生新 moc 文件。
// 堆叠：去重合并×N / 同源（title 同）就地替换 / FIFO 队列（上限 MAX_QUEUE，满丢最旧）/
// 计时只在显示时启动 / 关闭后 COOLDOWN_MS 冷却再晋升队头。
// 箭头用 QBitmap mask（QRegion 多边形思想，Qt3 无 QPainterPath）。
class TrayBubble;

class SystemTrayIcon::Private {
public:
	Private() : tray(0), menu(0), bubble(0) {}
	TrayIcon* tray;      // 用基类指针：私有实现放类定义之后，避免类型未完成
	PopupMenu* menu;
	TrayBubble* bubble;
};

class TrayBubble : public QWidget {
public:
	enum TimerMode { Idle, Visible, Cooldown };

	struct Item {
		Item() : iconType(0), msecs(10000), repeat(1) {}
		int iconType;
		QString title;
		QString message;
		int msecs;
		int repeat;  // 去重合并计数
	};

	TrayBubble(SystemTrayIcon* owner)
		: QWidget(0, 0, WStyle_StaysOnTop | WStyle_Customize | WStyle_NoBorder | WStyle_Tool)
		, m_owner(owner), m_hasCurrent(false), m_timerId(0), m_timerMode(Idle)
		, m_arrow(ArrowNone)
	{
		setFocusPolicy(NoFocus);   // 气泡不接收键盘焦点，点击仍走 mousePressEvent
	}

	// 入队策略见类注释；全部在调用方（GUI）线程，无锁
	void showMessage(int iconType, const QString& title, const QString& message, int msecs)
	{
		Item it;
		it.iconType = iconType;
		it.title = title;
		it.message = message;
		it.msecs = (msecs < 0) ? 10000 : msecs;
		if (m_hasCurrent && m_timerMode != Cooldown) {
			if (m_current.title == it.title && m_current.message == it.message) {
				++m_current.repeat; // 去重：×N
			} else if (m_current.title == it.title) {
				m_current = it;     // 同源替换（状态流）
				m_current.repeat = 1;
			} else {
				enqueue(it);        // 不同源：FIFO
				return;
			}
			prepareAndPos();        // repeat 预留 / 替换内容 → 同步重排
			buildMask();
			restartVisibleTimer();
			showMyself();
			return;
		}
		enqueue(it);                  // 空闲或冷却中：先入队再晋升
		promoteNext();
	}

	// 主窗激活时清空待显示队列（当前气泡不清）
	void clearMessageQueue()
	{
		m_queue.clear();
	}

private:
	void enqueue(const Item& it)
	{
		if (m_queue.count() >= MAX_QUEUE) {
			m_queue.remove(m_queue.begin());   // 满丢最旧（防刷屏）
		}
		m_queue.append(it);
	}

	void promoteNext()
	{
		if (m_timerMode == Cooldown) {
			return;                          // 冷却中，计时结束会自动来
		}
		if (m_queue.isEmpty() && !m_hasCurrent) {
			hideWindow();
			return;
		}
		if (m_queue.isEmpty()) {
			return;
		}
		m_current = m_queue.first();
		m_queue.remove(m_queue.begin());
		m_hasCurrent = true;
		prepareAndPos();
		buildMask();
		restartVisibleTimer();
		showMyself();
	}

	void closeCurrentAndPromote()
	{
		if (!m_hasCurrent) {
			return;
		}
		m_hasCurrent = false;
		hideWindow();
		if (m_queue.isEmpty()) {
			return;
		}
		m_timerMode = Cooldown;              // 冷却后晋升
		m_timerId = startTimer(COOLDOWN_MS);
	}

	void hideWindow()
	{
		if (m_timerId) {
			killTimer(m_timerId);
			m_timerId = 0;
		}
		m_timerMode = Idle;
		hide();
	}

	void restartVisibleTimer()
	{
		if (m_timerId) {
			killTimer(m_timerId);
			m_timerId = 0;
		}
		m_timerMode = Visible;
		if (m_current.msecs > 0) {            // 0=常显至点击
			m_timerId = startTimer(m_current.msecs);
		}
	}

	void showMyself()
	{
#ifdef Q_WS_X11
		// Qt3 无 WA_ShowWithoutActivating / WA_X11DoNotAcceptFocus，
		// 顶级窗默认 XWMHints.input=True → map 时 WM 把键盘焦点交给气泡，抢走原活动窗口焦点。
		Display* dpy = QPaintDevice::x11Display();
		Window w = winId();
		XWMHints* h = XGetWMHints(dpy, w);
		if (!h) {
			h = XAllocWMHints();
		} else {
			h->flags |= InputHint;
		}
		h->input = False;
		XSetWMHints(dpy, w, h);
		XFree(h);
		Atom userTime = XInternAtom(dpy, "_NET_WM_USER_TIME", False);
		if (userTime != None) {
			long zero = 0;
			XChangeProperty(dpy, w, userTime, XA_CARDINAL, 32,
			                PropModeReplace, (unsigned char*)&zero, 1);
		}
#endif
		QWidget::show();
		raise();
	}

protected:
	void paintEvent(QPaintEvent*)
	{
		QPainter p(this);
		p.setBrush(QColor(0x1a, 0x1a, 0x1a));
		p.setPen(Qt::NoPen);
		int contentTop = (m_arrow == ArrowUp) ? ARROW_EXT : 0;
		int bodyH = height() - ((m_arrow == ArrowNone) ? 0 : ARROW_EXT);
		p.drawRoundRect(0, contentTop, width(), bodyH, 15, 15);
		if (m_arrow == ArrowUp) {             // 气泡在图标下方，三角从顶边伸出指向图标
			QPointArray tri(3);
			tri.setPoint(0, width() / 2 - 6, ARROW_EXT);
			tri.setPoint(1, width() / 2 + 6, ARROW_EXT);
			tri.setPoint(2, width() / 2, 0);
			p.drawPolygon(tri);
		} else if (m_arrow == ArrowDown) {    // 气泡在图标上方，三角从底边伸出
			QPointArray tri(3);
			tri.setPoint(0, width() / 2 - 6, bodyH);
			tri.setPoint(1, width() / 2 + 6, bodyH);
			tri.setPoint(2, width() / 2, height());
			p.drawPolygon(tri);
		}
		int left = MARGIN_L;
		if (m_current.iconType != 0) {
			int icY = contentTop + (bodyH - ICON_SIZE) / 2;   // 图标垂直居中于气泡体
			p.setBrush(iconColor());
			p.drawEllipse(left, icY, ICON_SIZE, ICON_SIZE);
			QFont b = p.font();
			b.setBold(true);
			b.setPixelSize(ICON_CHAR_SIZE);   // 图标字符字号（独立于标题/内容）
			p.setFont(b);
			p.setPen(Qt::white);
			p.drawText(left, icY, ICON_SIZE, ICON_SIZE, Qt::AlignCenter, iconChar());
			left += ICON_SIZE + MARGIN_GAP;
		}
		QFontMetrics fmb(font());
		QFontMetrics tfm = titleFm();
		// 标题/内容用气泡基础字体（font()），勿用 p.font()——painter 当前字体被图标字符字号污染
		QFont f = font();
		f.setBold(true);
		p.setFont(f);
		p.setPen(QColor(0xFF, 0xFF, 0xFF));
		QStringList tl = wrapText(m_current.title, titleWrapW(), tfm);
		for (int i = 0; i < (int)tl.count(); ++i) {
			p.drawText(left, contentTop + 8 + (i + 1) * tfm.height(), tl[i]);
		}
		if (m_current.repeat > 1) {           // "×N" 右上角
			f.setBold(false);
			p.setFont(f);
			p.setPen(QColor(0xFF, 0xC1, 0x07));
			p.drawText(W_WIDTH - 12 - repeatW(), contentTop + 8 + tfm.height(), repeatLabel());
		}
		int y = contentTop + 8 + tl.count() * tfm.height() + 4;
		f.setBold(false);
		p.setFont(f);
		p.setPen(QColor(0xDD, 0xDD, 0xDD));
		QStringList ml = wrapText(m_current.message, msgWrapW(), fmb);
		for (int i = 0; i < (int)ml.count(); ++i) {
			p.drawText(left, y + (i + 1) * fmb.height(), ml[i]);
		}
		p.end();
	}

	void mousePressEvent(QMouseEvent*)
	{
		if (m_hasCurrent) {
			m_owner->fireMessageClicked();
		}
		closeCurrentAndPromote();
	}

	void timerEvent(QTimerEvent*)
	{
		if (m_timerMode == Visible) {         // 显示超时
			closeCurrentAndPromote();
		} else if (m_timerMode == Cooldown) { // 冷却结束晋升
			if (m_timerId) {
				killTimer(m_timerId);
				m_timerId = 0;
			}
			m_timerMode = Idle;
			promoteNext();
		}
	}

private:
	// 与 paintEvent 完全一致的排版参数（prepare 与 paint 共用，防纵向溢出）
	QFontMetrics titleFm() const
	{
		QFont f = font();
		f.setBold(true);
		return QFontMetrics(f);
	}

	QString repeatLabel() const
	{
		return qFromUtf8("×") + QString::number(m_current.repeat);
	}

	int contentLeft() const
	{
		return (m_current.iconType != 0) ? (MARGIN_L + ICON_SIZE + MARGIN_GAP) : MARGIN_L;
	}

	// "×N" 文本宽（regular 度量，与绘制一致）；repeat<=1 时 0
	int repeatW() const
	{
		if (m_current.repeat <= 1) {
			return 0;
		}
		return QFontMetrics(font()).width(repeatLabel());
	}

	// 总宽(W_WIDTH) - 图标占位 - 右缘 - (repeat>1 ? "×N"宽+6px 间隙 : 0)
	int titleWrapW() const
	{
		return W_WIDTH - contentLeft() - 12
			- ((m_current.repeat > 1) ? repeatW() + 6 : 0);
	}

	int msgWrapW() const
	{
		return W_WIDTH - contentLeft() - 12;
	}

	QColor iconColor() const
	{
		switch (m_current.iconType) {
			case SystemTrayIcon::Warning:
				return QColor(0xFF, 0xC1, 0x07);
			case SystemTrayIcon::Critical:
				return QColor(0xD0, 0x20, 0x20);
			default:
				return QColor(0x00, 0x9E, 0xFF);
		}
	}

	QString iconChar() const
	{
		switch (m_current.iconType) {
			case SystemTrayIcon::Warning:
				return QString("!");
			case SystemTrayIcon::Critical:
				return QString("x");
			default:
				return QString("i");
		}
	}

	// 按宽度换行：空格分词；超长词/连续 CJK 逐字切分
	static QStringList wrapText(const QString& s, int maxW, const QFontMetrics& fm)
	{
		QStringList out;
		QString rest = s;
		QString cur;
		while (!rest.isEmpty()) {
			int sp = rest.find(' ');
			QString w = (sp < 0) ? rest : rest.left(sp);
			rest = (sp < 0) ? QString() : rest.mid(sp + 1);
			while (!w.isEmpty() && fm.width(w) > maxW) {
				int cut = 1;
				while (cut < (int)w.length() && fm.width(w.left(cut + 1)) <= maxW) {
					++cut;
				}
				if (!cur.isEmpty()) {
					out += cur;
					cur = QString();
				}
				out += w.left(cut);
				w = w.mid(cut);
			}
			if (!cur.isEmpty()) {
				cur += QChar(' ');
			}
			cur += w;
		}
		if (!cur.isEmpty()) {
			out += cur;
		}
		if (out.isEmpty()) {
			out += QString();
		}
		return out;
	}

	// 一步完成：布局 + 箭头方向 + 桌面位置（高度依赖箭头，故合并计算）。
	// 上/下自适应贴近托盘图标；无可用几何回退桌面右下角。
	void prepareAndPos()
	{
		QFontMetrics fmb(font());
		QFontMetrics tfm = titleFm();
		QStringList tl = wrapText(m_current.title, titleWrapW(), tfm);
		QStringList ml = wrapText(m_current.message, msgWrapW(), fmb);
		int bodyH = 8 + tl.count() * tfm.height() + 4 + ml.count() * fmb.height() + 12;
		if (bodyH < ICON_SIZE + 16) {
			bodyH = ICON_SIZE + 16;   // 高度下限：保证图标区完整
		}
		QRect tg = m_owner->d->tray->trayIconGeometry();
		QRect scr = QApplication::desktop()->availableGeometry(-1);
		int h, x, y;
		if (tg.isEmpty()) {
			m_arrow = ArrowNone;
			h = bodyH;
			int m = 16;
			x = scr.right() - W_WIDTH - m;
			y = scr.bottom() - h - m;
		} else {
			int need = bodyH + ARROW_EXT + 6;
			if (tg.top() - need >= scr.top()) {
				m_arrow = ArrowDown;          // 气泡在上，三角朝下指向图标
				h = bodyH + ARROW_EXT;
				y = tg.top() - h - 6;
			} else if (tg.bottom() + need <= scr.bottom()) {
				m_arrow = ArrowUp;            // 气泡在下，三角朝上指向图标
				h = bodyH + ARROW_EXT;
				y = tg.bottom() + 6;
			} else {
				m_arrow = ArrowNone;
				h = bodyH;
				y = scr.top() + 10;
			}
			x = tg.right() - W_WIDTH;
			x = (x < scr.left()) ? scr.left() + 8 : x;
			x = (x + W_WIDTH > scr.right()) ? scr.right() - W_WIDTH - 8 : x;
		}
		setFixedSize(W_WIDTH, h);
		move(x, y);
	}

	// 圆角体 + 三角 → QBitmap mask（Qt3 无 QPainterPath 的等价物）
	void buildMask()
	{
		QBitmap bm(width(), height());
		bm.fill(Qt::color0);
		QPainter p(&bm);
		p.setPen(Qt::NoPen);
		p.setBrush(Qt::color1);
		int contentTop = (m_arrow == ArrowUp) ? ARROW_EXT : 0;
		int bodyH = height() - ((m_arrow == ArrowNone) ? 0 : ARROW_EXT);
		p.drawRoundRect(0, contentTop, width(), bodyH, 15, 15);
		if (m_arrow == ArrowUp) {
			QPointArray tri(3);
			tri.setPoint(0, width() / 2 - 6, ARROW_EXT);
			tri.setPoint(1, width() / 2 + 6, ARROW_EXT);
			tri.setPoint(2, width() / 2, 0);
			p.drawPolygon(tri);
		} else if (m_arrow == ArrowDown) {
			QPointArray tri(3);
			tri.setPoint(0, width() / 2 - 6, bodyH);
			tri.setPoint(1, width() / 2 + 6, bodyH);
			tri.setPoint(2, width() / 2, height());
			p.drawPolygon(tri);
		}
		p.end();
		setMask(bm);
	}

private:
	static const int MAX_QUEUE = 3;
	static const int COOLDOWN_MS = 200;
	static const int ARROW_EXT = 10;
	static const int W_WIDTH = 300;
	static const int ICON_SIZE = 48;    // 对齐 Qt5 QBalloonTip 的 48px 消息图标
	static const int ICON_CHAR_SIZE = ICON_SIZE - 12;  // 图标字符字号：比图标小一圈，与标题/内容各自独立
	static const int MARGIN_L = 10;    // 左边距
	static const int MARGIN_GAP = 10;  // 图标与文本间隙
	enum ArrowSide { ArrowNone, ArrowUp, ArrowDown };

	SystemTrayIcon* m_owner;
	QValueList<Item> m_queue;
	Item m_current;
	bool m_hasCurrent;
	int m_timerId;
	int m_timerMode;
	int m_arrow;
};

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

void SystemTrayIcon::fireMessageClicked()
{
	emit messageClicked();
}

void SystemTrayIcon::nativeMessageClicked()
{
	// Qt3 自绘气泡无原生信号，空实现（不被连接）
}

void SystemTrayIcon::showMessage(const QString& title, const QString& message,
								 SystemTrayIcon::MessageIcon icon, int msecs)
{
	if (!m_visible) {
		return;
	}
	if (!d->bubble) {
		d->bubble = new TrayBubble(this);
	}
	d->bubble->showMessage((int)icon, title, message, msecs);
}

void SystemTrayIcon::clearMessageQueue()
{
	if (d->bubble) {
		d->bubble->clearMessageQueue();
	}
}

bool SystemTrayIcon::supportsMessages()
{
#if defined(Q_WS_X11)
	return true;  // 自绘气泡不依赖托盘实现
#else
	return false;
#endif
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
	if (d->bubble) {
		delete d->bubble;   // 顶层窗口无 Qt 父对象，需显式释放
	}
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
	connect(d->native, SIGNAL(messageClicked()), this, SLOT(nativeMessageClicked()));
}

void SystemTrayIcon::fireActivated(int reason)
{
	emit activated(reason);
}

void SystemTrayIcon::fireMessageClicked()
{
	emit messageClicked();  // Qt4 经原生 messageClicked 中继，此方法不被调用
}

void SystemTrayIcon::nativeMessageClicked()
{
	emit messageClicked();
}

void SystemTrayIcon::showMessage(const QString& title, const QString& message,
								 SystemTrayIcon::MessageIcon icon, int msecs)
{
	if (!m_visible) {
		return;
	}
	d->native->showMessage(title, message,
		(QSystemTrayIcon::MessageIcon)icon, msecs);
}

void SystemTrayIcon::clearMessageQueue()
{
	// Qt4 原生队列由系统管理，不可清，no-op
}

bool SystemTrayIcon::supportsMessages()
{
	return QSystemTrayIcon::supportsMessages();
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
