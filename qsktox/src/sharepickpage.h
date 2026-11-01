#ifndef SHARE_PICK_PAGE_H
#define SHARE_PICK_PAGE_H

#include "page.h"

class SharePickPage : public Page
{
public:
    SharePickPage(QQuickItem* parent = nullptr);

protected:
    void onCreate(const QVariantMap& launchArgs,
                  const QVariantMap& savedState) override;
};

#endif // SHARE_PICK_PAGE_H
