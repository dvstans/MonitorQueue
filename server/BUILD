cc_binary(
    name = "mqserver",
    srcs = glob(["libjson.hpp","Queue.hpp","Queue.cpp","QueueServer.hpp","QueueServer.cpp","mqserver.cpp"]),
    includes = ["."],
    linkopts = ["-lpthread","-lPocoFoundation","-lPocoNet"],
    visibility = ["//visibility:public"]
)

cc_binary(
    name = "mqclient",
    srcs = glob(["libjson.hpp","mqclient.cpp"]),
    includes = ["."],
    linkopts = ["-lpthread","-lPocoFoundation","-lPocoNet"],
    visibility = ["//visibility:public"]
)

cc_test(
    name = "test_general",
    size = "small",
    tags = ["unit"],
    srcs = ["Queue.hpp","Queue.cpp","test_general.cpp"],
    linkopts = ["-lpthread"]
)

cc_test(
    name = "test_delay",
    size = "small",
    tags = ["unit"],
    srcs = ["Queue.hpp","Queue.cpp","test_delay.cpp"],
    linkopts = ["-lpthread"]
)

