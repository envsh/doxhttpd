#include "fanyibot.h"
#include "compatcore34.h"
#include "eventpoller.h"
#include "cJSON.h"

#include <qapplication.h>
#include <qobject.h>
#include <qmutex.h>
#include <qglobal.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <ctime>
#include <sys/stat.h>

extern "C" {
#include "md5.h"
}

namespace {

// ── 引擎静态表（顺序 = engine="all" 的固定尝试序）──
const FanyibotEngine kEngines[] = {
    { kFanyibotMsedge, "msedge", false },
    { kFanyibotGoogle, "google", false },
    { kFanyibotYoudao, "youdao", false },
    { kFanyibotYandex, "yandex", false },
    { kFanyibotDeepl,  "deepl",  true  },
};
const int kEngineCount = int(sizeof(kEngines) / sizeof(kEngines[0]));

// msedge 取 token 必须带 msie UA（有道/雅翻走浏览器 UA）
const char* kMsedgeUA = "msie 6";
const char* kBrowserUA = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
                         "(KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36";
const char* kYoudaoSignKey = "fanyideskweb";

// 目标语言显示名 → 各家语言码
struct LangRow {
    const char* display;
    const char* msedge;
    const char* google;
    const char* youdao;
    const char* yandex;
};
const LangRow kLangs[] = {
    { "中文",     "zh-Hans", "zh-CN", "zh-CHS", "zh" },
    { "繁體中文", "zh-Hant", "zh-TW",  "zh-CHT", "zh" },
    { "日本語",   "ja",      "ja",     "ja",     "ja" },
    { "한국어",   "ko",      "ko",     "ko",     "ko" },
    { "English",  "en",      "en",     "en",     "en" },
    { "Français", "fr",      "fr",     "fr",     "fr" },
    { "Deutsch",  "de",      "de",     "de",     "de" },
    { "Русский",  "ru",      "ru",     "ru",     "ru" },
    { "العربية",  "ar",      "ar",     "ar",     "ar" },
    { "地球语",   "eo",      "eo",     "eo",     "eo" },
};
const int kLangCount = int(sizeof(kLangs) / sizeof(kLangs[0]));

struct Session {
    FanyibotRequest req;
    QObject* target = nullptr;
    std::vector<const FanyibotEngine*> cands;
    int index = 0;
    unsigned int seq = 0;
    std::string lastError;
    std::vector<std::string> tried;
    std::string cred;    // msedge JWT / yandex key
    int step = 0;        // 0=取凭据 1=发翻译请求
    int credRetry = 0;   // 凭据失效重取次数（上限 1，防死循环）
};

QMutex g_mutex;
std::vector<Session*> g_sessions;
unsigned int g_seq = 0;
bool g_seeded = false;

std::string trimStr(const std::string& s) {
    const size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) { return std::string(); }
    const size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

std::string urlEncode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char c = (unsigned char)s[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
            || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0xF];
        }
    }
    return out;
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        const unsigned char c = (unsigned char)s[i];
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;
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

std::string jsonStr(cJSON* obj, const char* key) {
    if (!obj) { return std::string(); }
    cJSON* it = cJSON_GetObjectItem(obj, key);
    if (!it || !cJSON_IsString(it)) { return std::string(); }
    const char* v = cJSON_GetStringValue(it);
    return v ? std::string(v) : std::string();
}

std::string md5Hex(const std::string& s) {
    MD5_CTX ctx;
    uint8_t digest[16];
    MD5_Init(&ctx);
    MD5_Update(&ctx, (const uint8_t*)s.data(), s.size());
    MD5_Final(digest, &ctx);
    static const char* hex = "0123456789abcdef";
    std::string out;
    for (int i = 0; i < 16; i++) {
        out += hex[(digest[i] >> 4) & 0xF];
        out += hex[digest[i] & 0xF];
    }
    return out;
}

// ── 凭据缓存：$HOME/.config/qltox/fanyibot_<name>（0600，存短期 token/key）──
std::string credDir() {
    return std::string(qToUtf8(qGetHomePath() + "/.config/qltox").data());
}

std::string loadCred(const char* name) {
    const std::string path = credDir() + "/fanyibot_" + name;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { return std::string(); }
    std::string out;
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) { out.append(buf, n); }
    std::fclose(f);
    return trimStr(out);
}

void saveCred(const char* name, const std::string& val) {
    if (val.empty()) { return; }
    const std::string dir = credDir();
    qMkdir(qFromUtf8(dir.c_str()));
    const std::string path = dir + "/fanyibot_" + name;
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { return; }
    std::fwrite(val.data(), 1, val.size(), f);
    std::fclose(f);
    ::chmod(path.c_str(), 0600);
}

void removeCred(const char* name) {
    std::remove((credDir() + "/fanyibot_" + name).c_str());
}

void removeSession(Session* s) {
    {
        QMutexLocker lock(&g_mutex);
        for (size_t i = 0; i < g_sessions.size(); i++) {
            if (g_sessions[i] == s) {
                g_sessions.erase(g_sessions.begin() + i);
                break;
            }
        }
    }
    delete s;
}

// 同一消息（chatId+chatType+localId）已有更新的会话时，本次回包作废
bool isCurrentSeq(const Session* s) {
    QMutexLocker lock(&g_mutex);
    for (size_t i = 0; i < g_sessions.size(); i++) {
        const Session* o = g_sessions[i];
        if (o->req.chatId == s->req.chatId && o->req.chatType == s->req.chatType
            && o->req.localId == s->req.localId && o->seq > s->seq) {
            return false;
        }
    }
    return true;
}

void postResult(Session* s, bool ok, const std::string& text,
                const std::string& engineUsed, const std::string& err) {
    FanyibotDoneEvent* ev = new FanyibotDoneEvent();
    ev->chatId         = s->req.chatId;
    ev->chatType       = s->req.chatType;
    ev->localId        = s->req.localId;
    ev->success        = ok;
    ev->translatedText = text;
    ev->engineUsed     = engineUsed;
    ev->errorMsg       = err;
    QObject* target    = s->target;
    removeSession(s);
    if (target) { QApplication::postEvent(target, ev); }
    else { delete ev; }
}

std::string httpErr(const HttpResponse& resp) {
    std::string err = "HTTP " + std::to_string(resp.httpCode);
    if (!resp.curlErrStr.empty()) { err += " " + resp.curlErrStr; }
    return err;
}

// msedge: [{"translations":[{"text":"…"}]}]
std::string parseMsedge(const std::string& body) {
    cJSON* root = cJSON_Parse(body.c_str());
    std::string out;
    if (root) {
        cJSON* arr = cJSON_GetObjectItem(root, "translations");
        cJSON* first = arr ? cJSON_GetArrayItem(arr, 0) : nullptr;
        out = first ? jsonStr(first, "text") : std::string();
        cJSON_Delete(root);
    }
    return out;
}

// msedge 错误码（401001 = token 失效）
int msedgeErrorCode(const std::string& body) {
    cJSON* root = cJSON_Parse(body.c_str());
    int code = 0;
    if (root) {
        cJSON* err = cJSON_GetObjectItem(root, "error");
        cJSON* c = err ? cJSON_GetObjectItem(err, "code") : nullptr;
        if (c && cJSON_IsNumber(c)) { code = (int)c->valuedouble; }
        cJSON_Delete(root);
    }
    return code;
}

// google: [[["译文","原文",…],…
std::string parseGoogle(const std::string& body) {
    const size_t head = body.find("[[\"");
    if (head == std::string::npos) { return std::string(); }
    size_t start = head + 3;
    if (start >= body.size()) { return std::string(); }
    if (body[start] == '"') { start++; }
    const size_t b = body.find('"', start);
    if (b == std::string::npos) { return std::string(); }
    return body.substr(start, b - start);
}

// 有道：translateResult[0].t，退回 translation[0].t
std::string parseYoudao(const std::string& body) {
    cJSON* root = cJSON_Parse(body.c_str());
    std::string out;
    if (root) {
        cJSON* tr = cJSON_GetObjectItem(root, "translateResult");
        cJSON* first = tr ? cJSON_GetArrayItem(tr, 0) : nullptr;
        if (first) { out = jsonStr(first, "t"); }
        if (out.empty()) {
            cJSON* tl = cJSON_GetObjectItem(root, "translation");
            cJSON* t0 = tl ? cJSON_GetArrayItem(tl, 0) : nullptr;
            if (t0) { out = jsonStr(t0, "t"); }
        }
        cJSON_Delete(root);
    }
    return out;
}

// 雅翻：{"code":200,"text":["译文",…]}
std::string parseYandex(const std::string& body) {
    cJSON* root = cJSON_Parse(body.c_str());
    std::string out;
    if (root) {
        cJSON* arr = cJSON_GetObjectItem(root, "text");
        cJSON* first = arr ? cJSON_GetArrayItem(arr, 0) : nullptr;
        if (first && cJSON_IsString(first)) {
            const char* v = cJSON_GetStringValue(first);
            if (v) { out = v; }
        }
        cJSON_Delete(root);
    }
    return out;
}

// 雅翻 key 藏在首页 JS 配置里：…"key":"XXXX"…
std::string scrapeYandexKey(const std::string& html) {
    const std::string tag = "\"key\":\"";
    size_t pos = 0;
    while (true) {
        const size_t at = html.find(tag, pos);
        if (at == std::string::npos) { return std::string(); }
        const size_t b = at + tag.size();
        const size_t e = html.find('"', b);
        if (e == std::string::npos) { return std::string(); }
        const std::string key = html.substr(b, e - b);
        if (!key.empty() && key.size() >= 8 && key.find(' ') == std::string::npos) {
            return key;
        }
        pos = e + 1;
    }
}

std::vector<const FanyibotEngine*> buildCandidates(const FanyibotRequest& req,
                                                   std::string& reason) {
    std::vector<const FanyibotEngine*> out;
    if (req.engine == "all" || req.engine.empty() || req.engine == "any") {
        for (int i = 0; i < kEngineCount; i++) {
            if (!kEngines[i].unsupported) { out.push_back(&kEngines[i]); }
        }
        if (out.empty()) {
            reason = "没有可用引擎：候选表全为不可用";
            return out;
        }
        if (req.engine != "all") {   // any：随机取序
            for (size_t i = out.size() - 1; i > 0; i--) {
                const size_t j = (size_t)rand() % (i + 1);
                std::swap(out[i], out[j]);
            }
        }
        return out;
    }
    for (int i = 0; i < kEngineCount; i++) {
        if (req.engine != kEngines[i].name) { continue; }
        if (kEngines[i].unsupported) {
            reason = std::string(kEngines[i].name) + " 需官方 auth key，暂不可用";
        } else {
            out.push_back(&kEngines[i]);
        }
        return out;
    }
    reason = "未知引擎：" + req.engine;
    return out;
}

void onDone(const HttpResponse& resp, void* udata);
void startNext(Session* s);

void failEngine(Session* s, const FanyibotEngine& e, const std::string& why) {
    s->lastError = std::string(e.name) + ": " + why;
    ++s->index;
    startNext(s);
}

void sendHttp(Session* s, const std::string& url, const std::string& method,
              const std::string& body,
              const std::map<std::string, std::string>& headers) {
    HttpRequest req(url, method, body, 20, headers);
    req.followRedirects = true;
    EventPoller::addRequest(req, onDone, s);
}

void sendEngine(Session* s, const FanyibotEngine& e) {
    if (s->tried.empty() || s->tried.back() != e.name) { s->tried.push_back(e.name); }
    const std::string code = fanyibotLangCode(e.id, s->req.toLang);
    if (code.empty()) {
        failEngine(s, e, "不支持目标语言 " + s->req.toLang);
        return;
    }
    const std::string& text = s->req.text;
    std::map<std::string, std::string> h;

    if (e.id == kFanyibotMsedge) {
        if (s->step == 0) {   // 先取 token（缓存优先）
            if (s->cred.empty()) { s->cred = loadCred("msedge_token"); }
            if (!s->cred.empty()) { s->step = 1; sendEngine(s, e); return; }
            h["User-Agent"] = kMsedgeUA;
            sendHttp(s, "https://edge.microsoft.com/translate/auth", "GET", "", h);
            return;
        }
        h["User-Agent"]    = kMsedgeUA;
        h["Content-Type"]  = "application/json; charset=utf-8";
        h["Authorization"] = "Bearer " + s->cred;
        sendHttp(s, "https://api.cognitive.microsofttranslator.com/translate?to=" + code
                    + "&api-version=3.0&includeSentenceLength=true",
                 "POST", "[{\"text\":\"" + jsonEscape(text) + "\"}]", h);
        return;
    }

    if (e.id == kFanyibotGoogle) {
        h["User-Agent"] = kBrowserUA;
        sendHttp(s, "https://translate.googleapis.com/translate_a/single?client=gtx"
                    "&sl=auto&tl=" + code + "&dt=t&q=" + urlEncode(text),
                 "GET", "", h);
        return;
    }

    if (e.id == kFanyibotYoudao) {
        const std::string salt = std::to_string((long long)time(nullptr));
        const std::string body = "i=" + urlEncode(text) + "&from=auto&to=" + code
            + "&smartresult=dict&client=pc&salt=" + urlEncode(salt)
            + "&sign=" + md5Hex(std::string(kYoudaoSignKey) + text + salt);
        h["User-Agent"]   = kBrowserUA;
        h["Referer"]      = "https://fanyi.youdao.com/";
        h["Content-Type"] = "application/x-www-form-urlencoded; charset=UTF-8";
        sendHttp(s, "https://fanyi.youdao.com/translate", "POST", body, h);
        return;
    }

    if (e.id == kFanyibotYandex) {
        if (s->step == 0) {   // key 藏在首页 JS 里，抓一次缓存
            if (s->cred.empty()) { s->cred = loadCred("yandex_key"); }
            if (!s->cred.empty()) { s->step = 1; sendEngine(s, e); return; }
            h["User-Agent"] = kBrowserUA;
            sendHttp(s, "https://translate.yandex.com/", "GET", "", h);
            return;
        }
        h["User-Agent"] = kBrowserUA;
        sendHttp(s, "https://translate.yandex.com/api/v1/tr.json/translate?id="
                    + std::to_string((long long)time(nullptr))
                    + "&srv=trg&lang=auto-" + code + "&reason=auto&format=text"
                    + "&key=" + urlEncode(s->cred) + "&text=" + urlEncode(text),
                 "GET", "", h);
        return;
    }

    failEngine(s, e, "暂不可用");
}

void startNext(Session* s) {
    if (s->index < (int)s->cands.size()) {
        sendEngine(s, *s->cands[s->index]);
        return;
    }
    std::string err = s->lastError.empty() ? std::string("所有候选引擎均失败") : s->lastError;
    std::string tried;
    for (size_t i = 0; i < s->tried.size(); i++) {
        if (i) { tried += ", "; }
        tried += s->tried[i];
    }
    if (!tried.empty()) { err += "（已尝试：" + tried + "）"; }
    postResult(s, false, std::string(), std::string(), err);
}

void onDone(const HttpResponse& resp, void* udata) {
    Session* s = static_cast<Session*>(udata);
    if (!s) { return; }
    if (!isCurrentSeq(s)) { removeSession(s); return; }   // 迟到回包：已被重试覆盖
    const FanyibotEngine* e =
        (s->index < (int)s->cands.size()) ? s->cands[s->index] : nullptr;
    if (!e) { removeSession(s); return; }

    const bool httpOk = resp.httpCode >= 200 && resp.httpCode < 300
                        && resp.curlErrStr.empty();

    if (e->id == kFanyibotMsedge) {
        if (s->step == 0) {
            const std::string token = trimStr(resp.body);
            if (httpOk && !token.empty()) {
                saveCred("msedge_token", token);
                s->cred = token;
                s->step = 1;
                sendEngine(s, *e);
                return;
            }
            failEngine(s, *e, httpErr(resp));
            return;
        }
        if (httpOk) {
            if (msedgeErrorCode(resp.body) == 401001 && s->credRetry < 1) {
                ++s->credRetry;    // token 过期：丢弃缓存重取一次
                removeCred("msedge_token");
                s->step = 0;
                s->cred.clear();
                sendEngine(s, *e);
                return;
            }
            const std::string out = parseMsedge(resp.body);
            if (!out.empty()) {
                postResult(s, true, out, e->name, std::string());
                return;
            }
        }
        failEngine(s, *e, httpErr(resp));
        return;
    }

    if (e->id == kFanyibotGoogle) {
        if (httpOk) {
            const std::string out = parseGoogle(resp.body);
            if (!out.empty()) {
                postResult(s, true, out, e->name, std::string());
                return;
            }
        }
        failEngine(s, *e, httpErr(resp));
        return;
    }

    if (e->id == kFanyibotYoudao) {
        if (httpOk) {
            const std::string out = parseYoudao(resp.body);
            if (!out.empty()) {
                postResult(s, true, out, e->name, std::string());
                return;
            }
        }
        failEngine(s, *e, httpErr(resp));
        return;
    }

    if (e->id == kFanyibotYandex) {
        if (s->step == 0) {
            const std::string key = httpOk ? scrapeYandexKey(resp.body) : std::string();
            if (!key.empty()) {
                saveCred("yandex_key", key);
                s->cred = key;
                s->step = 1;
                sendEngine(s, *e);
                return;
            }
            failEngine(s, *e, httpOk ? "首页未解析到 key" : httpErr(resp));
            return;
        }
        if (httpOk) {
            const std::string out = parseYandex(resp.body);
            if (!out.empty()) {
                postResult(s, true, out, e->name, std::string());
                return;
            }
        }
        failEngine(s, *e, httpErr(resp));
        return;
    }

    failEngine(s, *e, "暂不可用");
}

void ensureSeed() {
    QMutexLocker lock(&g_mutex);
    if (g_seeded) { return; }
    g_seeded = true;
    srand((unsigned int)time(nullptr));
}

} // namespace

const std::vector<FanyibotEngine>& fanyibotEngines() {
    static std::vector<FanyibotEngine> engines(kEngines, kEngines + kEngineCount);
    return engines;
}

std::string fanyibotLangCode(FanyibotEngineId id, const std::string& toLang) {
    for (int i = 0; i < kLangCount; i++) {
        if (toLang != kLangs[i].display) { continue; }
        switch (id) {
        case kFanyibotMsedge: return kLangs[i].msedge;
        case kFanyibotGoogle: return kLangs[i].google;
        case kFanyibotYoudao: return kLangs[i].youdao;
        case kFanyibotYandex: return kLangs[i].yandex;
        case kFanyibotDeepl:  break;   // 暂不可用，无语言码
        }
        return std::string();
    }
    return std::string();
}

void fanyibotTranslateStart(const FanyibotRequest& req, QObject* target) {
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
    for (size_t i = 0; i < s->cands.size(); i++) {
        if (i) { order += " → "; }
        order += s->cands[i]->name;
    }
    qWarning("fanyibot: engine=%s toLang=%s 候选 %d 个: %s",
             req.engine.empty() ? "any" : req.engine.c_str(),
             req.toLang.c_str(), (int)s->cands.size(), order.c_str());
    startNext(s);
}