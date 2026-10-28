#ifndef LINK_PREVIEW_HOST_H
#define LINK_PREVIEW_HOST_H

#include <QObject>
#include <QString>
#include "linkpreview.h"

// 自动链接预览宿主（qltox auto link-preview 链移植，chatview.cpp:4813
// 口径）语义 = 请求 URL → 抓内容信息 → 更新消息绘制；本批不接点击跳转
//（另批）。纯文本卡（site/title/desc），无缩略图。
// 默认开（qltox "media/linkPreview" 默认 true 口径）。
// 失败路径：HTTP 非 200 / curl 错 / Content-Type 非 html / body 截断 /
// 解析全字段 miss → 控制台 qDebug 日志 + LinkPreviewCache 负缓存，
// UI 静默不振荡。
class LinkPreviewHost : public QObject
{
    Q_OBJECT
public:
    static LinkPreviewHost* instance();

    // 发起一次抓取；url 先 linkPreviewNormalizeUrl 作缓存键
    void requestPreview(const QString& url);

    static bool enabled() { return s_enabled; }
    static void setEnabled(bool on) { s_enabled = on; }

Q_SIGNALS:
    // data.ok=false 同样触发（负缓存命中后行侧置 Failed）
    void linkReady(const QString& urlKey, const LinkPreviewData& data);

protected:
    void customEvent(QEvent* event) override;

private:
    LinkPreviewHost() : QObject() {}
    static LinkPreviewHost* s_instance;
    static bool s_enabled;
};

#endif // LINK_PREVIEW_HOST_H