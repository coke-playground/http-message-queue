#include <cctype>

#include "message_server.h"

#include "log/log.h"
#include "workflow/HttpUtil.h"

void string_tolower(std::string &str) {
    for (char &c : str)
        c = std::tolower(c);
}

static void
split_request(const std::string &uri, std::string &path,
              std::unordered_map<std::string, std::string> &query)
{
    std::size_t p1 = uri.find('?'), p2, p3;
    auto npos = std::string::npos;

    path = uri.substr(0, p1);
    string_tolower(path);

    while (p1 != npos) {
        ++p1;
        p3 = uri.find('&', p1);
        p2 = uri.find('=', p1);

        if (p2 > p3)
            p2 = p3;

        std::string k = uri.substr(p1, p2 - p1);
        std::string v;

        if (p2 != p3)
            v = uri.substr(p2+1, p3-p2-1);

        string_tolower(k);
        query.emplace(std::move(k), std::move(v));

        p1 = p3;
    }
}

static std::size_t
get_size(const std::unordered_map<std::string, std::string> &m,
         const std::string &key, std::size_t dft)
{
    auto it = m.find(key);
    if (it == m.end())
        return dft;

    return (std::size_t)std::atoll(it->second.c_str());
}

static std::string
get_string(const std::unordered_map<std::string, std::string> &m,
           const std::string &key, std::string dft)
{
    auto it = m.find(key);
    if (it == m.end())
        return dft;

    return it->second;
}

void MessageServer::parse_request(const coke::HttpRequest &req,
                                  RequestParams &p)
{
    std::string uri;
    if (!req.get_request_uri(uri)) {
        p.method = RequestMethod::UNKNOWN;
        return;
    }

    std::string path;
    std::unordered_map<std::string, std::string> query;

    split_request(uri, path, query);

    if (path == "/create") {
        p.method = RequestMethod::CREATE;
        p.que_size = get_size(query, "que_size", params.dft_que_size);
    }
    else if (path == "/put") {
        p.method = RequestMethod::PUT;
    }
    else if (path == "/get") {
        p.method = RequestMethod::GET;
        p.offset = get_size(query, "offset", 0);
        p.timeout = get_size(query, "timeout", params.dft_get_timeout);
        p.max = get_size(query, "max", 1);
    }
    else if (path == "/delete") {
        p.method = RequestMethod::DELETE;
    }
    else {
        p.method = RequestMethod::UNKNOWN;
        return;
    }

    p.topic = get_string(query, "topic", "");
}

MessageServer::HandlerPtr
MessageServer::get_handler(const std::string &topic, bool remove) {
    HandlerPtr ptr;

    if (remove) {
        std::unique_lock<std::shared_mutex> lk(smtx);

        auto it = topics.find(topic);
        if (it != topics.end()) {
            ptr = std::move(it->second);
            topics.erase(it);
        }
    }
    else {
        std::shared_lock<std::shared_mutex> lk(smtx);

        auto it = topics.find(topic);
        if (it != topics.end())
            ptr = it->second;
    }

    return ptr;
}

coke::Task<> MessageServer::process(coke::HttpServerContext ctx) {
    coke::HttpRequest &req = ctx.get_req();
    coke::HttpResponse &resp = ctx.get_resp();
    RequestParams req_params;
    coke::Task<> (MessageServer::*func)(coke::HttpServerContext &, RequestParams &) = nullptr;

    parse_request(req, req_params);

    switch (req_params.method) {
    case RequestMethod::CREATE: func = &MessageServer::process_create; break;
    case RequestMethod::PUT: func = &MessageServer::process_put; break;
    case RequestMethod::GET: func = &MessageServer::process_get; break;
    case RequestMethod::DELETE: func = &MessageServer::process_delete; break;
    default: break;
    }

    if (func) {
        co_await (this->*func)(ctx, req_params);
    }
    else {
        protocol::HttpUtil::set_response_status(&resp, 400);
        resp.append_output_body("Unknown request method");
    }

    co_await ctx.reply();
}

coke::Task<>
MessageServer::process_create(coke::HttpServerContext &ctx, RequestParams &p) {
    coke::HttpResponse &resp = ctx.get_resp();
    bool success = false;
    std::string error;

    if (p.topic.empty()) {
        error = "Invalid topic name";
    }
    else if (p.que_size > params.max_que_size) {
        error = "Queue size exceeds limit";
    }
    else {
        std::lock_guard<std::shared_mutex> lg(smtx);

        auto it = topics.find(p.topic);
        if (it == topics.end()) {
            auto ptr = std::make_shared<MessageHandler>(p.topic, p.que_size);

            topics.emplace(p.topic, ptr);
            success = true;
        }
        else {
            error = "Topic already exists";
        }
    }

    if (success) {
        protocol::HttpUtil::set_response_status(&resp, 200);

        LOG_INFO("ProcessCreate 200 topic:{} que_size:{}", p.topic, p.que_size);
    }
    else {
        protocol::HttpUtil::set_response_status(&resp, 400);
        resp.append_output_body(error);

        LOG_WARN("ProcessCreate 400 topic:{} err:{}", p.topic, error);
    }

    co_return;
}

coke::Task<>
MessageServer::process_put(coke::HttpServerContext &ctx, RequestParams &p) {
    coke::HttpRequest &req = ctx.get_req();
    coke::HttpResponse &resp = ctx.get_resp();
    bool success = false;
    std::string error;
    HandlerPtr ptr = get_handler(p.topic, false);

    if (!ptr) {
        error = "No such topic";
    }
    else {
        std::string data = protocol::HttpUtil::decode_chunked_body(&req);

        if (data.size() > params.max_message_size) {
            error = "Message too large";
        }
        else {
            ptr->put(std::move(data));
            success = true;
        }
    }

    if (success) {
        protocol::HttpUtil::set_response_status(&resp, 200);

        LOG_INFO("ProcessPut 200 topic:{}", p.topic);
    }
    else {
        protocol::HttpUtil::set_response_status(&resp, 400);
        resp.append_output_body(error);

        LOG_WARN("ProcessPut 400 topic:{} err:{}", p.topic, error);
    }

    co_return;
}

coke::Task<>
MessageServer::process_get(coke::HttpServerContext &ctx, RequestParams &p) {
    coke::HttpResponse &resp = ctx.get_resp();
    bool success = false;
    std::string error;
    std::vector<Message> msgs;
    HandlerPtr ptr = get_handler(p.topic, false);

    if (!ptr) {
        error = "No such topic";
    }
    else {
        if (!ptr->try_get(p.offset, p.max, msgs)) {
            std::size_t timeout = std::min(params.max_get_timeout, p.timeout);
            auto ms = std::chrono::milliseconds(timeout);
            co_await ptr->get(p.offset, p.max, ms, msgs);
        }

        success = true;
    }

    if (success) {
        std::string data;
        protocol::HttpUtil::set_response_status(&resp, 200);

        data.append(std::to_string(msgs.size())).append("\r\n");

        for (auto &msg : msgs) {
            data.append(std::to_string(msg.offset)).append("\r\n")
                .append(std::to_string(msg.data.length())).append("\r\n")
                .append(msg.data).append("\r\n");
        }

        resp.append_output_body(data);

        std::size_t first_off = 0;
        if (!msgs.empty())
            first_off = msgs[0].offset;

        LOG_INFO("ProcessGet 200 topic:{} offset:{} msgs:{} timeout:{}",
                 p.topic, first_off, msgs.size(), p.timeout);
    }
    else {
        protocol::HttpUtil::set_response_status(&resp, 400);
        resp.append_output_body(error);

        LOG_WARN("ProcessGet 400 topic:{} err:{}", p.topic, error);
    }

    co_return;
}

coke::Task<>
MessageServer::process_delete(coke::HttpServerContext &ctx, RequestParams &p) {
    coke::HttpResponse &resp = ctx.get_resp();
    HandlerPtr ptr = get_handler(p.topic, true);

    if (!ptr) {
        std::string error = "No such topic";

        protocol::HttpUtil::set_response_status(&resp, 400);
        resp.append_output_body(error);

        LOG_WARN("ProcessDelete 400 topic:{} err:{}", p.topic, error);
    }
    else {
        protocol::HttpUtil::set_response_status(&resp, 200);

        LOG_INFO("ProcessDelete 200 topic:{}", p.topic);
    }

    co_return;
}
