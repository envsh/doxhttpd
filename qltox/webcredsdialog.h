#ifndef WEBCREDSDIALOG_H
#define WEBCREDSDIALOG_H

#include "compat34.h"
#include "webcreds.h"
#include <qstring.h>
#include <qdialog.h>
#include <vector>

class QComboBox;
class QLabel;
class QLineEdit;

// 凭据配置管理对话框（入口：工具 → Web 凭据）。
// 列表 + 操作按钮，等于 webcreds CLI 的 show/set/rm 图形化版本。
class WebCredDialog : public QDialog {
    Q_OBJECT
public:
    explicit WebCredDialog(QWidget* parent = nullptr);

private slots:
    void onEdit();
    void onRemove();
    void onToggleReveal();
    void onRefresh();

private:
    int currentRow() const;
    void refresh();
    QString rowText(const WebCredListEntry& e) const;

    void* listWidget;   // QListBox* (Qt3) or QListWidget* (Qt4)
    QPushButton* revealBtn;
    bool revealed;
    QString sessionPass;   // 会话内缓存口令（pass 字段解密/保存用），关窗即弃
    std::vector<WebCredListEntry> entries;
};

// 单字段 增/改 编辑弹窗。
class WebCredEditDialog : public QDialog {
    Q_OBJECT
public:
    explicit WebCredEditDialog(QWidget* parent, const std::string& field,
                               WebCredStatus status, const QString& sessionPass);

    WebCredMode getMode() const;
    QString getFieldName() const;
    QString getValue() const;
    QString getPassphrase() const;   // 空 = 复用会话口令

private slots:
    void onModeChanged();
    void onOk();

private:
    QComboBox* nameCombo;   // 仅 6 个已知字段
    QComboBox* modeCombo;   // 明文 / 令牌加密 / 口令加密
    QLineEdit* valueEdit;
    QLabel* passLabel;
    QLineEdit* passEdit;
    QString m_sessionPass;
};

#endif // WEBCREDSDIALOG_H