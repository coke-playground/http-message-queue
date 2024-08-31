#ifndef MESSAGE_SERVER_H
#define MESSAGE_SERVER_H

#include <unordered_map>
#include <shared_mutex>

#include "message_handler.h"
#include "coke/http_server.h"

struct MessageServerParams {
    coke::HttpServerParams http_params;

    std::size_t dft_que_size{1024};
    std::size_t dft_get_timeout{1000};

    std::size_t max_que_size{4096};
    std::size_t max_message_size{4096};
    std::size_t max_get_timeout{5000};
};

class MessageServer : public coke::HttpServer {
    static auto get_proc(MessageServer *server) {
        return [server](coke::HttpServerContext ctx) {
            return server->process(std::move(ctx));
        };
    }

public:
    MessageServer(MessageServerParams params)
        : coke::HttpServer(params.http_params, get_proc(this)),
          params(params)
    { }

private:
    enum RequestMethod {
        UNKNOWN = -1,
        CREATE = 0,
        PUT = 1,
        GET = 2,
        DELETE = 3,
    };

    struct RequestParams {
        RequestMethod method;
        std::string topic;
        std::size_t offset{0};
        std::size_t max{1};
        std::size_t que_size{1024};
        std::size_t timeout{1000};
    };

    using HandlerPtr = std::shared_ptr<MessageHandler>;
    using UMapIter = std::unordered_map<std::string, HandlerPtr>::iterator;

    void parse_request(const coke::HttpRequest &req, RequestParams &p);

    HandlerPtr get_handler(const std::string &topic, bool remove);

    coke::Task<> process(coke::HttpServerContext ctx);
    coke::Task<> process_create(coke::HttpServerContext &ctx, RequestParams &p);
    coke::Task<> process_put(coke::HttpServerContext &ctx, RequestParams &p);
    coke::Task<> process_get(coke::HttpServerContext &ctx, RequestParams &p);
    coke::Task<> process_delete(coke::HttpServerContext &ctx, RequestParams &p);

private:
    MessageServerParams params;

    std::shared_mutex smtx;
    std::unordered_map<std::string, HandlerPtr> topics;
};

#endif // MESSAGE_SERVER_H
