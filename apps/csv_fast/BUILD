load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("//tools:c_copts.bzl", "C_COPTS")

config_setting(
    name = "simd_experiment_enabled",
    define_values = {"csv_fast_simd_experiment": "1"},
)

cc_library(
    name = "csv_fast",
    srcs = ["csv.c"],
    hdrs = ["csv.h"],
    copts = C_COPTS + select({
        ":simd_experiment_enabled": ["-DCSV_ENABLE_SIMD_EXPERIMENT=1"],
        "//conditions:default": [],
    }),
    includes = ["."],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "csv_fast_headers",
    srcs = ["csv.h"],
    visibility = ["//visibility:public"],
)
