#ifndef TRANSLATE_HOST_H
#define TRANSLATE_HOST_H

#include <QObject>
#include <QHash>
#include <QSet>
#include <QString>

// 自动消息翻译宿主（qltox auto-translate 链移植，chatview.cpp:4246 口径）：
// 触发方 = MessageListWidget::scanAutoFeatures；引擎 = fanyibot 多引擎
// 直连降级（fanyibotTranslateStart，qsktox 已编入）。
// 语义组块（vs VirtualBotHost 翻译 bot 链）：bot 链会把译文作为虚拟
// 联系人新行 ack 落库（messagepage.cpp:891 onBotDone）；自动翻译要原行内
// 渲译文 → 必须走独立 target 宿主，不复用 botDone。
// 默认开（决策 1=A）；toLang 固定 "中文"（qsktox 无设置面，qltox
// Config("translate_tolang") 值域对齐）。失败：控制台日志，UI 静默。
class TranslateHost : public QObject
{
    Q_OBJECT
public:
    static TranslateHost* instance();

    // 发起一次翻译；translateKey = 宿主侧定位键（dbRowid>0 优先，
    // 否则 localId；DB 加载行 localId=0，合成不发）
    void requestTranslate(qint64 translateKey, const QString& text);

    // 需求判定开关（默认 true；无设置面，留运行时口）
    static bool enabled() { return s_enabled; }
    static void setEnabled(bool on) { s_enabled = on; }

    // 自动翻译需求判定（translate_util.cpp:196-244 口径精简移植：
    // UTF-8 字节权重的拉丁/日/韩 → 中文比 < 0.27 → 需要译到中文；
    // 原文件的 compat34.h 依赖与 Qt6 目标不兼容，故本地化此纯函数）
    static bool needsTranslation(const QString& text);

Q_SIGNALS:
    void translateDone(qint64 translateKey, bool success,
                       const QString& translatedText, const QString& errorMsg);

protected:
    void customEvent(QEvent* event) override;

private:
    TranslateHost() : QObject() {}
    static TranslateHost* s_instance;
    static bool s_enabled;
    // 同 key 在途去重（多引擎降级耗时较长，扫描泵高频重入）
    QSet<qint64> m_inflight;
};

#endif // TRANSLATE_HOST_H