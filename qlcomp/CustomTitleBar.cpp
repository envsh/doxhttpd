#include "CustomTitleBar.h"
#include "FramelessHelper.h"
#include "translator.h"

CustomTitleBar::CustomTitleBar(QWidget* parent)
    :     QWidget(parent) {
#ifdef QT3_BUILD
    setMinimumHeight(32);
    setMaximumHeight(32);
#elif defined(Q_OS_MAC) || defined(Q_OS_MACX) || defined(Q_OS_DARWIN)
    setMinimumHeight(32);       // macOS：40px 偏高约 50% 空白（原生控件矮），对齐 Qt3 紧凑高度
    setMaximumHeight(32);
#else
    setMinimumHeight(40);       // 其它平台（Linux 等）维持现状
    setMaximumHeight(40);
#endif

    QBoxLayout* layout = qNewBoxLayout(this, QBoxLayout::LeftToRight, 0, 0);

    appMenuBtn = new QPushButton(qFromUtf8("≡"), this);
    appMenuBtn->setFixedSize(30, 30);
    layout->addWidget(appMenuBtn, 0);
    layout->addSpacing(4);

    menubar = new EmbeddedMenuBar(this);
    menubar->init();
    layout->addWidget(menubar, 0);

    titleLabel = new QLabel(_("app_title"), this);
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel, 1);

    sysMenuBtn = new QPushButton(qFromUtf8("≡"), this);
    sysMenuBtn->setFixedSize(30, 30);
    layout->addWidget(sysMenuBtn, 0);

    minBtn = new QPushButton(qFromUtf8("─"), this);
    minBtn->setFixedSize(30, 30);
    layout->addWidget(minBtn, 0);

    maxBtn = new QPushButton(qFromUtf8("□"), this);
    maxBtn->setFixedSize(30, 30);
    layout->addWidget(maxBtn, 0);

    closeBtn = new QPushButton(qFromUtf8("✕"), this);
    closeBtn->setFixedSize(30, 30);
    layout->addWidget(closeBtn, 0);

    QObject::connect(appMenuBtn, SIGNAL(clicked()), this, SIGNAL(appMenuClicked()));
}

void CustomTitleBar::connectFramelessHelper(FramelessHelper* helper) {
    if (!helper) { return; }
    helper->setTitleBar(this);
#ifdef QT3_BUILD
    QObject::connect(sysMenuBtn, SIGNAL(clicked()), helper, SLOT(showSystemMenuFromSys()));
    QObject::connect(closeBtn, SIGNAL(clicked()), helper->parent(), SLOT(close()));
    QObject::connect(minBtn, SIGNAL(clicked()), helper->parent(), SLOT(showMinimized()));
    QObject::connect(maxBtn, SIGNAL(clicked()), helper, SLOT(toggleMaximize()));
#else
    QObject::connect(sysMenuBtn, SIGNAL(clicked()), helper, SLOT(showSystemMenuFromSys()));
    QObject::connect(closeBtn, SIGNAL(clicked()), helper, SLOT(onCloseClicked()));
    QObject::connect(minBtn, SIGNAL(clicked()), helper, SLOT(onMinClicked()));
    QObject::connect(maxBtn, SIGNAL(clicked()), helper, SLOT(onMaxClicked()));
#endif
}

void CustomTitleBar::toggleMenu() {
    m_menuVisible = !m_menuVisible;
    if (m_menuVisible) {
        menubar->show();
    } else
        menubar->hide();
}

void CustomTitleBar::setLabel(const QString& text) {
    if (titleLabel) { titleLabel->setText(text); }
}

void CustomTitleBar::addTitleWidget(QWidget* w, int stretch) {
    if (!w) { return; }
    QBoxLayout* lay = static_cast<QBoxLayout*>(layout());
    if (!lay) { return; }
#ifdef QT3_BUILD
    int idx = lay->findWidget(sysMenuBtn);          // qlayout.h:415 返回 -1 表示未找到
#else
    int idx = lay->indexOf(sysMenuBtn);
#endif
    if (idx < 0) { lay->addWidget(w, stretch); return; }
    lay->insertWidget(idx, w, stretch);             // 插到 sysMenuBtn 之前 = 标题与右侧之间
}

QString CustomTitleBar::label() const {
    return titleLabel ? titleLabel->text() : QString();
}
