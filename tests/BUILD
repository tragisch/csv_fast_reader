"""Unity tests for csv_fast."""

load("//tools:c_copts.bzl", "C_COPTS")
load("//tools:unity_test.bzl", "unity_test")

unity_test(
    name = "test_csv",
    srcs = ["test_csv.c"],
    copts = C_COPTS,
    data = glob(["fixtures/csv_spectrum/*.csv"]),
    deps = ["//apps/csv_fast"],
)
