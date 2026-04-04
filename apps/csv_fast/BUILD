load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//tools:c_copts.bzl", "C_COPTS")

cc_library(
    name = "csv_fast",
    srcs = ["csv.c"],
    hdrs = ["csv.h"],
    copts = C_COPTS,
    includes = ["."],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "csv_fast_headers",
    srcs = ["csv.h"],
    visibility = ["//visibility:public"],
)
