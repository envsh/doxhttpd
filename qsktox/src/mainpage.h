#ifndef MAIN_PAGE_H
#define MAIN_PAGE_H

#include "page.h"

class QskTextLabel;
class QskPushButton;
class QTimer;
class ChannelListWidget;

class MainPage : public Page
{
    Q_OBJECT
public:
    MainPage(QQuickItem* parent = nullptr);

    void showToast(const QString& msg, int durationMs = 2000);
    void handleShareIntent(const QString& action, const QString& mimeType,
                           const QString& text, const QString& urisJson);
    bool keepScreenOn() const { return m_keepScreenOn; }
    void setKeepScreenOn(bool on);
    ChannelListWidget* channelList() const { return m_channelList; }

Q_SIGNALS:
    void keepScreenOnChanged(bool on);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;
    void onNewIntent(const QVariantMap& launchArgs) override;
    void onPause() override;
    void onResume() override;

private:
    void showChannelMenu(int row, const QPointF& pos);
    void setActiveTab(int index);

    QskTextLabel* m_toastLabel = nullptr;
    QTimer* m_toastTimer = nullptr;
    ChannelListWidget* m_channelList = nullptr;
    bool m_keepScreenOn = true;

    struct NavTab {
        QskPushButton* btn = nullptr;
        QskTextLabel* label = nullptr;
    };
    NavTab m_navTabs[4];
    int m_activeTab = 0;
};

void registerMainPage(MainPage* page);

#endif
