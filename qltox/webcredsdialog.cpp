#include "webcredsdialog.h"
#include <qobject.h>
#include <qcombobox.h>
#include <qmessagebox.h>
#include <qfont.h>
#ifdef QT3_BUILD
#include <qlistbox.h>
#else
#include <qlistwidget.h>
#endif

namespace {
const char* const kModeLabels[] = { "明文", "令牌加密", "口令加密" };

// 口令输入弹窗（Qt3/Qt4 的 QInputDialog 均无统一的 getPassword，自建模态框）。
QString qPasswordPrompt(QWidget* parent, const QString& title,
                        const QString& label, bool* ok) {
    QDialog dlg(parent);
    qSetWindowTitle(&dlg, title);
    QBoxLayout* lay = qNewBoxLayout(&dlg, QBoxLayout::TopToBottom, 10, 10);
    lay->addWidget(new QLabel(label, &dlg));
    QLineEdit* edit = new QLineEdit(&dlg);
    edit->setEchoMode(QLineEdit::Password);
    lay->addWidget(edit);
    QBoxLayout* btns = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    btns->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    QPushButton* okBtn = new QPushButton(qFromUtf8("确定"), &dlg);
    QObject::connect(okBtn, SIGNAL(clicked()), &dlg, SLOT(accept()));
    btns->addWidget(okBtn);
    QPushButton* cancelBtn = new QPushButton(qFromUtf8("取消"), &dlg);
    QObject::connect(cancelBtn, SIGNAL(clicked()), &dlg, SLOT(reject()));
    btns->addWidget(cancelBtn);
    lay->addLayout(btns);
    if (dlg.exec() == QDialog::Accepted) {
        *ok = true;
        return edit->text();
    }
    *ok = false;
    return QString();
}
}

// ── WebCredDialog ──────────────────────────────────────────────

WebCredDialog::WebCredDialog(QWidget* parent)
    : QDialog(parent), listWidget(nullptr), revealBtn(nullptr), revealed(false) {
    qSetWindowTitle(this, qFromUtf8("Web 凭据配置"));
    resize(660, 360);

    QBoxLayout* mainLayout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 10, 10);

    QFont monoFont("monospace", 10);
    if (!monoFont.exactMatch()) {
        monoFont.setStyleHint(QFont::TypeWriter);
    }
#ifdef QT3_BUILD
    QListBox* lb = new QListBox(this);
    lb->setFont(monoFont);
    listWidget = lb;
    mainLayout->addWidget(lb);
#else
    QListWidget* lw = new QListWidget(this);
    lw->setFont(monoFont);
    listWidget = lw;
    mainLayout->addWidget(lw);
#endif

    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    QPushButton* editBtn = new QPushButton(qFromUtf8("配置/修改"), this);
    QObject::connect(editBtn, SIGNAL(clicked()), this, SLOT(onEdit()));
    btnLayout->addWidget(editBtn);
    QPushButton* removeBtn = new QPushButton(qFromUtf8("删除"), this);
    QObject::connect(removeBtn, SIGNAL(clicked()), this, SLOT(onRemove()));
    btnLayout->addWidget(removeBtn);
    revealBtn = new QPushButton(qFromUtf8("显示明文"), this);
    QObject::connect(revealBtn, SIGNAL(clicked()), this, SLOT(onToggleReveal()));
    btnLayout->addWidget(revealBtn);
    QPushButton* refreshBtn = new QPushButton(qFromUtf8("刷新"), this);
    QObject::connect(refreshBtn, SIGNAL(clicked()), this, SLOT(onRefresh()));
    btnLayout->addWidget(refreshBtn);
    btnLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    QPushButton* closeBtn = new QPushButton(qFromUtf8("关闭"), this);
    QObject::connect(closeBtn, SIGNAL(clicked()), this, SLOT(accept()));
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);

    refresh();
}

int WebCredDialog::currentRow() const {
#ifdef QT3_BUILD
    return ((QListBox*)listWidget)->currentItem();
#else
    return ((QListWidget*)listWidget)->currentRow();
#endif
}

QString WebCredDialog::rowText(const WebCredListEntry& e) const {
    const QString statusName = qFromUtf8(webCredStatusName(e.status));
    QString valuePart = qFromUtf8(" ");
    switch (e.status) {
    case kWebCredNone:
        valuePart = qFromUtf8("未配置");
        break;
    case kWebCredConstant:
        valuePart = qFromUtf8("内置常量（侧车未配置）");
        break;
    case kWebCredPlain:
        valuePart = (revealed && e.ok)
            ? qFromUtf8(e.value.c_str()) : qFromUtf8("<明文已保存>");
        break;
    case kWebCredTokenEnc:
    case kWebCredPassEnc:
        valuePart = (revealed && e.ok)
            ? qFromUtf8(e.value.c_str()) : qFromUtf8("<加密>");
        break;
    case kWebCredBroken:
        valuePart = qFromUtf8("<解不开>");
        break;
    }
    return QString("%1  |  %2  |  %3")
        .arg(qFromUtf8(e.name.c_str()), -22)
        .arg(statusName, -12)
        .arg(valuePart);
}

void WebCredDialog::refresh() {
    std::vector<WebCredListEntry> out;
    std::string err;
    QByteArray passUtf8 = qToUtf8(sessionPass);
    if (!webCredsList(revealed, passUtf8.data(), out, err)) {
        QMessageBox::warning(this, qFromUtf8("Web 凭据"), qFromUtf8(err.c_str()));
        return;
    }
    entries = out;
#ifdef QT3_BUILD
    QListBox* lb = (QListBox*)listWidget;
    lb->clear();
    for (size_t i = 0; i < entries.size(); ++i) {
        lb->insertItem(rowText(entries[i]));
    }
#else
    QListWidget* lw = (QListWidget*)listWidget;
    lw->clear();
    for (size_t i = 0; i < entries.size(); ++i) {
        new QListWidgetItem(rowText(entries[i]), lw);
    }
#endif
    revealBtn->setText(revealed ? qFromUtf8("遮蔽值") : qFromUtf8("显示明文"));
}

void WebCredDialog::onEdit() {
    const int row = currentRow();
    QString field;
    WebCredStatus st = kWebCredNone;
    if (row >= 0 && row < (int)entries.size()) {
        field = qFromUtf8(entries[row].name.c_str());
        st = entries[row].status;
    }
    WebCredEditDialog dlg(this,
                          field.isEmpty() ? std::string() : std::string(qToUtf8(field).data()),
                          st, sessionPass);
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }
    const WebCredMode mode = dlg.getMode();
    const QString value = dlg.getValue();
    if (value.isEmpty()) {
        QMessageBox::warning(this, qFromUtf8("Web 凭据"), qFromUtf8("值不能为空。"));
        return;
    }
    QString passText = dlg.getPassphrase();
    if (passText.isEmpty()) {
        passText = sessionPass;
    }
    if (mode == kWebCredPassMode && passText.isEmpty()) {
        QMessageBox::warning(this, qFromUtf8("口令加密"), qFromUtf8("口令加密字段需要口令。"));
        return;
    }
    QByteArray nameUtf8 = qToUtf8(dlg.getFieldName());
    QByteArray valueUtf8 = qToUtf8(value);
    QByteArray passUtf8 = qToUtf8(passText);
    std::string err;
    if (!webCredsSetField(nameUtf8.data(), valueUtf8.data(), mode, passUtf8.data(), err)) {
        QMessageBox::warning(this, qFromUtf8("保存失败"), qFromUtf8(err.c_str()));
        return;
    }
    if (!dlg.getPassphrase().isEmpty()) {
        sessionPass = dlg.getPassphrase();
    }
    refresh();
}

void WebCredDialog::onRemove() {
    const int row = currentRow();
    if (row < 0 || row >= (int)entries.size()) {
        return;
    }
    const std::string name = entries[row].name;
    const int ret = QMessageBox::warning(
        this, qFromUtf8("删除字段"),
        qFromUtf8("删除「%1」后，该字段将回退源码常量兜底。").arg(qFromUtf8(name.c_str())),
        qFromUtf8("删除"), qFromUtf8("取消"));
    if (ret != 0) {
        return;
    }
    std::string err;
    if (!webCredsRemoveField(name, err)) {
        QMessageBox::warning(this, qFromUtf8("删除失败"), qFromUtf8(err.c_str()));
        return;
    }
    refresh();
}

void WebCredDialog::onToggleReveal() {
    if (revealed) {
        revealed = false;
        refresh();
        return;
    }
    bool needPass = false;
    for (size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].status == kWebCredPassEnc) {
            needPass = true;
            break;
        }
    }
    if (needPass && sessionPass.isEmpty()) {
        bool ok = false;
        const QString pw = qPasswordPrompt(
            this, qFromUtf8("输入口令"), qFromUtf8("口令加密字段需要口令"), &ok);
        if (!ok || pw.isEmpty()) {
            return;
        }
        sessionPass = pw;
    }
    revealed = true;
    refresh();
}

void WebCredDialog::onRefresh() {
    refresh();
}

// ── WebCredEditDialog ──────────────────────────────────────────

WebCredEditDialog::WebCredEditDialog(QWidget* parent, const std::string& field,
                                     WebCredStatus status, const QString& sessionPass)
    : QDialog(parent), nameCombo(nullptr), modeCombo(nullptr), valueEdit(nullptr),
      passLabel(nullptr), passEdit(nullptr), m_sessionPass(sessionPass) {
    qSetWindowTitle(this, qFromUtf8("配置 Web 凭据"));
    resize(400, 180);

    QBoxLayout* mainLayout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 10, 10);

    QBoxLayout* nameRow = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    nameRow->addWidget(new QLabel(qFromUtf8("字段:"), this));
    nameCombo = new QComboBox(this);
    size_t kc = 0;
    const char* const* known = webCredsKnownFields(kc);
    int wantIdx = 0;
    for (size_t i = 0; i < kc; ++i) {
#ifdef QT3_BUILD
        nameCombo->insertItem(qFromUtf8(known[i]));
#else
        nameCombo->addItem(qFromUtf8(known[i]));
#endif
        if (field == known[i]) {
            wantIdx = (int)i;
        }
    }
#ifdef QT3_BUILD
    nameCombo->setCurrentItem(wantIdx);
#else
    nameCombo->setCurrentIndex(wantIdx);
#endif
    nameRow->addWidget(nameCombo, 1);
    mainLayout->addLayout(nameRow);

    QBoxLayout* modeRow = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    modeRow->addWidget(new QLabel(qFromUtf8("模式:"), this));
    modeCombo = new QComboBox(this);
    int modeIdx = 0;
    if (field.empty()) {
        modeIdx = 1;   // 新建默认令牌加密（零交互）
    } else if (status == kWebCredTokenEnc) {
        modeIdx = 1;
    } else if (status == kWebCredPassEnc) {
        modeIdx = 2;
    }
    for (int i = 0; i < 3; ++i) {
#ifdef QT3_BUILD
        modeCombo->insertItem(qFromUtf8(kModeLabels[i]));
#else
        modeCombo->addItem(qFromUtf8(kModeLabels[i]));
#endif
    }
#ifdef QT3_BUILD
    modeCombo->setCurrentItem(modeIdx);
#else
    modeCombo->setCurrentIndex(modeIdx);
#endif
    modeRow->addWidget(modeCombo, 1);
    mainLayout->addLayout(modeRow);

    QBoxLayout* valueRow = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    valueRow->addWidget(new QLabel(qFromUtf8("值:"), this));
    valueEdit = new QLineEdit(this);
    valueEdit->setEchoMode(modeIdx == 0 ? QLineEdit::Normal : QLineEdit::Password);
    valueRow->addWidget(valueEdit, 1);
    mainLayout->addLayout(valueRow);

    QBoxLayout* passRow = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    passLabel = new QLabel(qFromUtf8("口令:"), this);
    passEdit = new QLineEdit(this);
    passEdit->setEchoMode(QLineEdit::Password);
    passRow->addWidget(passLabel);
    passRow->addWidget(passEdit, 1);
    mainLayout->addLayout(passRow);
    if (modeIdx != 2) {
        passLabel->hide();
        passEdit->hide();
    }

    QBoxLayout* btnLayout = qNewBoxLayout(nullptr, QBoxLayout::LeftToRight, 0, 0);
    btnLayout->addItem(new QSpacerItem(1, 1, QSizePolicy::Expanding, QSizePolicy::Minimum));
    QPushButton* okBtn = new QPushButton(qFromUtf8("确定"), this);
    QObject::connect(okBtn, SIGNAL(clicked()), this, SLOT(onOk()));
    btnLayout->addWidget(okBtn);
    QPushButton* cancelBtn = new QPushButton(qFromUtf8("取消"), this);
    QObject::connect(cancelBtn, SIGNAL(clicked()), this, SLOT(reject()));
    btnLayout->addWidget(cancelBtn);
    mainLayout->addLayout(btnLayout);

    QObject::connect(modeCombo, SIGNAL(activated(int)), this, SLOT(onModeChanged()));
}

WebCredMode WebCredEditDialog::getMode() const {
#ifdef QT3_BUILD
    const int idx = modeCombo->currentItem();
#else
    const int idx = modeCombo->currentIndex();
#endif
    if (idx == 2) {
        return kWebCredPassMode;
    }
    if (idx == 1) {
        return kWebCredTokenMode;
    }
    return kWebCredPlainMode;
}

QString WebCredEditDialog::getFieldName() const {
    return nameCombo->currentText();
}

QString WebCredEditDialog::getValue() const {
    return valueEdit->text();
}

QString WebCredEditDialog::getPassphrase() const {
    return passEdit->text();
}

void WebCredEditDialog::onModeChanged() {
    const WebCredMode mode = getMode();
    if (mode == kWebCredPassMode) {
        passLabel->show();
        passEdit->show();
    } else {
        passLabel->hide();
        passEdit->hide();
    }
    valueEdit->setEchoMode(mode == kWebCredPlainMode ? QLineEdit::Normal : QLineEdit::Password);
}

void WebCredEditDialog::onOk() {
    if (qTrim(valueEdit->text()).isEmpty()) {
        QMessageBox::warning(this, qFromUtf8("Web 凭据"), qFromUtf8("值不能为空。"));
        return;
    }
    accept();
}