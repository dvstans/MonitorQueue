#cc_binary(
#    name = "monitorqueue",
#    srcs = glob(["*.cpp","*.hpp"]),
#    linkopts = ["-lpthread"],
#    visibility = ["//visibility:public"]
#)

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
