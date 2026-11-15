#include "plugin_manager_dialog.h"
#include "plugin_loader.h"
#include "limelog.h"
#include "compat34.h"
#include <qlayout.h>
#include <qlabel.h>
#include <qpushbutton.h>
#include <qcheckbox.h>
#include <qfont.h>

PluginManagerDialog::PluginManagerDialog(QWidget* parent)
    : QDialog(parent) {
#ifdef QT3_BUILD
    setCaption(qFromUtf8("插件管理"));
#else
    setWindowTitle(qFromUtf8("插件管理"));
#endif
    resize(600, 450);
    setupUi();
    refreshList();
}

PluginManagerDialog::~PluginManagerDialog() {
}

void PluginManagerDialog::setupUi() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel(qFromUtf8("已安装的插件"), this);
    headerLayout->addWidget(titleLabel);

    m_refreshBtn = new QPushButton(qFromUtf8("刷新"), this);
    connect(m_refreshBtn, SIGNAL(clicked()), this, SLOT(onRefresh()));
    headerLayout->addWidget(m_refreshBtn);

    m_enableAllBtn = new QPushButton(qFromUtf8("全部启用"), this);
    connect(m_enableAllBtn, SIGNAL(clicked()), this, SLOT(onEnableAll()));
    headerLayout->addWidget(m_enableAllBtn);

    m_disableAllBtn = new QPushButton(qFromUtf8("全部禁用"), this);
    connect(m_disableAllBtn, SIGNAL(clicked()), this, SLOT(onDisableAll()));
    headerLayout->addWidget(m_disableAllBtn);

    headerLayout->addStretch();
    mainLayout->addLayout(headerLayout);

    m_scrollArea = new ScrollArea(this);
    mainLayout->addWidget(m_scrollArea);

    m_contentWidget = new QWidget(m_scrollArea);
    m_contentLayout = new QVBoxLayout(m_contentWidget);
#ifdef QT3_BUILD
    m_scrollArea->addChild(m_contentWidget);
#else
    m_scrollArea->setWidget(m_contentWidget);
#endif
}

void PluginManagerDialog::addPluginRow(QVBoxLayout* layout, int index, bool isUiApp) {
    const PluginInfo& info = isUiApp
        ? PluginLoader::uiappAt(index)
        : PluginLoader::nouiAt(index);

    QHBoxLayout* row = new QHBoxLayout();

    QCheckBox* cb = new QCheckBox(info.name, m_contentWidget);
    cb->setChecked(info.enabled);
    m_checkboxToIndex[cb] = isUiApp ? index : (PluginLoader::uiappCount() + index);
    connect(cb, SIGNAL(toggled(bool)), this, SLOT(onToggleEnabled()));
    row->addWidget(cb);

    QLabel* verLabel = new QLabel("v" + info.version, m_contentWidget);
    verLabel->setMinimumWidth(50);
    row->addWidget(verLabel);

    QLabel* descLabel = new QLabel(info.description, m_contentWidget);
    row->addWidget(descLabel, 1);

    if (isUiApp) {
        QPushButton* openBtn = new QPushButton(qFromUtf8("打开"), m_contentWidget);
        m_buttonToIndex[openBtn] = index;
        connect(openBtn, SIGNAL(clicked()), this, SLOT(onOpenPlugin()));
        row->addWidget(openBtn);
    }

    QLabel* pathLabel = new QLabel(info.soPath, m_contentWidget);
    row->addWidget(pathLabel);

    layout->addLayout(row);
}

static void setLabelBold(QLabel* label, bool bold) {
    QFont f = label->font();
    f.setBold(bold);
    label->setFont(f);
}

void PluginManagerDialog::refreshList() {
    PluginLoader::rescan();
    m_checkboxToIndex.clear();
    m_buttonToIndex.clear();

    delete m_contentWidget;
    m_contentWidget = new QWidget(m_scrollArea);
    m_contentLayout = new QVBoxLayout(m_contentWidget);
#ifdef QT3_BUILD
    m_scrollArea->addChild(m_contentWidget);
#else
    m_scrollArea->setWidget(m_contentWidget);
#endif

    if (PluginLoader::uiappCount() > 0) {
        QLabel* uiappHeader = new QLabel(qFromUtf8("UI App 插件"), m_contentWidget);
        setLabelBold(uiappHeader, true);
        m_contentLayout->addWidget(uiappHeader);

        for (int i = 0; i < PluginLoader::uiappCount(); i++) {
            addPluginRow(m_contentLayout, i, true);
        }
    }

    if (PluginLoader::nouiCount() > 0) {
        QLabel* nouiHeader = new QLabel(qFromUtf8("Noui 插件"), m_contentWidget);
        setLabelBold(nouiHeader, true);
        m_contentLayout->addWidget(nouiHeader);

        for (int i = 0; i < PluginLoader::nouiCount(); i++) {
            addPluginRow(m_contentLayout, i, false);
        }
    }

    if (PluginLoader::uiappCount() == 0 && PluginLoader::nouiCount() == 0) {
        QLabel* noPlugins = new QLabel(qFromUtf8("未发现任何插件"), m_contentWidget);
        m_contentLayout->addWidget(noPlugins);
    }

    m_contentLayout->addStretch();
}

void PluginManagerDialog::onRefresh() {
    refreshList();
}

void PluginManagerDialog::onEnableAll() {
    int total = PluginLoader::uiappCount() + PluginLoader::nouiCount();
    for (int i = 0; i < total; i++) {
        PluginLoader::setEnabled(i, true);
    }
    refreshList();
}

void PluginManagerDialog::onDisableAll() {
    int total = PluginLoader::uiappCount() + PluginLoader::nouiCount();
    for (int i = 0; i < total; i++) {
        PluginLoader::setEnabled(i, false);
    }
    refreshList();
}

void PluginManagerDialog::onToggleEnabled() {
    QCheckBox* cb = (QCheckBox*)sender();
    if (!cb || !m_checkboxToIndex.contains(cb)) {
        return;
    }
    int index = m_checkboxToIndex[cb];
    const bool on = cb->isChecked();
    PluginLoader::setEnabled(index, on);
    if (!on && index < PluginLoader::uiappCount()) {
        /* App 规则：禁用插件必须立即关闭其窗口（不保留隐藏实例） */
        PluginLoader::destroyUiApp(index);
    }
}

void PluginManagerDialog::onOpenPlugin() {
    QPushButton* btn = (QPushButton*)sender();
    if (!btn || !m_buttonToIndex.contains(btn)) {
        return;
    }
    int idx = m_buttonToIndex[btn];
    if (idx >= 0 && idx < PluginLoader::uiappCount()) {
        PluginLoader::createUiApp(idx, 0);
    }
}
