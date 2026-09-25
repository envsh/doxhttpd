#ifndef SYSTEMTRAYICON_H
#define SYSTEMTRAYICON_H

#include <qobject.h>
#include <qstring.h>
#include <qpixmap.h>

// PopupMenu/TrayActivationReason 本地 typedef（不改 compat34.h），
// 参照 EmbeddedMenuBar.h 的 MenuWidget34 先例；#ifdef 均在类体之外（moc 安全）
#ifdef QT3_BUILD
#include <qpopupmenu.h>
typedef QPopupMenu PopupMenu;
typedef int TrayActivationReason;
#else
#include <qmenu.h>
#include <qsystemtrayicon.h>
typedef QMenu PopupMenu;
typedef QSystemTrayIcon::ActivationReason TrayActivationReason;
#endif

/*
 * SystemTrayIcon - 系统托盘图标（Qt3/Qt4 双兼容）
 *
 * API 对齐 Qt4 QSystemTrayIcon 范式。
 * Qt3 分支基于 Psi 0.10 TrayIcon（trayicon*.{h,cpp}，LGPL 2.1）适配。
 * showMessage 气泡：Qt4 原生 / Qt3 自绘（见 docs/systemtrayicon-showmessage.md）。
 */
class SystemTrayIcon : public QObject {
	Q_OBJECT

public:
	// 数值与 Qt4 QSystemTrayIcon::ActivationReason 一致
	enum ActivationReason {
		Unknown = 0,
		Context = 1,
		DoubleClick = 2,
		Trigger = 3,
		MiddleClick = 4
	};

	// 数值与 Qt4 QSystemTrayIcon::MessageIcon 一致
	enum MessageIcon {
		NoIcon = 0,
		Information = 1,
		Warning = 2,
		Critical = 3
	};

	SystemTrayIcon(const QPixmap& icon, QObject* parent = 0);
	SystemTrayIcon(const QString& fileName, QObject* parent = 0);
	~SystemTrayIcon();

	void setIcon(const QPixmap& icon);
	void setIcon(const QString& fileName);
	QPixmap icon() const;

	void setBadgeCount(int count);
	int badgeCount() const;

	void setToolTip(const QString& tip);
	QString toolTip() const;

	void setVisible(bool visible);
	bool isVisible() const;
	void show();
	void hide();

	void setContextMenu(PopupMenu* menu);
	PopupMenu* contextMenu() const;

	// Qt4:委托原生 showMessage(外观随系统,堆叠由系统管)。
	// Qt3:自绘气泡贴托盘图标;堆叠=去重合并×N/FIFO队列(上限3)/同源替换/200ms冷却。
	void showMessage(const QString& title, const QString& message,
		MessageIcon icon = Information, int msecs = 10000);
	// Qt3:清空待显示队列(Qt4 原生无队列,no-op)。调用方在应用窗口激活时调用。
	void clearMessageQueue();
	static bool isSystemTrayAvailable();
	static bool supportsMessages();

signals:
	void activated(int reason);
	void messageClicked();

private slots:
	// Qt4 分支的原生信号中继；Qt3 分支为空实现（不被连接）
	void nativeActivated(TrayActivationReason reason);
	// Qt4 分支原生气泡点击中继；Qt3 分支为空实现
	void nativeMessageClicked();

private:
	friend class SystemTrayIconAdapter;
	friend class TrayBubble;
	void fireActivated(int reason);
	void fireMessageClicked();
	void initTray(const QPixmap& pm);
	void applyIcon();
	QPixmap renderBadgeIcon(const QPixmap& base, int count) const;

	int m_badgeCount;
	QPixmap m_baseIcon;
	QString m_toolTip;
	class Private;
	Private* d;
	bool m_visible;
};

#endif  // SYSTEMTRAYICON_H
