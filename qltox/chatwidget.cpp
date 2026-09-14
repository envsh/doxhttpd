#include "chatwidget.h"
#include "translator.h"
#include "compat34.h"
#include <cassert>
#include "restapi.h"
#include "translate_util.h"
#include "translation_cache.h"
#ifdef QT3_BUILD
#include <qtimer.h>
#else
#include <QTimer>
#endif

#include "config.h"
#include "ThemeManager.h"
#include "stickerpicker.h"
#include "storage.h"
#ifdef QT3_BUILD
#include <qlayout.h>
#include <qhbox.h>
#include <qvbox.h>
#else
#include <QHBoxLayout>
#include <QVBoxLayout>
#endif

static void clearChipRow(QWidget* row, ChipWidgetList& chips);

#ifdef QT3_BUILD
#include <qfiledialog.h>
#else
#include <QFileDialog>
#endif

namespace {

QStringList defaultQuickReplies() {
    QStringList list;
    list << qFromUtf8("你好，在的");
    list << qFromUtf8("收到，谢谢");
    list << qFromUtf8("好的，没问题");
    list << qFromUtf8("请稍等，我看一下");
    list << qFromUtf8("好的，马上就来");
    list << qFromUtf8("稍等，我查一下再回复您");
    list << qFromUtf8("了解，收到");
    list << qFromUtf8("不好意思，刚才有点忙");
    list << qFromUtf8("你方便的时候再回复我");
    return list;
}

QStringList quickReplies() {
    QString raw = Config::value("quick_replies");
    if (!raw.isEmpty()) {
#ifdef QT3_BUILD
        QStringList split = QStringList::split("\n", raw);
#else
        QStringList split = raw.split("\n", QString::SkipEmptyParts);
#endif
        if (!split.isEmpty()) { return split; }
    }
    return defaultQuickReplies();
}

}  // namespace

ChatWidget::ChatWidget(QWidget* parent) : QWidget(parent) {
    if (s_autoTranslateArg) { m_autoTranslateEnabled = true; }
    QBoxLayout* mainLayout = qNewBoxLayout(this, QBoxLayout::TopToBottom, 0, 0);
    mainLayout->setSpacing(0);
    qSetMargins(mainLayout, 0, 0, 0, 0);
    
    // 聊天头部
    QBoxLayout* headerLayout = new QBoxLayout(QBoxLayout::LeftToRight);
    headerText = new QLabel(_("select_chat_object"), this);
    {
        QFont hf = headerText->font();
        hf.setPointSize(hf.pointSize() + 2);
        hf.setBold(true);
        headerText->setFont(hf);
    }
    headerLayout->addWidget(headerText, 1);

    // 加载状态条（语言选择器左侧）
    m_loadingBar = new LoadingBar(this);
    headerLayout->addWidget(m_loadingBar, 0);

    // 语言选择器
    langSelector = new QComboBox(this);
#ifdef QT3_BUILD
    langSelector->insertItem(qFromUtf8("简体中文"), 0);
    langSelector->insertItem(qFromUtf8("繁體中文"), 1);
    langSelector->insertItem(qFromUtf8("English"), 2);
    langSelector->setCurrentItem(0);
#else
    langSelector->insertItem(0, qFromUtf8("简体中文"));
    langSelector->insertItem(1, qFromUtf8("繁體中文"));
    langSelector->insertItem(2, qFromUtf8("English"));
    langSelector->setCurrentIndex(0);
#endif
    connect(langSelector, SIGNAL(activated(int)), this, SLOT(onUilangChanged(int)));
    connect(langSelector, SIGNAL(activated(int)), this, SLOT(onTranslateTolangChanged(int)));
    headerLayout->addWidget(langSelector);

    // 风格选择器
    m_styleSelector = new QComboBox(this);
    {
        const auto& styles = StyleParams::registeredStyles();
#ifdef QT3_BUILD
        for (int i = 0; i < (int)styles.size(); ++i)
            m_styleSelector->insertItem(_(styles[i].displayKey), i);
        int cur = 0;
        for (int i = 0; i < (int)styles.size(); ++i)
            if (QString(styles[i].id) == QString(ThemeManager::styleId())) { cur = i; break; }
        m_styleSelector->setCurrentItem(cur);
#else
        for (int i = 0; i < (int)styles.size(); ++i)
            m_styleSelector->insertItem(i, _(styles[i].displayKey));
        int cur = 0;
        for (int i = 0; i < (int)styles.size(); ++i)
            if (QString(styles[i].id) == QString(ThemeManager::styleId())) { cur = i; break; }
        m_styleSelector->setCurrentIndex(cur);
#endif
    }
    connect(m_styleSelector, SIGNAL(activated(int)), this, SLOT(onStyleChanged(int)));
    headerLayout->addWidget(m_styleSelector);

    // 主题切换复选框
    themeCheckBox = new QCheckBox(_("theme_dark"), this);
    qSetChecked(themeCheckBox, true);
    connect(themeCheckBox, SIGNAL(toggled(bool)), this, SLOT(onThemeToggled(bool)));
    headerLayout->addWidget(themeCheckBox);
    
    mainLayout->addLayout(headerLayout);
    
    // ── 未读消息横幅 ──
    m_unreadBanner = new QLabel(this);
    m_unreadBanner->setAlignment(Qt::AlignCenter);
    m_unreadBanner->setFixedHeight(26);
    m_unreadBanner->hide();
    mainLayout->addWidget(m_unreadBanner);
    
    // 消息区域（虚拟化列表）
    messageArea = new ChatView(this);
    mainLayout->addWidget(messageArea, 1);

    // ── 待发扩展上下文指示区（引用条 + 提及 chip 行，初始隐藏）──
    m_ctxBar = new QWidget(this);
    {
        QVBoxLayout* barLay = new QVBoxLayout(m_ctxBar);
        barLay->setSpacing(2);

        m_replyRow = new QWidget(m_ctxBar);
        {
            QHBoxLayout* rl = new QHBoxLayout(m_replyRow);
            rl->setSpacing(4);
            qSetMargins(rl, 0, 0, 16, 0);      // 把 × 移离最右缘 ~16px，避开右缘覆盖条带
            m_replyStrip = new QLabel(m_replyRow);
            m_replySnippet = new QLabel(m_replyRow);
            m_replyCloseBtn = new QPushButton(qFromUtf8("×"), m_replyRow);
            m_replyCloseBtn->setFixedSize(18, 18);
            rl->addWidget(m_replyStrip);
            rl->addWidget(m_replySnippet, 1);
            rl->addWidget(m_replyCloseBtn);
            {
                bool ok = connect(m_replyCloseBtn, SIGNAL(clicked()), this, SLOT(onReplyStripClose()));
                qWarning("DIA replyClose connect ok=%d", (int)ok);
            }
        }
        m_replyRow->hide();

        m_chipRow = new QWidget(m_ctxBar);
        new QHBoxLayout(m_chipRow);
        m_chipRow->hide();

        barLay->addWidget(m_replyRow);
        barLay->addWidget(m_chipRow);
    }
    m_ctxBar->hide();
    mainLayout->addWidget(m_ctxBar);
    /*
     * Qt3 SIGNAL/SLOT 空格问题：
     *    Qt3 的 connect() 不自动归一化空格 (normalizeSignalSlot 是 protected，
     *    connect() 不会调用它)。多参数 SIGNAL/SLOT 中逗号后的空格会导致
     *    moc 字符串与运行时字符串不匹配，连接静默失败。
     *    Qt4+ 才开始在 connect() 内部 fallback 调用 normalizedSignature()。
     *
     * 出处:
     *   - https://doc.qt.io/archives/3.3/qobject-h.html
     *     connect() 是 public static，normalizeSignalSlot 是 static protected
     *   - https://www.trinitydesktop.org/docs/qt3/tqobject.html
     *     normalizeSignalSlot: "removes unnecessary whitespace" [static protected]
     *   - https://marcmutz.wordpress.com/effective-qt/prefer-to-use-normalised-signalslot-signatures/
     *     Qt4 connect() 源码: 首次查找失败后才调用 normalizedSignature()
     *   - https://github.com/lxqt/libqtxdg/commit/39e75f0
     *     lxqt 2018 年 normalize 提交：移除 SIGNAL 中空格
     */
    connect(messageArea, SIGNAL(translateClicked(int)), this, SLOT(onTranslateClicked(int)));
    connect(messageArea, SIGNAL(sourceClicked(int)), this, SIGNAL(sourceClicked(int)));
    connect(messageArea, SIGNAL(retryClicked(int, const QString&, const QString&)), this, SIGNAL(retryClicked(int, const QString&, const QString&)));
    connect(messageArea, SIGNAL(resendMessage(int)), this, SIGNAL(resendMessage(int)));
    connect(messageArea, SIGNAL(openFullSizeImage(int, const QString&)),
            this, SIGNAL(openFullSizeImage(int, const QString&)));
    connect(messageArea, SIGNAL(openMediaPlayer(int)),
            this, SIGNAL(openMediaPlayer(int)));
    connect(messageArea, SIGNAL(mentionClicked(const QString&,const QString&)),
            this, SLOT(onMentionClicked(const QString&,const QString&)));
    connect(messageArea, SIGNAL(autoTranslateRequested(int, const QString&, const QString&)),
            this, SLOT(onAutoTranslateRequested(int, const QString&, const QString&)));
    connect(messageArea, SIGNAL(replyRequested(int)), this, SLOT(onReplyRequested(int)));
    connect(messageArea, SIGNAL(editRequested(int)), this, SLOT(onEditRequested(int)));
    connect(messageArea, SIGNAL(deleteRequested(int)), this, SLOT(onDeleteRequested(int)));
    connect(messageArea, SIGNAL(redactRequested(int)), this, SLOT(onRedactRequested(int)));
    connect(messageArea, SIGNAL(favoriteClicked(int)), this, SIGNAL(favoriteClicked(int)));
    connect(messageArea, SIGNAL(forwardClicked(int)), this, SLOT(onForwardRequested(int)));
    
    // 输入区域 (2行 x 3列)
#ifdef QT3_BUILD
    QGridLayout* inputGrid = new QGridLayout(2, 6, 2);
    inputEdit = new MessageInput(this);
    inputGrid->addMultiCellWidget(inputEdit, 0, 1, 0, 0);
    emojiBtn = new EmojiPushButton(qFromUtf8("😊"), this);
    emojiBtn->setFixedSize(24, 24);
    qSetToolTip(emojiBtn, _("tooltips.emoji"));
    inputGrid->addWidget(emojiBtn, 0, 1);
    fileBtn = new EmojiPushButton(qFromUtf8("📎"), this);
    fileBtn->setFixedSize(24, 24);
    qSetToolTip(fileBtn, _("tooltips.file"));
    inputGrid->addWidget(fileBtn, 1, 1);
    stickerBtn = new EmojiPushButton(qFromUtf8("🧸"), this);
    stickerBtn->setFixedSize(24, 24);
    qSetToolTip(stickerBtn, _("tooltips.sticker"));
    inputGrid->addWidget(stickerBtn, 0, 2);
    quickReplyBtn = new EmojiPushButton(qFromUtf8("⚡"), this);
    quickReplyBtn->setFixedSize(24, 24);
    qSetToolTip(quickReplyBtn, _("tooltips.quickreply"));
    inputGrid->addWidget(quickReplyBtn, 1, 2);
    historyBtn = new EmojiPushButton(qFromUtf8("🕘"), this);
    historyBtn->setFixedSize(24, 24);
    qSetToolTip(historyBtn, qFromUtf8("显示历史消息"));
    inputGrid->addWidget(historyBtn, 0, 3);
    screenshotBtn = new EmojiPushButton(qFromUtf8("📷"), this);
    screenshotBtn->setFixedSize(24, 24);
    qSetToolTip(screenshotBtn, qFromUtf8("屏幕抓图"));
    inputGrid->addWidget(screenshotBtn, 1, 3);
    sendBtn = new QPushButton(_("buttons.send"), this);
    QFontMetrics fm = inputEdit->fontMetrics();
    int twoLineH = fm.lineSpacing() * 2 + fm.lineSpacing() / 2 + 6;
    inputEdit->setMaximumHeight(twoLineH);
    sendBtn->setFixedSize(twoLineH, twoLineH);
    inputGrid->addMultiCellWidget(sendBtn, 0, 1, 4, 4);
    inputGrid->setColStretch(0, 1);

    m_sendEnBtn = new QPushButton("Send EN", this);
    m_sendEnBtn->setFixedWidth(60);
    m_sendEnBtn->setFixedHeight(twoLineH);
    inputGrid->addMultiCellWidget(m_sendEnBtn, 0, 1, 5, 5);
#else
    QGridLayout* inputGrid = new QGridLayout();
    inputGrid->setSpacing(2);
    inputEdit = new MessageInput(this);
    inputGrid->addWidget(inputEdit, 0, 0, 2, 1);
    emojiBtn = new EmojiPushButton(qFromUtf8("😊"), this);
    emojiBtn->setFixedSize(24, 24);
    qSetToolTip(emojiBtn, _("tooltips.emoji"));
    inputGrid->addWidget(emojiBtn, 0, 1);
    fileBtn = new EmojiPushButton(qFromUtf8("📎"), this);
    fileBtn->setFixedSize(24, 24);
    qSetToolTip(fileBtn, _("tooltips.file"));
    inputGrid->addWidget(fileBtn, 1, 1);
    stickerBtn = new EmojiPushButton(qFromUtf8("🧸"), this);
    stickerBtn->setFixedSize(24, 24);
    qSetToolTip(stickerBtn, _("tooltips.sticker"));
    inputGrid->addWidget(stickerBtn, 0, 2);
    quickReplyBtn = new EmojiPushButton(qFromUtf8("⚡"), this);
    quickReplyBtn->setFixedSize(24, 24);
    qSetToolTip(quickReplyBtn, _("tooltips.quickreply"));
    inputGrid->addWidget(quickReplyBtn, 1, 2);
    historyBtn = new EmojiPushButton(qFromUtf8("🕘"), this);
    historyBtn->setFixedSize(24, 24);
    qSetToolTip(historyBtn, qFromUtf8("显示历史消息"));
    inputGrid->addWidget(historyBtn, 0, 3);
    screenshotBtn = new EmojiPushButton(qFromUtf8("📷"), this);
    screenshotBtn->setFixedSize(24, 24);
    qSetToolTip(screenshotBtn, qFromUtf8("屏幕抓图"));
    inputGrid->addWidget(screenshotBtn, 1, 3);
    sendBtn = new QPushButton(_("buttons.send"), this);
    QFontMetrics fm = inputEdit->fontMetrics();
    int twoLineH = fm.lineSpacing() * 2 + fm.lineSpacing() / 2 + 6;
    inputEdit->setMaximumHeight(twoLineH);
    sendBtn->setFixedSize(twoLineH, twoLineH);
    inputGrid->addWidget(sendBtn, 0, 4, 2, 1);
    inputGrid->setColumnStretch(0, 1);

    m_sendEnBtn = new QPushButton("Send EN", this);
    m_sendEnBtn->setFixedWidth(60);
    m_sendEnBtn->setFixedHeight(twoLineH);
    inputGrid->addWidget(m_sendEnBtn, 0, 5, 2, 1);
#endif

        inputEdit->setPlaceholderText(_("placeholders.type_message"));

    // Emoji picker
    emojiPicker = new EmojiPicker(this);
    connect(emojiPicker, SIGNAL(emojiSelected(const QString&)), this, SLOT(onEmojiInsert(const QString&)));
    
    connect(sendBtn, SIGNAL(clicked()), this, SLOT(onSendClicked()));
    connect(m_sendEnBtn, SIGNAL(clicked()), this, SLOT(onSendEnClicked()));
    connect(inputEdit, SIGNAL(sendRequested()), this, SLOT(onSendClicked()));
    connect(emojiBtn, SIGNAL(clicked()), this, SLOT(onEmojiClicked()));
    connect(fileBtn, SIGNAL(clicked()), this, SLOT(onFileClicked()));
    connect(stickerBtn, SIGNAL(clicked()), this, SLOT(onStickerClicked()));
    connect(quickReplyBtn, SIGNAL(clicked()), this, SLOT(onQuickReplyClicked()));
    connect(screenshotBtn, SIGNAL(clicked()), this, SLOT(onScreenshotClicked()));
    connect(inputEdit, SIGNAL(filePasteRequested(const QString&, const QString&)), this, SLOT(onFilePaste(const QString&, const QString&)));
    
    mainLayout->addLayout(inputGrid);
}

void ChatWidget::setHeaderText(const QString& text) {
    m_baseHeader = text;
    int cnt = messageArea->messageCount();
    if (cnt > 0)
        headerText->setText(text + QString(" (%1)").arg(cnt));
    else
        headerText->setText(text);
}

void ChatWidget::showUnreadBanner(int count) {
    if (count <= 0) { hideUnreadBanner(); return; }
    QColor bg(200, 220, 255);
    QColor fg(30, 30, 30);
#ifdef QT3_BUILD
    m_unreadBanner->setPaletteBackgroundColor(bg);
    m_unreadBanner->setPaletteForegroundColor(fg);
#else
    QPalette p;
    p.setColor(QPalette::Window, bg);
    p.setColor(QPalette::WindowText, fg);
    m_unreadBanner->setPalette(p);
    m_unreadBanner->setAutoFillBackground(true);
#endif
    m_unreadBanner->setText(QString("  %1  ").arg(_("n_new_messages").arg(count)));
    m_unreadBanner->show();
    QTimer::singleShot(5000, this, SLOT(hideUnreadBanner()));
}

void ChatWidget::hideUnreadBanner() {
    m_unreadBanner->hide();
}

void ChatWidget::setBuffer(ChatHistory* hist) {
    messageArea->setBuffer(hist);
    updateHeaderCount();
}

void ChatWidget::updateHeaderCount() {
    headerText->setText(m_baseHeader + " (" + QString::number(messageCount()) + ")");
}

void ChatWidget::scrollBottomIfNeeded() {
    messageArea->scrollBottomIfNeeded();
    updateHeaderCount();
}

int ChatWidget::messageCount() const {
    return messageArea->messageCount();
}

ChatElement ChatWidget::messageAt(int index) const {
    return messageArea->messageAt(index);
}

ChatElement& ChatWidget::mutableMessageAt(int index) {
    return messageArea->messageAt(index);
}



void ChatWidget::onSendClicked() {
    if (inputEdit->placeholderText().length() > 0) {
#ifdef QT3_BUILD
        if (inputEdit->text() == inputEdit->placeholderText()) { return; }
#else
        if (inputEdit->toPlainText() == inputEdit->placeholderText()) { return; }
#endif
    }
#ifdef QT3_BUILD
    QString msg = qTrim(inputEdit->text());
#else
    QString msg = qTrim(inputEdit->toPlainText());
#endif
    if (msg.isEmpty()) return;
    
    inputEdit->saveToHistory(msg);
    emit messageSent(msg, m_pendingCtx);
    m_pendingCtx.clear();
    m_pendingMentionDisplay.clear();
    m_replyMentionedName.truncate(0);
    m_replyDisplayName.truncate(0);
    m_replySnippetText.truncate(0);
    m_replyRow->hide();
    clearChipRow(m_chipRow, m_chipWidgets);
    m_ctxBar->hide();
#ifdef QT3_BUILD
    inputEdit->selectAll();
    inputEdit->removeSelectedText();
#else
    QTextCursor cursor = inputEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
#endif
}

void ChatWidget::onSendEnClicked() {
#ifdef QT3_BUILD
    QString msg = qTrim(inputEdit->text());
#else
    QString msg = qTrim(inputEdit->toPlainText());
#endif
    if (msg.isEmpty()) return;

    inputEdit->saveToHistory(msg);
#ifdef QT3_BUILD
    inputEdit->selectAll();
    inputEdit->removeSelectedText();
#else
    QTextCursor cursor = inputEdit->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::End, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();
#endif
    emit translateForSendRequested(msg, "en");
}

void ChatWidget::retranslateUi() {
    // 更新聊天头
    // 注意：headerText 的更新由 MainWindow::retranslateUi() 处理
    
    // 更新按钮文字
    if (sendBtn) { sendBtn->setText(_("buttons.send")); }
    
    // 更新主题复选框文本
    if (themeCheckBox) { themeCheckBox->setText(_("theme_dark")); }
    
    // 更新风格选择器文字
    if (m_styleSelector) {
        const auto& styles = StyleParams::registeredStyles();
        for (int i = 0; i < (int)styles.size(); ++i) {
#ifdef QT3_BUILD
            m_styleSelector->changeItem(_(styles[i].displayKey), i);
#else
            m_styleSelector->setItemText(i, _(styles[i].displayKey));
#endif
        }
    }
    
    // 更新输入框 placeholder
    if (inputEdit) {
    inputEdit->setPlaceholderText(_("placeholders.type_message"));
    }
}

static QString langCodeFromIndex(int index) {
    if (index == 0) return "zh-CN";
    if (index == 1) return "zh-TW";
    if (index == 2) return "en-US";
    return "zh-CN";
}

void ChatWidget::onUilangChanged(int index) {
    QString langCode = langCodeFromIndex(index);
    Config::setValue("uilang", langCode);
    qWarning("ChatWidget: uilang changed to %s", qToUtf8(langCode).data());
    emit languageChanged(langCode);
}

void ChatWidget::onTranslateTolangChanged(int index) {
    Config::setValue("translate_tolang", langCodeFromIndex(index));
}

bool ChatWidget::applyCachedTranslation(int msgIndex, const QString& toLang) {
    if (toLang.isEmpty() || msgIndex < 0 || msgIndex >= (int)messageArea->messageCount()) {
        return false;
    }
    ChatElement& msg = messageArea->messageAt(msgIndex);
    if (msg.dbRowid <= 0) { return false; }
    std::string cached;
    if (!translationCache()->lookup(msg.dbRowid,
            std::string(qToUtf8(toLang).data()), cached)
        || cached.empty()) {
        return false;
    }
    qWarning("ChatWidget: translation cache hit msgIndex=%d rowid=%lld lang=%s",
             msgIndex, (long long)msg.dbRowid, qToUtf8(toLang).data());
    msg.transState = TransState::Done;
    msg.translatedText = qFromUtf8(cached);
    msg.showTranslation = true;
    msg.translateError = QString();
    msg.cachedWidth = -1;  // 强制 updateElement 重算高度
    messageArea->updateElement(msgIndex);
    return true;
}

void ChatWidget::onTranslateClicked(int msgIndex) {
    ChatElement& msg = messageArea->messageAt(msgIndex);
    if (msg.transState == TransState::InFlight) { return; }

    // Toggle: if already translated, just toggle display
    if (msg.transState == TransState::Done && !msg.translatedText.isEmpty()) {
        msg.showTranslation = !msg.showTranslation;
        messageArea->updateElement(msgIndex);
        return;
    }

    // 翻译缓存命中：直接显示，不发网络请求
    if (applyCachedTranslation(msgIndex, Config::value("translate_tolang"))) { return; }

    msg.translateError = QString();
    msg.transState = TransState::InFlight;
    messageArea->updateElement(msgIndex);
    emit translateRequested(msgIndex, msg.messageText,
                            Config::value("translate_tolang"));
}

void ChatWidget::onAutoTranslateRequested(int msgIndex, const QString& text, const QString& toLang) {
    if (msgIndex < 0 || msgIndex >= (int)messageArea->messageCount()) { return; }
    ChatElement& msg = messageArea->messageAt(msgIndex);
    assert(msg.transState == TransState::Scheduled);

    if (!m_autoTranslateEnabled) {
        msg.transState = TransState::None;
        return;
    }

    // 翻译缓存命中：直接显示，不发网络请求
    if (applyCachedTranslation(msgIndex, toLang)) { return; }

    qWarning("ChatWidget: auto-translate request msgIndex=%d toLang=%s text=[%.80s]",
             msgIndex, qToUtf8(toLang).data(), qToUtf8(text).data());
    msg.translateError = QString();
    msg.transState = TransState::InFlight;
    messageArea->updateElement(msgIndex);
    emit translateRequested(msgIndex, text, toLang);
}

void ChatWidget::onTranslateResult(int msgIndex, bool success, const QString& translatedText, const QString& errorMessage) {
    if (msgIndex < 0 || msgIndex >= (int)messageArea->messageCount()) return;
    ChatElement& msg = messageArea->messageAt(msgIndex);
    if (success) {
        msg.transState = TransState::Done;
        msg.translatedText = translatedText;
        msg.showTranslation = true;
        msg.translateError = QString();
        msg.cachedWidth = -1;  // 强制 updateElement 重算高度，否则 cachedWidth==w 时不会展开气泡
        // 写入翻译缓存（异步、幂等），供下次加载同消息时直接使用
        if (msg.dbRowid > 0) {
            translationCache()->save(msg.dbRowid,
                std::string(qToUtf8(Config::value("translate_tolang")).data()),
                std::string(qToUtf8(translatedText).data()));
        }
    } else {
        msg.transState = TransState::Done;
        msg.translateError = errorMessage;
    }
    messageArea->updateElement(msgIndex);
}

void ChatWidget::onThemeToggled(bool checked) {
    // TODO: 实现主题切换逻辑
    qWarning("Theme toggled: %d", checked);
    ThemeManager::applyTheme(checked);
}

void ChatWidget::onStyleChanged(int index) {
    const auto& styles = StyleParams::registeredStyles();
    if (index >= 0 && index < (int)styles.size())
        ThemeManager::setStyle(styles[index].id, ThemeManager::isDarkMode());
}

void ChatWidget::onEmojiClicked() {
    QPoint btnPos = emojiBtn->mapToGlobal(QPoint(0, 0));
    int pickerW = emojiPicker->width();
    int pickerH = emojiPicker->height();
    int x = btnPos.x() + emojiBtn->width() - pickerW;
    int y = btnPos.y() - pickerH;
    if (y < 0) { y = btnPos.y() + emojiBtn->height(); }
    emojiPicker->showAt(QPoint(x, y));
}

void ChatWidget::onEmojiInsert(const QString& emoji) {
    qWarning("ChatWidget::onEmojiInsert: received emoji string of length %d: '%s'", emoji.length(), qToUtf8(emoji).data());
    inputEdit->clearPlaceholder();
#ifdef QT3_BUILD
    inputEdit->insert(emoji);
#else
    inputEdit->insertPlainText(emoji);
#endif
    inputEdit->setFocus();
}

void ChatWidget::onFileClicked() {
    QString path;
#ifdef QT3_BUILD
    path = QFileDialog::getOpenFileName(QString::null, QString::null, this);
#else
    path = QFileDialog::getOpenFileName(this, QString(), QString());
#endif
    if (path.isEmpty()) { return; }
    emit fileSendRequested(path);
}

void ChatWidget::onFilePaste(const QString& filePath, const QString& caption) {
    if (filePath.isEmpty()) { return; }
    emit fileSendRequested(filePath, caption);
}

void ChatWidget::onStickerClicked() {
    if (!m_stickerPicker) {
        m_stickerPicker = new StickerPicker(this);
        m_stickerPicker->setStickerDb(Storage::instance().stickerDb());
    }
    m_stickerPicker->loadPacks();
    QPoint btnPos = stickerBtn->mapToGlobal(QPoint(0, 0));
    int w = m_stickerPicker->width();
    int h = m_stickerPicker->height();
    int x = btnPos.x() + stickerBtn->width() - w;
    int y = btnPos.y() - h;
    if (y < 0) { y = btnPos.y() + stickerBtn->height(); }
    m_stickerPicker->showAt(QPoint(x, y));
}

void ChatWidget::onQuickReplyClicked() {
    QStringList list = quickReplies();
    if (list.isEmpty()) { return; }
#ifdef QT3_BUILD
    QPopupMenu menu(this);
    for (int i = 0; i < (int)list.size(); ++i) {
        menu.insertItem(list[i], i);
    }
    int choice = menu.exec(quickReplyBtn->mapToGlobal(QPoint(0, quickReplyBtn->height())));
    if (choice < 0 || choice >= (int)list.size()) { return; }
    QString text = list[choice];
#else
    QMenu menu(this);
    for (int i = 0; i < list.size(); ++i) {
        QAction* act = menu.addAction(list[i]);
        act->setData(i);
    }
    QAction* sel = menu.exec(quickReplyBtn->mapToGlobal(QPoint(0, quickReplyBtn->height())));
    if (!sel) { return; }
    QString text = list[sel->data().toInt()];
#endif
    inputEdit->clearPlaceholder();
#ifdef QT3_BUILD
    inputEdit->insert(text);
#else
    inputEdit->insertPlainText(text);
#endif
    inputEdit->setFocus();
}

bool ChatWidget::s_autoTranslateArg = false;

// 往 ctx 的 key 追加一个 CSV 值（去重）
static void appendCsvField(QMap<QString,QString>& ctx, const QString& key, const QString& value) {
    QString cur;
    QMap<QString,QString>::iterator it = ctx.find(key);
    if (it != ctx.end()) {
#ifdef QT3_BUILD
        cur = it.data();
#else
        cur = it.value();
#endif
    }
    QStringList parts = cur.isEmpty() ? QStringList() : qSplit(cur, ",");
    if (!parts.contains(value)) {
        parts.append(value);
        ctx.insert(key, parts.join(","));
    }
}

// 从 ctx 的 key 的 CSV 中移除一个值；空则删除该 key
static void removeCsvField(QMap<QString,QString>& ctx, const QString& key, const QString& value) {
    QString cur;
    QMap<QString,QString>::iterator it = ctx.find(key);
    if (it != ctx.end()) {
#ifdef QT3_BUILD
        cur = it.data();
#else
        cur = it.value();
#endif
    }
    QStringList keep;
    QStringList parts = cur.isEmpty() ? QStringList() : qSplit(cur, ",");
    for (QStringList::const_iterator p = parts.begin(); p != parts.end(); ++p) {
        if (*p != value) keep.append(*p);
    }
    if (keep.isEmpty()) ctx.remove(key);
    else ctx.insert(key, keep.join(","));
}

// 被引用摘要截断（Qt3 QLabel 无 elide，手动截断到 ~60 字符）
static QString snippetOneLine(const QString& text) {
    QString one = text;
    one.replace('\n', ' ');
    one.replace('\r', ' ');
    if (one.length() > 60) one = one.left(59) + qFromUtf8("…");
    return one;
}

// 清空 chip 行：Qt3 用 remove+delete，Qt4 用 takeAt 循环（两 API 不一致）
static void clearChipRow(QWidget* row, ChipWidgetList& chips) {
    for (int i = 0; i < chips.count(); ++i) {
#ifdef QT3_BUILD
        row->layout()->remove(chips.at(i));
        delete chips.at(i);
#endif
    }
#ifndef QT3_BUILD
    while (row->layout()->count() > 0) {
        QLayoutItem* li = row->layout()->takeAt(0);
        if (li->widget()) delete li->widget();
        delete li;
    }
#endif
    chips.clear();
}

void ChatWidget::onMentionClicked(const QString& senderName, const QString& nickName) {
    // QString mention = "@" + senderName + " ";
    QString nickfix = nickName.isEmpty() ? senderName : nickName;
    QString mention = qFromUtf8("[%1](%2) ").arg(nickfix).arg(senderName);
    inputEdit->clearPlaceholder();
#ifdef QT3_BUILD
    inputEdit->insert(mention);
#else
    inputEdit->insertPlainText(mention);
#endif
    inputEdit->setFocus();
    appendCsvField(m_pendingCtx, "mentions", senderName);
    if (!m_pendingMentionDisplay.contains(senderName))
        m_pendingMentionDisplay.append(senderName);
    updateMentionChips();
}

void ChatWidget::onReplyRequested(int msgIndex) {
    QString text = messageArea->messageAt(msgIndex).messageText;
    inputEdit->clearPlaceholder();
#ifdef QT3_BUILD
    inputEdit->insert(">> " + text + "\n\n");
#else
    inputEdit->insertPlainText(">> " + text + "\n\n");
#endif
    inputEdit->setFocus();
    ChatElement& target = messageArea->messageAt(msgIndex);   // 仅追加副作用，不改变 >> 行为
    if (!target.messageId.isEmpty()) {
        // 引用条为单值：新回复覆盖旧引用，并把旧引用捆绑的提到先撤掉
        if (!m_replyMentionedName.isEmpty())
            removeCsvField(m_pendingCtx, "mentions", m_replyMentionedName);
        appendCsvField(m_pendingCtx, "reply_to", target.messageId);
        m_replyDisplayName = target.senderName;
        m_replySnippetText = snippetOneLine(target.messageText);
    } else {
        m_replyDisplayName.truncate(0);
        m_replySnippetText.truncate(0);
    }
    if (!target.senderName.isEmpty())
        appendCsvField(m_pendingCtx, "mentions", target.senderName);
    m_replyMentionedName = target.senderName;
    updateReplyStrip();
    qWarning("DIA btn geo=%d,%d %dx%d rowVis=%d ctxVis=%d",
             m_replyCloseBtn->x(), m_replyCloseBtn->y(),
             m_replyCloseBtn->width(), m_replyCloseBtn->height(),
             (int)m_replyRow->isVisible(), (int)m_ctxBar->isVisible());
}

void ChatWidget::onReplyStripClose() {
    qWarning("DIA onReplyStripClose called");
    m_pendingCtx.remove("reply_to");
    if (!m_replyMentionedName.isEmpty())
        removeCsvField(m_pendingCtx, "mentions", m_replyMentionedName);
    m_replyMentionedName.truncate(0);
    m_replyDisplayName.truncate(0);
    m_replySnippetText.truncate(0);
    updateReplyStrip();
}

void ChatWidget::updateReplyStrip() {
    if (m_replySnippetText.isEmpty()) {
        m_replyRow->hide();
    } else {
        m_replyStrip->setText(qFromUtf8("» ") + m_replyDisplayName);
        m_replySnippet->setText(m_replySnippetText);
        m_replyRow->show();
    }
    updateCtxBarVisibility();
}

void ChatWidget::updateMentionChips() {
    clearChipRow(m_chipRow, m_chipWidgets);
    for (QStringList::const_iterator it = m_pendingMentionDisplay.begin();
         it != m_pendingMentionDisplay.end(); ++it) {
        const QString name = *it;
        QWidget* chip = new QWidget(m_chipRow);
        QHBoxLayout* hl = new QHBoxLayout(chip);
        hl->setSpacing(2);
        QLabel* nameLbl = new QLabel(qFromUtf8("@") + name, chip);
        QPushButton* xBtn = new QPushButton(qFromUtf8("×"), chip);
        xBtn->setMinimumSize(18, 18);
        hl->addWidget(nameLbl);
        hl->addWidget(xBtn);
        auto* slot = new LambdaSlot(xBtn, [this, name]() { onChipClose(name); });
        connect(xBtn, SIGNAL(clicked()), slot, SLOT(call()));
        static_cast<QHBoxLayout*>(m_chipRow->layout())->addWidget(chip);
        m_chipWidgets.append(chip);
    }
    if (m_chipWidgets.count() > 0) m_chipRow->show(); else m_chipRow->hide();
    updateCtxBarVisibility();
}

void ChatWidget::onChipClose(const QString& senderName) {
    removeCsvField(m_pendingCtx, "mentions", senderName);
#ifdef QT3_BUILD
    m_pendingMentionDisplay.remove(senderName);
#else
    m_pendingMentionDisplay.removeAll(senderName);
#endif
    updateMentionChips();
}

void ChatWidget::updateCtxBarVisibility() {
    bool visible = !m_replySnippetText.isEmpty() || m_chipWidgets.count() > 0;
    if (visible) m_ctxBar->show(); else m_ctxBar->hide();
}

void ChatWidget::resetPendingContext() {
    m_pendingCtx.clear();
    m_pendingMentionDisplay.clear();
    m_replyMentionedName.truncate(0);
    m_replyDisplayName.truncate(0);
    m_replySnippetText.truncate(0);
    m_replyRow->hide();
    clearChipRow(m_chipRow, m_chipWidgets);
    m_ctxBar->hide();
}

void ChatWidget::onEditRequested(int msgIndex) {
    // TODO: 编辑消息（暂未实现）
}

void ChatWidget::onDeleteRequested(int msgIndex) {
    // TODO: 删除消息（暂未实现）
}

void ChatWidget::onRedactRequested(int msgIndex) {
    if (msgIndex < 0 || msgIndex >= messageCount()) { return; }
    // TEMP(临时): 全放开点击，回滚时取消注释。
    // const ChatElement& el = messageArea->messageAt(msgIndex);
    // if (el.category != "self" || el.messageId.isEmpty()) { return; }
    emit requestRedactMessage(msgIndex);
}

void ChatWidget::onForwardRequested(int msgIndex) {
    if (msgIndex < 0 || msgIndex >= messageCount()) { return; }
    QString text = messageArea->messageAt(msgIndex).messageText;
    if (text.isEmpty()) { return; }
    resetPendingContext();
    inputEdit->clearPlaceholder();
#ifdef QT3_BUILD
    inputEdit->setText(text);
#else
    inputEdit->setPlainText(text);
#endif
    inputEdit->setFocus();
}

void ChatWidget::onScreenshotClicked() {
    emit screenshotRequested();
}
