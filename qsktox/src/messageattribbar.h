#ifndef MESSAGEATTRIBBAR_H
#define MESSAGEATTRIBBAR_H

#include <QskControl.h>
#include <QskComboBox.h>
#include <QskCheckBox.h>
#include <QskSpinBox.h>
#include <QskTextField.h>
#include <QskTextLabel.h>
#include <QStringList>
#include <QVector>
#include <QMap>
#include "messageattribdefs.h"

class TagEditor;

// 属性组件条(Qsk 版，无 QtWidgets)：输入区下方,随联系人类型动态重建。
// 手排 flow 布局(仿 qltox layoutItems)，只做 UI 不回填发送。
class MessageAttribBar : public QskControl
{
public:
    MessageAttribBar(QQuickItem* parent = nullptr);

    void setDefs(const MessageAttrDefList& defs);
    void setTypeLabel(const QString& text);
    void setValues(const QMap<QString,QString>& values);
    QMap<QString,QString> values() const;   // UI 读取接口(占位阶段暂不回填发送)
    void clear();
    void requestRelayout();   // kTags 增删后重排属性条,避免压到下一行

protected:
    void updateLayout() override;
    void geometryChangeEvent(QskGeometryChangeEvent* e) override;

private:
    struct Item {
        QskTextLabel* label = nullptr;
        QskControl* control = nullptr;
        QStringList wireOptions;   // kCombo: 与显示列同长,index->wire 值
    };

    MessageAttrDefList m_defs;
    QVector<Item> m_items;
    QMap<QString,QString> m_values;
    QskTextLabel* m_typeLabel = nullptr;
    bool m_inLayout = false;       // 递归保护(对齐 qltox layoutItems)

    void rebuildChildren();
    void layoutItems();
    QString itemValue(int i) const;
    void setItemValue(int i, const QString& v);
};

#endif // MESSAGEATTRIBBAR_H
