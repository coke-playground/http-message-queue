package(default_visibility = ["//visibility:public"])

cc_library(
    name = "message_server",
    srcs = ["src/message_server.cpp"],
    hdrs = [
        "include/message_handler.h",
        "include/message_server.h"
    ],
    includes = ["include"],
    deps = [
        "@common//:log",
        "@coke//:http",
    ]
)

cc_binary(
    name = "http_message_queue",
    srcs = ["src/main.cpp"],
    deps = [
        "//:message_server",
        "@common//:log",
        "@coke//:tools",
    ]
)
