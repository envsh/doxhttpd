#include "aigptbot.h"
#include "compatcore34.h"
#include "eventpoller.h"
#include "cJSON.h"

#include <qapplication.h>
#include <qobject.h>
#include <qmutex.h>
#include <qglobal.h>
#include <qdatetime.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>
#include <map>

namespace {

// ── provider 静态表（顺序 = provider="all" 的固定尝试序）──
// 名称字段 name 原样拷贝自 imageaiutil.cpp（startOpenAiVision 第一个参数）
const AigptbotProvider kAigptProviders[] = {
    { kAigptbotPollinations,  "pollinations",    true,  false,   // 现已要求 key，any/all 跳过
      "https://gen.pollinations.ai/v1/chat/completions", "openai" },
    { kAigptbotPollinationsText,"pollinations(匿名)", false, true,
      "https://text.pollinations.ai/openai/chat/completions", "openai" },
    { kAigptbotZhipu,         "智谱",            true,  false,
      "https://open.bigmodel.cn/api/paas/v4/chat/completions", "" },
    { kAigptbotSiliconFlow,   "硅基流动",        true,  false,
      "https://api.siliconflow.cn/v1/chat/completions", "" },
    { kAigptbotNvidia,        "NVIDIA NIM",      true,  false,
      "https://integrate.api.nvidia.com/v1/chat/completions", "" },
    { kAigptbotOpenRouter,    "OpenRouter",      true,  false,
      "https://openrouter.ai/api/v1/chat/completions", "" },
    { kAigptbotBlockRun,      "BlockRun",        false, true,
      "https://blockrun.ai/api/v1/chat/completions", "nvidia/mistral-nemotron" },
    { kAigptbotLlm7,          "LLM7",            true,  false,
      "https://api.llm7.io/v1/chat/completions", "" },
    { kAigptbotCloudflare,    "Cloudflare",      true,  false,
      "https://api.cloudflare.com/client/v4/accounts/%1/ai/v1/chat/completions", "" },
    { kAigptbotDashScope,     "百炼",            true,  false,
      "https://dashscope.aliyun.com/compatible-mode/v1/chat/completions", "" },
    { kAigptbotOvh,           "OVH",             true, true,
      "https://oai.endpoints.kepler.ai.cloud.ovh.net/v1/chat/completions", "Qwen3.6-27B" },
    { kAigptbotVolcengine,    "豆包",            true,  false,
      "https://ark.cn-beijing.volces.com/api/v3/chat/completions", "" },
    { kAigptbotModelScope,    "ModelScope(国内)", true, false,
      "https://api-inference.modelscope.cn/v1/chat/completions", "" },
    { kAigptbotModelScopeIntl,"ModelScope(国际)", true, false,
      "https://api-inference.modelscope.ai/v1/chat/completions", "" },
    { kAigptbotGroq,          "Groq",            true,  false,
      "https://api.groq.com/openai/v1/chat/completions", "" },
    { kAigptbotHuggingFace,   "HuggingFace",     true,  false,
      "https://router.huggingface.co/v1/chat/completions", "" },
    { kAigptbotGemini,        "Gemini",          true,  false,
      "https://generativelanguage.googleapis.com/v1beta/openai/chat/completions", "" },
    { kAigptbotOllama,        "Ollama 本地",     false, true,
      "http://localhost:11434/v1/chat/completions", "qwen2.5" },
    { kAigptbotZai,           "Z.ai(国际)",      true,  false,
      "https://api.z.ai/api/paas/v4/chat/completions", "" },
    { kAigptbotGroqViaCf,     "groq-viacf",      true,  false,
      "https://gateway.ai.cloudflare.com/v1/%1/%2/groq/chat/completions", "" },
    { kAigptbotGeminiViaCf,   "gemini-viacf",    true,  false,
      "https://gateway.ai.cloudflare.com/v1/%1/%2/compat/chat/completions", "" },
    { kAigptbotAiHorde,       "aihorde",         true,  false,
      "", "" }, // 不走 OpenAI 兼容
};
const int kAigptProviderCount = int(sizeof(kAigptProviders) / sizeof(kAigptProviders[0]));

struct Session {
    AigptbotRequest req;
    QObject* target = nullptr;
    std::vector<const AigptbotProvider*> cands;
    int index = 0;
    unsigned long long seq = 0;
    std::string lastError;
    std::vector<std::string> tried;
};

QMutex g_mutex;
std::vector<Session*> g_sessions;
unsigned long long g_seq = 0;
bool g_seeded = false;

std::string trimStr(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) { return std::string(); }
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

bool strEqual(const std::string& a, const std::string& b) {
    return a == b;
}

// 日志截断：取首行、上限 64 字符（同 fanyibot::briefOf）
std::string briefBody(const std::string& body) {
    std::string b = body;
    const std::string::size_type nl = b.find('\n');
    if (nl != std::string::npos) { b = b.substr(0, nl); }
    if (b.size() > 64) { b = b.substr(0, 64); }
    return b;
}

void ensureSeed() {
    QMutexLocker lock(&g_mutex);
    if (g_seeded) { return; }
    g_seeded = true;
    srand((unsigned int)time(nullptr));
}

void removeSession(Session* s) {
    if (!s) { return; }
    {
        QMutexLocker lock(&g_mutex);
        for (size_t i = 0; i < g_sessions.size(); ++i) {
            if (g_sessions[i] == s) {
                g_sessions.erase(g_sessions.begin() + i);
                break;
            }
        }
    }
    delete s;
}

bool isCurrentSeq(const Session* s) {
    if (!s) { return false; }
    QMutexLocker lock(&g_mutex);
    for (size_t i = 0; i < g_sessions.size(); ++i) {
        const Session* o = g_sessions[i];
        if (o->req.chatId == s->req.chatId && o->req.chatType == s->req.chatType
            && o->req.localId == s->req.localId && o->seq > s->seq) {
            return false;
        }
    }
    return true;
}

void postResult(Session* s, bool ok, const std::string& reply,
                const std::string& providerUsed, const std::string& err) {
    if (!s) { return; }
    AigptbotDoneEvent* ev = new AigptbotDoneEvent();
    ev->chatId       = s->req.chatId;
    ev->chatType     = s->req.chatType;
    ev->localId      = s->req.localId;
    ev->success      = ok;
    ev->reply        = reply;
    ev->providerUsed = providerUsed;
    ev->errorMsg     = err;
    QObject* target  = s->target;
    removeSession(s);
    if (target) {
        QApplication::postEvent(target, ev);
    } else {
        delete ev;
    }
}

std::string jsonEscape(const std::string& in) {
    std::string out;
    for (size_t i = 0; i < in.size(); ++i) {
        const unsigned char c = (unsigned char)in[i];
        switch (c) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b";  break;
        case '\f': out += "\\f";  break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (c < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                out += buf;
            } else {
                out += (char)c;
            }
        }
    }
    return out;
}

// JSON 简易解析（仅提取需要字段）
std::string parseOpenAiContent(const std::string& body, std::string& apiErr) {
    apiErr.clear();
    std::string text = trimStr(body);
    if (text.empty()) { return std::string(); }
    cJSON* root = cJSON_Parse(text.c_str());
    if (!root) { return std::string(); }
    // error.message
    cJSON* err = cJSON_GetObjectItem(root, "error");
    if (err && cJSON_IsObject(err)) {
        cJSON* msg = cJSON_GetObjectItem(err, "message");
        if (msg && cJSON_IsString(msg) && msg->valuestring) {
            apiErr = msg->valuestring;
        }
    }
    // choices[0].message.content
    std::string content;
    cJSON* choices = cJSON_GetObjectItem(root, "choices");
    if (choices && cJSON_IsArray(choices) && cJSON_GetArraySize(choices) > 0) {
        cJSON* first = cJSON_GetArrayItem(choices, 0);
        if (first && cJSON_IsObject(first)) {
            cJSON* message = cJSON_GetObjectItem(first, "message");
            if (message && cJSON_IsObject(message)) {
                cJSON* c = cJSON_GetObjectItem(message, "content");
                if (c && cJSON_IsString(c) && c->valuestring) {
                    content = c->valuestring;
                }
            } else {
                cJSON* c = cJSON_GetObjectItem(first, "text");
                if (c && cJSON_IsString(c) && c->valuestring) {
                    content = c->valuestring;
                }
            }
        }
    }
    cJSON_Delete(root);
    if (content == std::string("[object Object]")) { return std::string(); }
    return trimStr(content);
}

void onDone(const HttpResponse& resp, void* udata);

void sendHttp(Session* s, const std::string& url, const std::string& body,
              const std::map<std::string, std::string>& headers) {
    HttpRequest req(url, "POST", body, 51, headers);
    req.followRedirects = true;
    EventPoller::addRequest(req, onDone, s);
}

void startNext(Session* s);

void sendHost(Session* s, const AigptbotProvider& p) {
    s->tried.push_back(p.name);
    // AI Horde：直接指定时立即失败
    if (p.id == kAigptbotAiHorde) {
        postResult(s, false, std::string(), p.name,
                   std::string("AI Horde 暂不支持文本对话"));
        return;
    }
    const std::string model = s->req.model.empty() ? p.modelDefault : s->req.model;
    qWarning("aigptbot: 候选[%d/%d] provider=%s host=%s model=%s brief=%d textlen=%d B",
             s->index, (int)s->cands.size(), p.name, p.baseUrl,
             model.empty() ? "(未指定)" : model.c_str(), (int)s->req.brief, (int)s->req.text.size());
    // 构造 OpenAI 兼容请求体
    std::string body = "{";
    // model（UI 未指定时回落 provider 默认模型）
    if (!model.empty()) {
        body += "\"model\":\"" + jsonEscape(model) + "\",";
    }
    // 显式关闭流式：避免服务端 SSE/keep-alive 不关连接导致 200+超时误判
    body += "\"stream\":false,";
    // 简洁回复：max_tokens 兜底 + system 简洁指令引导
    if (s->req.brief) {
        body += "\"max_tokens\":512,";
    }
    // messages
    body += "\"messages\":[";
    if (s->req.brief) {
        body += "{\"role\":\"system\",\"content\":\""
                + jsonEscape("请尽量用最简洁的方式回答，简短直接，避免冗长。") + "\"},";
    }
    body += "{\"role\":\"user\",\"content\":\""
            + jsonEscape(s->req.text) + "\"}]";
    body += "}";
    // headers
    std::map<std::string, std::string> headers;
    headers["User-Agent"] = "qltox/1.0";
    headers["Accept-Language"] = "zh-CN,zh;q=0.9,en;q=0.4";
    headers["Content-Type"] = "application/json";
    // Authorization：仅非 allowEmptyKey
    if (!p.allowEmptyKey) {
        // 不读取配置文件，key 为空→不添加
        // （直接指定场景将发出无 auth 请求，失败由 onDone 处理）
    }
    sendHttp(s, p.baseUrl, body, headers);
}

void startNext(Session* s) {
    if (!s) { return; }
    if (s->index < (int)s->cands.size()) {
        sendHost(s, *s->cands[s->index]);
        return;
    }
    std::string err = s->lastError.empty()
            ? std::string("所有候选服务均失败")
            : s->lastError;
    std::string tried;
    for (size_t i = 0; i < s->tried.size(); ++i) {
        if (i) { tried += ", "; }
        tried += s->tried[i];
    }
    if (!tried.empty()) { err += "（已尝试：" + tried + "）"; }
    qWarning("aigptbot: 全部候选失败: %s", err.c_str());
    postResult(s, false, std::string(), std::string(), err);
}

void onDone(const HttpResponse& resp, void* udata) {
    Session* s = static_cast<Session*>(udata);
    if (!s) { return; }
    if (!isCurrentSeq(s)) {
        qWarning("aigptbot: 迟到回包丢弃 seq=%u", (unsigned int)s->seq);
        removeSession(s);
        return;
    }
    if (s->index < 0 || s->index >= (int)s->cands.size()) {
        removeSession(s);
        return;
    }
    const AigptbotProvider* h = s->cands[s->index];
    std::string apiErr;
    qWarning("aigptbot: resp provider=%s http=%d curl=[%s] body=[%s]",
             h ? h->name : "?", resp.httpCode,
             resp.curlErrStr.c_str(), briefBody(resp.body).c_str());
    if (resp.httpCode >= 200 && resp.httpCode < 300) {
        const std::string content = parseOpenAiContent(resp.body, apiErr);
        if (!content.empty()) {
            qWarning("aigptbot: provider=%s 成功 textlen=%d B",
                     h ? h->name : "?", (int)content.size());
            postResult(s, true, content, h ? h->name : std::string(), std::string());
            return;
        }
    }
    // 失败
    std::string lastErr;
    if (h) { lastErr += h->name; } else { lastErr += "provider"; }
    lastErr += ": HTTP " + std::to_string(resp.httpCode);
    if (!resp.curlErrStr.empty()) { lastErr += " " + resp.curlErrStr; }
    if (resp.httpCode >= 200 && resp.httpCode < 300 && !resp.curlErrStr.empty()) {
        lastErr += "（收到 HTTP " + std::to_string(resp.httpCode) + " 但响应体不完整）";
    }
    if (!apiErr.empty()) { lastErr += "（" + apiErr + "）"; }
    s->lastError = lastErr;
    qWarning("aigptbot: provider=%s 失败: %s → 切换候选", h ? h->name : "?", lastErr.c_str());
    ++s->index;
    startNext(s);
}

std::vector<const AigptbotProvider*> buildCandidates(const AigptbotRequest& req,
                                                   std::string& reason) {
    std::vector<const AigptbotProvider*> out;
    std::string prov = req.provider;
    // 空串视为 any
    if (prov.empty()) { prov = "any"; }
    if (prov == "all") {
        for (int i = 0; i < kAigptProviderCount; ++i) {
            const AigptbotProvider& p = kAigptProviders[i];
            if (p.id == kAigptbotAiHorde) { continue; } // any/all 跳过
            if (p.needsKey) { continue; } // 不读配置文件，视为未配置→跳过
            out.push_back(&p);
        }
        if (out.empty()) {
            reason = "any/all：无可用候选（未配置 key 或仅含暂不支持项）";
        }
        return out;
    }
    if (prov == "any") {
        for (int i = 0; i < kAigptProviderCount; ++i) {
            const AigptbotProvider& p = kAigptProviders[i];
            if (p.id == kAigptbotAiHorde) { continue; } // any/all 跳过
            if (p.needsKey) { continue; } // 跳过未配置
            out.push_back(&p);
        }
        if (!out.empty()) {
            // Fisher–Yates
            for (size_t i = out.size() - 1; i > 0; --i) {
                const size_t j = (size_t)rand() % (i + 1);
                std::swap(out[i], out[j]);
            }
        } else {
            reason = "any/all：无可用候选（未配置 key 或仅含暂不支持项）";
        }
        return out;
    }
    // 指定
    for (int i = 0; i < kAigptProviderCount; ++i) {
        const AigptbotProvider& p = kAigptProviders[i];
        if (prov == p.name) {
            // 指定：不跳过（包括 AI Horde、needsKey）
            out.push_back(&p);
            return out;
        }
    }
    reason = "未知服务：" + prov;
    return out;
}

} // namespace

const std::vector<AigptbotProvider>& aigptbotProviders() {
    static std::vector<AigptbotProvider> list(
        kAigptProviders, kAigptProviders + kAigptProviderCount);
    return list;
}

void aigptbotChatStart(const AigptbotRequest& req, QObject* target) {
    if (!target) { return; }
    ensureSeed();
    Session* s = new Session();
    s->req = req;
    s->target = target;
    {
        QMutexLocker lock(&g_mutex);
        s->seq = ++g_seq;
        g_sessions.push_back(s);
    }
    std::string reason;
    s->cands = buildCandidates(req, reason);
    if (!reason.empty()) { s->lastError = reason; }
    std::string order;
    for (size_t i = 0; i < s->cands.size(); ++i) {
        if (i) { order += " → "; }
        order += s->cands[i]->name;
    }
    qWarning("aigptbot: provider=%s model=%s 候选 %d 个: %s",
             req.provider.empty() ? "any" : req.provider.c_str(),
             req.model.empty() ? "(默认)" : req.model.c_str(),
             (int)s->cands.size(), order.c_str());
    startNext(s);
}
