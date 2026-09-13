#include "eventpoller.h"
#include "limelog.h"

const char* const kImapMailType       = "imap_mail";
const char* const kGomuksRoomType     = "gomuks_room";
const char* const kUnktoxFriendType   = "unktox_friend";
const char* const kUnktoxConferenceType = "unktox_conference";
const char* const kUnktoxGroupType    = "unktox_group";
const char* const kSyseventType       = "sysevent";
const char* const kTopicType          = "topic";
const char* const kFilesyncType       = "filesync";
const char* const kClipboardType      = "clipboard";
const char* const kUnknownType        = "unknown";
const char* const kBookmarkType       = "bookmark";
const char* const kAichatType         = "aichat";
const char* const kPastebinType       = "pastebin";
const char* const kTranslateType      = "translate";
const char* const kMisskeyType        = "misskey_note";
#include <unistd.h>

EventPoller* EventPoller::s_instance = nullptr;

static size_t writeCb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

static size_t headerCb(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* headers = static_cast<std::map<std::string, std::string>*>(userp);
    std::string line(static_cast<char*>(contents), total);
    size_t colon = line.find(':');
    if (colon == std::string::npos || colon == 0) { return total; }
    std::string name = line.substr(0, colon);
    size_t vStart = colon + 1;
    while (vStart < line.size() && (line[vStart] == ' ' || line[vStart] == '\t')) { vStart++; }
    size_t vEnd = line.size();
    while (vEnd > vStart && (line[vEnd - 1] == '\r' || line[vEnd - 1] == '\n')) vEnd--;
    if (vEnd <= vStart) { return total; }
    for (char& c : name) { c = static_cast<char>(tolower(static_cast<unsigned char>(c))); }
    (*headers)[name] = line.substr(vStart, vEnd - vStart);
    return total;
}

// 下载进度回调（curl_multi pump 线程内）：≥100ms 或传输完成才转发一次
static int xferinfoCb(void* clientp, curl_off_t dltotal, curl_off_t dlnow,
                      curl_off_t ultotal, curl_off_t ulnow) {
    (void)ultotal; (void)ulnow;
    auto* ctx = static_cast<HttpCtx*>(clientp);
    if (!ctx || !ctx->progress) { return 0; }
    bool done = (dltotal > 0 && dlnow >= dltotal);
    if (done || elapsedMs(ctx->lastEmitTp) >= 100) {
        ctx->lastEmitTp = timeNow();
        ctx->progress((long long)dlnow, (long long)dltotal, ctx->udata);
    }
    return 0;
}

EventPoller::EventPoller()
    : QThread(), running(true), multi(nullptr) {
    curl_global_init(CURL_GLOBAL_DEFAULT);
    multi = curl_multi_init();
    s_instance = this;
}

void EventPoller::start() {
    if (!s_instance) {
        auto* ep = new EventPoller();
        ep->QThread::start();
    }
}

void EventPoller::stop() {
    if (!s_instance) { return; }
    s_instance->running = false;
    s_instance->wait();
    if (s_instance->multi) {
        curl_multi_cleanup(s_instance->multi);
        s_instance->multi = nullptr;
    }
    delete s_instance;
    s_instance = nullptr;
}

void EventPoller::addRequest(const HttpRequest& req,
                              void (*done)(const HttpResponse& resp, void* udata),
                              void* udata) {
    if (!s_instance || !s_instance->multi) { return; }

    ALOG_INFO(">>", req.method, req.url);

    auto t0 = timeNow();

    CURL* easy = curl_easy_init();
    if (!easy) { return; }

    auto* ctx = new HttpCtx{req.url, req.data,
                             std::string(), std::map<std::string, std::string>(),
                             nullptr, done, udata, req.progress, timeNow()};

    curl_easy_setopt(easy, CURLOPT_URL, ctx->urlStr.c_str());
    curl_easy_setopt(easy, CURLOPT_PRIVATE, ctx);
    curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, writeCb);
    curl_easy_setopt(easy, CURLOPT_WRITEDATA, &ctx->body);
    curl_easy_setopt(easy, CURLOPT_HEADERFUNCTION, headerCb);
    curl_easy_setopt(easy, CURLOPT_HEADERDATA, &ctx->headers);
    curl_easy_setopt(easy, CURLOPT_TIMEOUT, (long)req.timeoutSec);
    curl_easy_setopt(easy, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(easy, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(easy, CURLOPT_FRESH_CONNECT, 1L);
    curl_easy_setopt(easy, CURLOPT_SSL_SESSIONID_CACHE, 0L);

    // 下载进度：≥100ms 或传输完成才转发一次，避免高频重绘刷新
    if (req.progress) {
        curl_easy_setopt(easy, CURLOPT_NOPROGRESS, 0L);
        curl_easy_setopt(easy, CURLOPT_XFERINFOFUNCTION, xferinfoCb);
        curl_easy_setopt(easy, CURLOPT_XFERINFODATA, ctx);
    }

    if (req.method == "POST") {
        curl_easy_setopt(easy, CURLOPT_POSTFIELDS, ctx->postData.c_str());
        curl_easy_setopt(easy, CURLOPT_POSTFIELDSIZE, (long)ctx->postData.size());
    }

    if (!req.extraHeaders.empty()) {
        for (const auto& h : req.extraHeaders) {
            std::string hv = h.first + ": " + h.second;
            ctx->requestHeaders = curl_slist_append(ctx->requestHeaders, hv.c_str());
        }
        curl_easy_setopt(easy, CURLOPT_HTTPHEADER, ctx->requestHeaders);
    }

    s_instance->multiMutex.lock();
    curl_multi_add_handle(s_instance->multi, easy);
    s_instance->multiMutex.unlock();

    ALOG_INFO("addRequest setup took", timeSince(t0), "for", ctx->urlStr);
}

void EventPoller::run() {
    while (running) {
        int numfds;
        curl_multi_wait(multi, NULL, 0, 50, &numfds);
        int r;
        while (curl_multi_perform(multi, &r) == CURLM_CALL_MULTI_PERFORM) { }

        CURLMsg* msg;
        int left;
        while ((msg = curl_multi_info_read(multi, &left))) {
            if (msg->msg == CURLMSG_DONE) {
                HttpCtx* ctx;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_PRIVATE, &ctx);
                long httpCode = 0;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_RESPONSE_CODE, &httpCode);
                int curlResult = msg->data.result;
                HttpResponse resp;
                resp.httpCode = (int)httpCode;
                if (curlResult != 0) {
                    resp.curlErrStr = curl_easy_strerror((CURLcode)curlResult);
                }
                resp.body = std::move(ctx->body);
                resp.headers = std::move(ctx->headers);
                double totalTime = 0.0;
                curl_easy_getinfo(msg->easy_handle, CURLINFO_TOTAL_TIME, &totalTime);
                resp.elapsedMs = (int64_t)(totalTime * 1000);

                if (curlResult != 0) {
                    ALOG_WARN("!!", ctx->urlStr, resp.curlErrStr);
                }
                if (httpCode >= 400 && !resp.body.empty()) {
                    std::string snippet = resp.body.substr(0, 99);
                    ALOG_INFO("<<", httpCode, ctx->urlStr,
                        resp.elapsedMs, "ms", resp.body.size(), "bytes",
                        "| body:", snippet);
                } else {
                    ALOG_INFO("<<", httpCode, ctx->urlStr,
                        resp.elapsedMs, "ms", resp.body.size(), "bytes");
                }

                curl_multi_remove_handle(multi, msg->easy_handle);
                curl_easy_cleanup(msg->easy_handle);

                if (ctx->requestHeaders) {
                    curl_slist_free_all(ctx->requestHeaders);

                }

                ctx->done(resp, ctx->udata);
                delete ctx;
            }
        }
    }
    ALOG_INFO("EventPoller thread exited");
}
