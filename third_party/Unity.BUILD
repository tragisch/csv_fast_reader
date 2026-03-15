load("@rules_cc//cc:cc_library.bzl", "cc_library")

filegroup(
    name = "TestRunnerGenerator",
    srcs = ["auto/generate_test_runner.rb"],
    
    visibility = ["//visibility:public"],
)

filegroup(
    name = "HelperScripts",
    srcs = glob(["auto/*.rb"]),
    visibility = ["//visibility:public"],
)

cc_library(
    name = "Unity",
    srcs = [
        "src/unity.c",
    ],
    hdrs = [
        "src/unity.h",
        "src/unity_internals.h",
    ],
    copts = [
        "-DUNITY_INCLUDE_DOUBLE",
    ],
    defines = ["UNITY_SUPPORT_64"],
    includes = ["src"],
    visibility = ["//visibility:public"],
)
