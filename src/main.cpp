#include <iostream>
#include <atomic>
#include <csignal>

#include "coke/tools/option_parser.h"
#include "log/log.h"

#include "message_server.h"

std::atomic<bool> run_flag{true};

void sighandler(int) {
    run_flag.store(false, std::memory_order_relaxed);
    run_flag.notify_all();
}

int main(int argc, char *argv[]) {
    MessageServerParams params;
    int port = 8000;

    coke::OptionParser args;
    args.add_integer(port, 'p', "port").set_default(8000)
        .set_description("The http message queue server serve on this port");

    args.add_integer(params.dft_que_size, 0, "default-queue-size")
        .set_default(1024)
        .set_description("The default queue size of a new topic");

    args.add_integer(params.max_que_size, 0, "max-queue-size")
        .set_default(4096)
        .set_description("The max queue size of a new topic");

    args.add_integer(params.max_message_size, 0, "max-message-size")
        .set_default(4096)
        .set_description("The max size of each message");

    args.set_help_flag('h', "help");

    std::string err;
    int ret = args.parse(argc, argv, err);

    if (ret < 0) {
        std::cerr << err << std::endl;
        return 1;
    }
    else if (ret > 0) {
        args.usage(std::cout);
        return 0;
    }

    MessageServer server(params);

    signal(SIGTERM, sighandler);
    signal(SIGINT, sighandler);

    if (server.start(port) == 0) {
        LOG_INFO("ServerStart port:{}", port);

        run_flag.wait(true, std::memory_order_relaxed);

        LOG_INFO("ServerStop shutdown server");
        server.shutdown();

        LOG_INFO("ServerStop wait finish");
        server.wait_finish();

        LOG_INFO("ServerStop done");
        return 0;
    }
    else {
        LOG_INFO("ServerStartFailed port:{} errno:{}", port, (int)errno);
        return 1;
    }
}
