# Benchmarks

Benchmark comparison on the bundled real-world datasets.

## Test machine

- Model: MacBook Air (`Mac15,12`)
- Chip: Apple M3
- Memory: 24 GB
- OS: macOS 26.3.1
- Date: 2026-04-03

## Method

All benchmark binaries were executed with Bazel release settings:

```bash
bazel run --config=opt //benchmarks:csv_benchmark -- --suite real
bazel run --config=opt //benchmarks:csv_parser_benchmark -- --suite real
bazel run --config=opt //benchmarks:libcsv_benchmark -- --suite real
bazel run --config=opt //benchmarks:rapidcsv_benchmark -- --suite real
bazel run --config=opt //benchmarks:fastcsv_benchmark -- --suite real
```

Each result below shows average throughput in MiB/s over 5 measured iterations after 1 warmup run.

## Results

| Dataset         |     csv_fast |   csv_parser |       libcsv |     rapidcsv |      FastCSV |
| --------------- | -----------: | -----------: | -----------: | -----------: | -----------: |
| `cbp23co`       | 449.29 MiB/s | 311.30 MiB/s | 303.99 MiB/s | 155.09 MiB/s | 291.93 MiB/s |
| `zbp23detail`   | 521.30 MiB/s | 401.79 MiB/s | 311.49 MiB/s | 159.32 MiB/s | 333.05 MiB/s |
| `star2002-full` | 688.56 MiB/s | 636.32 MiB/s | 294.53 MiB/s | 234.68 MiB/s | 416.96 MiB/s |

