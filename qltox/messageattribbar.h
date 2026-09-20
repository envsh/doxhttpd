#ifndef MESSAGEATTRIBBAR_H
#define MESSAGEATTRIBBAR_H

#include "compat34.h"
#include <qmap.h>
#ifdef QT3_BUILD
#include <qvaluelist.h>
#include <qstringlist.h>
#include <qspinbox.h>
#else
#include <QList>
#include <QStringList>
#include <QSpinBox>
#endif
#include <qcombobox.h>

// 属性定义：key=ASCII wire 标识；label 展示文本(CJK 用 qFromUtf8 传入)
struct MessageAttrDef {
    enum Kind { kCombo = 0, kCheck, kSpin, kLine, kTags };
    QString key;
    QString label;
    Kind kind;
    QStringList options;      // kCombo
    int spinMin; int spinMax; int spinStep;   // kSpin
    QString defValue;         // 默认值(combo=选项文本 / check="0"/"1" / spin=数字串 / line,tags=文本)
    QString hint;             // tooltip 提示(Qt3 无 placeholder,仅 tooltip)
};

#ifdef QT3_BUILD
typedef QValueList<MessageAttrDef> MessageAttrDefList;
#else
typedef QList<MessageAttrDef> MessageAttrDefList;
#endif

// 属性组件条：输入区下方,随联系人类型动态重建(无 Q_OBJECT,规避 Qt3 moc 约束)
class MessageAttribBar : public QWidget {
public:
    MessageAttribBar(QWidget* parent = 0);
    void setDefs(const MessageAttrDefList& defs);
    void setValues(const QMap<QString,QString>& values);
    QMap<QString,QString> values() const;   // UI 读取接口(占位阶段暂不回填发送)
    void setTypeLabel(const QString& text); // 行首固定类型标签(key 固定为 proto_type)
    void clear();
    void requestRelayout();   // kTags 增删后重排属性条,避免压到下一行
protected:
    void resizeEvent(QResizeEvent* e);
    void showEvent(QShowEvent* e);
private:
    struct Item { QLabel* label; QWidget* control; int kind; };
#ifdef QT3_BUILD
    typedef QValueList<Item> ItemList;
#else
    typedef QList<Item> ItemList;
#endif
    MessageAttrDefList m_defs;
    ItemList m_items;
    QMap<QString,QString> m_values;
    QLabel* m_typeLabel;
    bool m_inLayout;
    void rebuildChildren();
    void layoutItems();
    QString itemValue(int i) const;
    void setItemValue(int i, const QString& v);
};

MessageAttrDefList messageAttribBarDefsForType(const QString& type);

#endif // MESSAGEATTRIBBAR_H