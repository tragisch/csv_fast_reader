"""Format targets for the workspace."""

load("@aspect_rules_lint//format:defs.bzl", "format_multirun")
load("@bazel_skylib//rules:native_binary.bzl", "native_binary")

native_binary(
    name = "clang-format",
    src = "@llvm_toolchain_llvm//:bin/clang-format",
    out = "clang-format",
)

native_binary(
    name = "buildifier",
    src = "@buildifier_prebuilt//:buildifier",
    out = "buildifier",
)

format_multirun(
    name = "format",
    cc = ":clang-format",
    starlark = ":buildifier",
)
