#include "ConfigDialog.h"

BoolConfigItem::BoolConfigItem(const QString& key, bool defaultValue, 
                             const QString& label, QWidget* parent)
    : m_key(key), m_label(label), m_default(defaultValue), m_value(defaultValue),
      m_checkbox(0), m_parent(parent)
{
    m_checkbox = new QCheckBox(label, parent);
}

BoolConfigItem::~BoolConfigItem() {}

void BoolConfigItem::load() {
    if (!m_settings) { return; }
#ifdef QT3_BUILD
    m_settings->beginGroup("Settings");
    m_value = m_settings->readBoolEntry(m_key, m_default);
    m_settings->endGroup();
#else
    m_value = m_settings->value(m_key, m_default).toBool();
#endif
    checkBox()->setChecked(m_value);
}

void BoolConfigItem::save() {
    if (!m_settings) { return; }
    m_value = checkBox()->isChecked();
#ifdef QT3_BUILD
    m_settings->beginGroup("Settings");
    m_settings->writeEntry(m_key, m_value);
    m_settings->endGroup();
#else
    m_settings->setValue(m_key, m_value);
#endif
}

StringConfigItem::StringConfigItem(const QString& key, const QString& defaultValue,
                                const QString& label, QWidget* parent)
    : m_key(key), m_label(label), m_default(defaultValue), m_value(defaultValue),
      m_lineEdit(0), m_parent(parent)
{
    m_lineEdit = new QLineEdit(parent);
}

StringConfigItem::~StringConfigItem() {}

void StringConfigItem::load() {
    if (!m_settings) { return; }
#ifdef QT3_BUILD
    m_settings->beginGroup("Settings");
    m_value = m_settings->readEntry(m_key, m_default);
    m_settings->endGroup();
#else
    m_value = m_settings->value(m_key, m_default).toString();
#endif
    lineEdit()->setText(m_value);
}

void StringConfigItem::save() {
    if (!m_settings) { return; }
    m_value = lineEdit()->text();
#ifdef QT3_BUILD
    m_settings->beginGroup("Settings");
    m_settings->writeEntry(m_key, m_value);
    m_settings->endGroup();
#else
    m_settings->setValue(m_key, m_value);
#endif
}

IntConfigItem::IntConfigItem(const QString& key, int defaultValue,
                          const QString& label, int min, int max, QWidget* parent)
    : m_key(key), m_label(label), m_default(defaultValue), m_min(min), m_max(max), m_value(defaultValue),
      m_spinBox(0), m_parent(parent)
{
    m_spinBox = new QSpinBox(parent);
}

IntConfigItem::~IntConfigItem() {}

void IntConfigItem::load() {
    if (!m_settings) { return; }
#ifdef QT3_BUILD
    m_settings->beginGroup("Settings");
    m_value = m_settings->readNumEntry(m_key, m_default);
    m_settings->endGroup();
#else
    m_value = m_settings->value(m_key, m_default).toInt();
#endif
    spinBox()->setValue(m_value);
}

void IntConfigItem::save() {
    if (!m_settings) { return; }
    m_value = spinBox()->value();
#ifdef QT3_BUILD
    m_settings->beginGroup("Settings");
    m_settings->writeEntry(m_key, m_value);
    m_settings->endGroup();
#else
    m_settings->setValue(m_key, m_value);
#endif
}

SelectConfigItem::SelectConfigItem(const QString& key, const QString& defaultValue,
                           const QString& label, const QStringList& options, QWidget* parent)
    : m_key(key), m_label(label), m_default(defaultValue), m_options(options), m_value(defaultValue),
      m_comboBox(0), m_parent(parent)
{
#ifdef QT3_BUILD
    m_comboBox = new QComboBox(false, parent);
#else
    m_comboBox = new QComboBox(parent);
#endif
}

SelectConfigItem::~SelectConfigItem() {}

void SelectConfigItem::load() {
    if (!m_settings) { return; }
#ifdef QT3_BUILD
    m_settings->beginGroup("Settings");
    m_value = m_settings->readEntry(m_key, m_default);
    m_settings->endGroup();
#else
    m_value = m_settings->value(m_key, m_default).toString();
#endif
    int index = 0;
    for (int i = 0; i < m_options.count(); ++i) {
        if (m_options[i] == m_value) {
            index = i;
            break;
        }
    }
#ifdef QT3_BUILD
    comboBox()->setCurrentItem(index);
#else
    comboBox()->setCurrentIndex(index);
#endif
    m_value = comboBox()->currentText();
}

void SelectConfigItem::save() {
    if (!m_settings) { return; }
    m_value = comboBox()->currentText();
#ifdef QT3_BUILD
    m_settings->beginGroup("Settings");
    m_settings->writeEntry(m_key, m_value);
    m_settings->endGroup();
#else
    m_settings->setValue(m_key, m_value);
#endif
}

QString SelectConfigItem::value() {
    return const_cast<SelectConfigItem*>(this)->comboBox()->currentText();
}

ConfigDialog::ConfigDialog(const QString& title, QWidget* parent)
    : QDialog(parent), m_settings(0)
{
#ifdef QT3_BUILD
    setCaption(title);
#else
    setWindowTitle(title);
#endif
    setModal(true);
    m_settingsFile = defaultSettingsFile();
    setupUi();
    resize(600, 400);
}

ConfigDialog::~ConfigDialog() {
    delete m_settings;
}

void ConfigDialog::setupUi() {
#ifdef QT3_BUILD
    QVBoxLayout* mainLayout = new QVBoxLayout(this, 4);
    QHBoxLayout* topLayout = new QHBoxLayout(mainLayout);
    
    QSplitter* splitter = new QSplitter(this);
    splitter->setOrientation(Horizontal);
    
    m_categoryList = new QListBox(splitter);
    m_pageStack = new QWidgetStack(splitter);
    
    m_categoryList->setMaximumWidth(150);
    m_categoryList->setMinimumWidth(100);
    
    splitter->addWidget(m_categoryList);
    splitter->addWidget(m_pageStack);
    
    topLayout->addWidget(splitter);
    
    QWidget* buttonBox = createButtonBox();
    mainLayout->addWidget(buttonBox);
    
    connect(m_categoryList, SIGNAL(highlighted(int)), this, SLOT(onCategoryChanged(int)));
#else
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(4);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    
    QSplitter* splitter = new QSplitter(Qt::Horizontal, this);
    
    m_categoryList = new QListWidget(splitter);
    m_pageStack = new QStackedWidget(splitter);
    
    m_categoryList->setMaximumWidth(150);
    m_categoryList->setMinimumWidth(100);
    
    mainLayout->addWidget(splitter, 1);
    
    QWidget* buttonBox = createButtonBox();
    mainLayout->addWidget(buttonBox);
    
    connect(m_categoryList, SIGNAL(currentRowChanged(int)), this, SLOT(onCategoryChanged(int)));
    m_pageStack->setCurrentIndex(0);
#endif
}

QWidget* ConfigDialog::createButtonBox() {
    QWidget* buttonBox = new QWidget(this);
    buttonBox->setMaximumHeight(40);
    
#ifdef QT3_BUILD
    QHBoxLayout* layout = new QHBoxLayout(buttonBox, 2);
    qSetLayoutMargin(layout, 2);
#else
    QHBoxLayout* layout = new QHBoxLayout(buttonBox);
    layout->setSpacing(6);
    layout->setContentsMargins(2, 2, 2, 2);
#endif
    
    QPushButton* okBtn = new QPushButton("OK", buttonBox);
    QPushButton* cancelBtn = new QPushButton("Cancel", buttonBox);
    QPushButton* applyBtn = new QPushButton("Apply", buttonBox);
    
    connect(okBtn, SIGNAL(clicked()), this, SLOT(onAccepted()));
    connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
    connect(applyBtn, SIGNAL(clicked()), this, SLOT(onApply()));
    
#ifdef QT3_BUILD
    layout->addStretch(1);
#else
    layout->addStretch();
#endif
    layout->addWidget(okBtn);
    layout->addWidget(cancelBtn);
    layout->addWidget(applyBtn);
    
    return buttonBox;
}

void ConfigDialog::addCategory(const QString& name, QWidget* page) {
#ifdef QT3_BUILD
    m_categoryList->insertItem(name);
    int wid = m_pageStack->addWidget(page);
    m_pageIds.append(wid);
#else
    m_categoryList->addItem(name);
    m_pageStack->addWidget(page);
#endif
    if (categoryCount() == 1) {
#ifdef QT3_BUILD
        m_pageStack->raiseWidget(m_pageIds[0]);
#else
        m_categoryList->setCurrentRow(0);
        m_pageStack->setCurrentIndex(0);
#endif
    }
}

int ConfigDialog::categoryCount() const {
    return m_categoryList->count();
}

int ConfigDialog::currentCategory() const {
#ifdef QT3_BUILD
    return m_categoryList->currentItem();
#else
    return m_categoryList->currentRow();
#endif
}

void ConfigDialog::setCurrentCategory(int index) {
#ifdef QT3_BUILD
    m_categoryList->setCurrentItem(index);
#else
    m_categoryList->setCurrentRow(index);
#endif
}

QString ConfigDialog::categoryName(int index) const {
#ifdef QT3_BUILD
    QListBoxItem* item = m_categoryList->item(index);
    if (item) {
        return item->text();
    }
#else
    QListWidgetItem* item = m_categoryList->item(index);
    if (item) {
        return item->text();
    }
#endif
    return QString();
}

void ConfigDialog::loadSettings() {
    if (m_settingsFile.isEmpty()) return;
    
    delete m_settings;
#ifdef QT3_BUILD
    m_settings = new QSettings(QSettings::Ini);
#else
    m_settings = new QSettings(m_settingsFile, QSettings::IniFormat, this);
#endif
    
    for (int i = 0; i < m_configItems.count(); ++i) {
        m_configItems[i]->setSettings(m_settings);
        m_configItems[i]->load();
    }
}

void ConfigDialog::saveSettings() {
    if (m_settingsFile.isEmpty() || !m_settings) { return; }
    
    SettingsChangedMap all;
    for (int i = 0; i < m_configItems.count(); ++i) {
        ConfigItemBase* item = m_configItems[i];
        all[item->itemKey()] = item->widgetValue();
    }
    
    // batch 写文件
    for (auto it = all.begin(); it != all.end(); ++it) {
#ifdef QT3_BUILD
        switch (it.data().type()) {
            case QVariant::Bool:   m_settings->writeEntry(it.key(), it.data().toBool()); break;
            case QVariant::Int:    m_settings->writeEntry(it.key(), it.data().toInt());  break;
            default:               m_settings->writeEntry(it.key(), it.data().toString()); break;
        }
#else
        m_settings->setValue(it.key(), it.value());
#endif
    }
    m_settings->sync();
    
    // 计算 diff
    SettingsChangedMap changed;
    for (int i = 0; i < m_configItems.count(); ++i) {
        ConfigItemBase* item = m_configItems[i];
        if (item->isModified())
            changed[item->itemKey()] = all[item->itemKey()];
    }
    
    emit settingsSaved(changed);
}

void ConfigDialog::setSettingsFile(const QString& filePath) {
    m_settingsFile = filePath;
}

QString ConfigDialog::defaultSettingsFile() {
    return qGetHomePath() + "/.q3tox_settings";
}

void ConfigDialog::registerConfigItem(ConfigItemBase* item) {
    m_configItems.append(item);
    if (m_settings) {
        item->setSettings(m_settings);
    }
}

void ConfigDialog::resizeEvent(QResizeEvent* event) {
    QDialog::resizeEvent(event);
}

void ConfigDialog::onCategoryChanged(int index) {
    qWarning("onCategoryChanged: index=%d", index);
#ifdef QT3_BUILD
    if (index >= 0 && index < (int)m_pageIds.count()) {
        m_pageStack->raiseWidget(m_pageIds[index]);
    }
#else
    m_pageStack->setCurrentIndex(index);
#endif
}



void ConfigDialog::onAccepted() {
    saveSettings();
    accept();
}

void ConfigDialog::onApply() {
    saveSettings();
}

CategoryPage::CategoryPage(const QString& title, QWidget* parent)
    : QWidget(parent), m_mainLayout(0), m_groupBox(0)
{
#ifdef QT3_BUILD
    m_mainLayout = new QVBoxLayout(this, 6);
#else
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(6);
    m_mainLayout->setContentsMargins(6, 6, 6, 6);
#endif
    
    m_groupBox = new ConfigGroupBox(title, this);
    m_mainLayout->addWidget(m_groupBox);
}

CategoryPage::~CategoryPage() {}

void CategoryPage::addWidget(QWidget* widget, int stretch) {
    Q_UNUSED(stretch);
    if (!widget) { return; }
    if (m_groupBox) {
        m_groupBox->addWidget(widget);
    }
}

void CategoryPage::addLayout(QLayout* layout) {
    if (m_groupBox) {
        m_groupBox->addLayout(layout);
    }
}

void CategoryPage::addSpacing(int size) {
    Q_UNUSED(size);
}

void CategoryPage::addStretch(int stretch) {
    if (m_groupBox) {
        m_groupBox->addStretch(stretch);
    }
}

void CategoryPage::addLabeledControl(const QString& label, QWidget* control) {
    if (!control) { return; }
    if (m_groupBox) {
        m_groupBox->addLabeledControl(label, control);
    }
}

ConfigGroupBox::ConfigGroupBox(const QString& title, QWidget* parent)
    : QGroupBox(title, parent)
#ifdef QT3_BUILD
    , m_grid(0)
#else
    , m_grid(0)
#endif
{
#ifdef QT3_BUILD
    m_grid = new QGridLayout(this, 0, 2);
#else
    m_grid = new QGridLayout(this);
    m_grid->setSpacing(2);
    m_grid->setVerticalSpacing(2);
#endif
}

ConfigGroupBox::~ConfigGroupBox() {}

void ConfigGroupBox::addWidget(QWidget* widget, int stretch) {
    Q_UNUSED(stretch);
    if (!widget) { return; }
#ifdef QT3_BUILD
    if (m_grid) {
        if (widget->isWidgetType()) {
            widget->reparent(this, QPoint(0, 0), false);
            widget->setMinimumHeight(22);
            widget->setMaximumHeight(22);
        }
        int row = m_grid->numRows();
        m_grid->addWidget(widget, row, 0);
    }
#else
    if (m_grid) {
        int row = m_grid->rowCount();
        m_grid->addWidget(widget, row, 0, 1, 2);
    }
#endif
}

void ConfigGroupBox::addLayout(QLayout* layout) {
#ifdef QT3_BUILD
    if (m_grid) {
        int row = m_grid->numRows();
        m_grid->addLayout(layout, row, 0);
    }
#else
    if (m_grid) {
        int row = m_grid->rowCount();
        m_grid->addLayout(layout, row, 0, 1, 2);
    }
#endif
}

void ConfigGroupBox::addSpacing(int size) {
    Q_UNUSED(size);
}

void ConfigGroupBox::addStretch(int stretch) {
    Q_UNUSED(stretch);
#ifdef QT3_BUILD
    if (m_grid) {
        m_grid->setRowStretch(m_grid->numRows(), 1);
    }
#else
    if (m_grid) {
        if (stretch > 0) {
            QSpacerItem* spacer = new QSpacerItem(1, stretch, QSizePolicy::Minimum, QSizePolicy::Expanding);
            m_grid->addItem(spacer, m_grid->rowCount(), 0);
        }
    }
#endif
}

void ConfigGroupBox::addLabeledControl(const QString& label, QWidget* control) {
    if (!control) { return; }
#ifdef QT3_BUILD
    if (m_grid) {
        int row = m_grid->numRows();
        if (control->isWidgetType()) {
            control->reparent(this, QPoint(0, 0), false);
            control->setMinimumSize(150, 22);
            control->setMaximumHeight(22);
        }
        QLabel* lbl = new QLabel(label, this);
        lbl->setMinimumHeight(22);
        lbl->setMaximumHeight(22);
        m_grid->addWidget(lbl, row, 0);
        m_grid->addWidget(control, row, 1);
    }
#else
    if (m_grid) {
        int row = m_grid->rowCount();
        QLabel* lbl = new QLabel(label, this);
        lbl->setBuddy(control);
        m_grid->addWidget(lbl, row, 0);
        m_grid->addWidget(control, row, 1);
    }
#endif
}