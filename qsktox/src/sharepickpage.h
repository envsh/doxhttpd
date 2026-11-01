#ifndef SHARE_PICK_PAGE_H
#define SHARE_PICK_PAGE_H

#include "page.h"

#include <QVariant>

class QskLinearBox;

class SharePickPage : public Page
{
public:
    SharePickPage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;
    void onNewIntent(const QVariantMap& launchArgs) override;

private:
    void rebuildFrom(const QVariantMap& launchArgs);

    // 行序 → 打开参数（QQuickTapHandler 行命中换算的数据源）
    QVector<QVariantMap> m_shareTargets;
    QskLinearBox* m_layout = nullptr;
};

#endif // SHARE_PICK_PAGE_H
