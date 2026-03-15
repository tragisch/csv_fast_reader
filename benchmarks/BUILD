load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("//tools:c_copts.bzl", "C_COPTS")

cc_binary(
    name = "csv_benchmark",
    srcs = ["csv_benchmark.c"],
    copts = C_COPTS,
    data = [
        "//data:cbp23co.csv",
        "//data:star2002-full.csv",
        "//data:zbp23detail.csv",
    ],
    deps = ["//apps/csv_fast"],
)
