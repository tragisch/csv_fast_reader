"""Root build file for csv_fast."""

load("//tools/install:def.bzl", "installer")

#   bazel run //:refresh_compile_commands

alias(
    name = "refresh_compile_commands",
    actual = "@wolfd_bazel_compile_commands//:generate_compile_commands",
)

installer(
    name = "install_csv_fast",
    data = [
        "//apps/csv_fast",
        "//apps/csv_fast:csv_fast_headers",
    ],
    executable = False,
)
