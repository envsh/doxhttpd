#include "translatehost.h"
#include "fanyibot.h"
#include "eventpoller.h"
#include <QDebug>

namespace {

// 需求判定字符分类（translate_util.cpp classifyChar 精简移植，仅取
// need 侧分支实质；差异：不做不可翻译区间跳过——URL/链接字符落入
// LATIN 计 need，偏保守方向：倾向多译不漏译）
enum TrCharClass {
    TRC_SKIP = 0,       // 空白 / 数字（原口径两不计）
    TRC_NEED = 1,       // 拉丁 / 西里尔 / 希腊
    TRC_NEED_JP_KR = 2, // 日文 / 韩文（权重 ×2）
    TRC_NONEED = 3,     // 其余（中日文字符等 → 中文已可读）
};

TrCharClass trClassify(uint32_t cp)
{
    if (cp == 0x20 || cp == 0x09 || cp == 0x0a || cp == 0x0d) {
        return TRC_SKIP;
    }
    if (cp >= 0x30 && cp <= 0x39) {
        return TRC_SKIP;
    }
    if ((cp >= 0x41 && cp <= 0x5A)
        || (cp >= 0x61 && cp <= 0x7A)
        || (cp >= 0xC0 && cp <= 0x24FF)     // Latin-1 扩展 + 拉丁扩展 + 希腊 + 西里尔
        || (cp >= 0xFF21 && cp <= 0xFF5A)) { // 全角拉丁
        return TRC_NEED;
    }
    if ((cp >= 0x3040 && cp <= 0x30FF)       // 平/片假名
        || (cp >= 0xFF66 && cp <= 0xFF9F)    // 半角片假名
        || (cp >= 0xAC00 && cp <= 0xD7AF)    // 谚文音节
        || (cp >= 0x1100 && cp <= 0x11FF)) { // 谚文字母
        return TRC_NEED_JP_KR;
    }
    return TRC_NONEED;
}

int trUtf8Bytes(uint32_t cp)
{
    return (cp < 0x80) ? 1 : (cp < 0x800) ? 2 : (cp < 0x10000) ? 3 : 4;
}

} // namespace

// 自动翻译需求判定（translate_util.cpp:196-244 口径精简移植：UTF-8 字节
// 权重的拉丁/日/韩 → 中文占比 < 0.27 → 需要译）
bool TranslateHost::needsTranslation(const QString& text)
{
    if (text.isEmpty()) {
        return false;
    }
    int need = 0;
    int noneed = 0;
    for (int i = 0; i < text.length(); ++i) {
        uint32_t cp = text.unicode()[i].unicode();
        if (cp >= 0xD800 && cp <= 0xDBFF
            && i + 1 < text.length()) {
            cp = 0x10000 + ((cp - 0xD800) << 10)
                + (text.unicode()[i + 1].unicode() - 0xDC00);
            ++i;
        }
        const TrCharClass cls = trClassify(cp);
        const int bytes = trUtf8Bytes(cp);
        if (cls == TRC_SKIP) {
            // 空格与数字不计入任何一组
        } else if (cls == TRC_NEED) {
            need += bytes;
        } else if (cls == TRC_NEED_JP_KR) {
            need += bytes * 2;
        } else {
            noneed += bytes;
        }
    }
    if (need == 0 && noneed == 0) {
        return false;
    }
    const float ratio = (float)noneed / (float)(noneed + need);
    return ratio < 0.27f;
}

TranslateHost* TranslateHost::s_instance = nullptr;
bool TranslateHost::s_enabled = true;

TranslateHost* TranslateHost::instance()
{
    if (!s_instance) {
        s_instance = new TranslateHost();
    }
    return s_instance;
}

void TranslateHost::requestTranslate(qint64 translateKey, const QString& text)
{
    if (!s_enabled || translateKey <= 0 || text.isEmpty()) {
        return;
    }
    if (m_inflight.contains(translateKey)) {
        return;
    }
    m_inflight.insert(translateKey);
    FanyibotRequest req;
    req.text = text.toUtf8().constData();
    req.toLang = "中文";
    req.engine = "any";
    req.localId = (long long)translateKey;
    req.chatId = 0;
    req.chatType = "auto";
    fanyibotTranslateStart(req, this);
}

void TranslateHost::customEvent(QEvent* event)
{
    if (event->type() != FanyibotDoneType) {
        QObject::customEvent(event);
        return;
    }
    auto* evt = static_cast<FanyibotDoneEvent*>(event);
    const qint64 key = (qint64)evt->localId;
    m_inflight.remove(key);
    if (!evt->success) {
        // 失败控制台日志（UI 静默不振荡；errorMsg 含引擎序与最后错误
        // 全文）
        qDebug() << "[translate] fail key=" << key
                 << QString::fromUtf8(evt->errorMsg.c_str());
        Q_EMIT translateDone(key, false, QString(),
            QString::fromUtf8(evt->errorMsg.c_str()));
        return;
    }
    Q_EMIT translateDone(key, true,
        QString::fromUtf8(evt->translatedText.c_str()),
        QString::fromUtf8(evt->engineUsed.c_str()));
}
