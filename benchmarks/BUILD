load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_java//java:defs.bzl", "java_binary", "java_import")
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

cc_binary(
    name = "csv_parser_benchmark",
    srcs = ["csv_parser_benchmark.cc"],
    data = [
        "//data:cbp23co.csv",
        "//data:star2002-full.csv",
        "//data:zbp23detail.csv",
    ],
    deps = ["//third_party/csv-parser"],
)

cc_binary(
    name = "libcsv_benchmark",
    srcs = ["libcsv_benchmark.c"],
    copts = C_COPTS,
    data = [
        "//data:cbp23co.csv",
        "//data:star2002-full.csv",
        "//data:zbp23detail.csv",
    ],
    deps = ["@libcsv"],
)

cc_binary(
    name = "rapidcsv_benchmark",
    srcs = ["rapidcsv_benchmark.cc"],
    data = [
        "//data:cbp23co.csv",
        "//data:star2002-full.csv",
        "//data:zbp23detail.csv",
    ],
    deps = ["@rapidcsv"],
)

java_import(
    name = "fastcsv",
    jars = ["@fastcsv_jar//file"],
)

java_binary(
    name = "fastcsv_benchmark",
    srcs = ["FastCsvBenchmark.java"],
    data = [
        "//data:cbp23co.csv",
        "//data:star2002-full.csv",
        "//data:zbp23detail.csv",
    ],
    main_class = "FastCsvBenchmark",
    deps = [":fastcsv"],
)
