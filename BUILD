load("@rules_cc//cc:defs.bzl", "cc_proto_library")
load("@rules_proto//proto:defs.bzl", "proto_library")


proto_library(
    name = "enums_proto",
    srcs = ["enums.proto"]
)

cc_proto_library(
    name = "enums_cc",
    deps = [":enums_proto"],
)

cc_library(
    name = "homework",
    hdrs = ["homework.h"],
    deps = [
        "@com_google_absl//absl/strings:strings",  # Include str_split dependency
        "@com_google_absl//absl/strings:str_format",
        "@com_github_google_glog//:glog",
        ":enums_cc",
    ],
)


cc_test(
    name = "homework_test",
    srcs = ["homework_test.cpp"],
    deps = [
        ":homework",
        "@com_google_googletest//:gtest_main",
    ],
)

