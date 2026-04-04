platform(
    name = "linux_x86_64_platform",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
    visibility = ["//visibility:public"],
)

platform(
    name = "macos_arm64_platform",
    constraint_values = [
        "@platforms//os:macos",
        "@platforms//cpu:arm64",
    ],
    visibility = ["//visibility:public"],
)

platform(
    name = "windows_x86_64_platform",
    constraint_values = [
        "@platforms//os:windows",
        "@platforms//cpu:x86_64",
    ],
    visibility = ["//visibility:public"],
)

constraint_setting(name = "gpu_type")

constraint_value(
    name = "cuda",
    constraint_setting = ":gpu_type",
)

platform(
    name = "cuda_linux_x86_64_platform",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
        ":cuda",
    ],
    visibility = ["//visibility:public"],
)
