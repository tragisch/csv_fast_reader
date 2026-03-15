/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2025-2026 @tragisch <https://github.com/tragisch>
 *
 * This file contains substantial modifications of the original MIT-licensed
 * work. See the LICENSE file in the project root for license details.
 */

#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../apps/csv_fast/csv.h"

typedef enum BenchmarkWorkload {
    BENCHMARK_WORKLOAD_SIMPLE = 0,
    BENCHMARK_WORKLOAD_QUOTED,
    BENCHMARK_WORKLOAD_MULTILINE,
    BENCHMARK_WORKLOAD_CRLF,
} BenchmarkWorkload;

typedef enum BenchmarkSuite {
    BENCHMARK_SUITE_NONE = 0,
    BENCHMARK_SUITE_SYNTHETIC,
    BENCHMARK_SUITE_REAL,
    BENCHMARK_SUITE_ALL,
} BenchmarkSuite;

typedef struct BenchmarkOptions {
    BenchmarkSuite suite;
    BenchmarkWorkload workload;
    const char* workloadName;
    const char* inputPath;
    size_t rows;
    size_t warmupIterations;
    size_t iterations;
} BenchmarkOptions;

typedef struct BenchmarkCase {
    const char* label;
    const char* inputPath;
    BenchmarkWorkload workload;
    size_t rows;
} BenchmarkCase;

typedef struct BenchmarkInput {
    char path[1024];
    size_t bytes;
    size_t rows;
    int isTemporary;
} BenchmarkInput;

typedef struct BenchmarkResult {
    size_t rows;
    size_t fields;
    size_t bytes;
    double seconds;
} BenchmarkResult;

typedef struct BenchmarkSummary {
    BenchmarkResult best;
    BenchmarkResult worst;
    size_t measuredIterations;
    double totalSeconds;
} BenchmarkSummary;

static const BenchmarkCase kSyntheticCases[] = {
    {"simple", NULL, BENCHMARK_WORKLOAD_SIMPLE, 200000U},
    {"quoted", NULL, BENCHMARK_WORKLOAD_QUOTED, 200000U},
    {"multiline", NULL, BENCHMARK_WORKLOAD_MULTILINE, 200000U},
    {"crlf", NULL, BENCHMARK_WORKLOAD_CRLF, 200000U},
};

static const BenchmarkCase kRealCases[] = {
    {"cbp23co", "data/cbp23co.csv", BENCHMARK_WORKLOAD_SIMPLE, 0U},
    {"zbp23detail", "data/zbp23detail.csv", BENCHMARK_WORKLOAD_SIMPLE, 0U},
    {"star2002-full", "data/star2002-full.csv", BENCHMARK_WORKLOAD_SIMPLE, 0U},
};

static void benchmark_print_usage(const char* argv0)
{
    (void)fprintf(stderr,
                  "Usage: %s [--workload simple|quoted|multiline|crlf]"
                  " [--suite synthetic|real|all]"
                  " [--input PATH]"
                  " [--rows N] [--warmup N] [--iterations N]\n",
                  argv0);
}

static int benchmark_parse_size(const char* text, size_t* out_value)
{
    char* end = NULL;
    unsigned long long value = 0ULL;

    if (!text || !out_value || *text == '\0')
        return -1;

    errno = 0;
    value = strtoull(text, &end, 10);
    if (errno != 0 || !end || *end != '\0')
        return -1;

    *out_value = (size_t)value;
    return 0;
}

static int benchmark_parse_workload(const char* text,
                                    BenchmarkWorkload* out_workload)
{
    if (!text || !out_workload)
        return -1;

    if (strcmp(text, "simple") == 0)
        *out_workload = BENCHMARK_WORKLOAD_SIMPLE;
    else if (strcmp(text, "quoted") == 0)
        *out_workload = BENCHMARK_WORKLOAD_QUOTED;
    else if (strcmp(text, "multiline") == 0)
        *out_workload = BENCHMARK_WORKLOAD_MULTILINE;
    else if (strcmp(text, "crlf") == 0)
        *out_workload = BENCHMARK_WORKLOAD_CRLF;
    else
        return -1;

    return 0;
}

static int benchmark_parse_suite(const char* text, BenchmarkSuite* out_suite)
{
    if (!text || !out_suite)
        return -1;

    if (strcmp(text, "synthetic") == 0)
        *out_suite = BENCHMARK_SUITE_SYNTHETIC;
    else if (strcmp(text, "real") == 0)
        *out_suite = BENCHMARK_SUITE_REAL;
    else if (strcmp(text, "all") == 0)
        *out_suite = BENCHMARK_SUITE_ALL;
    else
        return -1;

    return 0;
}

static const char* benchmark_workload_name(BenchmarkWorkload workload)
{
    switch (workload)
    {
        case BENCHMARK_WORKLOAD_SIMPLE:
            return "simple";
        case BENCHMARK_WORKLOAD_QUOTED:
            return "quoted";
        case BENCHMARK_WORKLOAD_MULTILINE:
            return "multiline";
        case BENCHMARK_WORKLOAD_CRLF:
            return "crlf";
    }

    return "unknown";
}

static const char* benchmark_row_pattern(BenchmarkWorkload workload)
{
    switch (workload)
    {
        case BENCHMARK_WORKLOAD_SIMPLE:
            return "alpha,beta,gamma,delta\n";
        case BENCHMARK_WORKLOAD_QUOTED:
            return "\"alpha\",\"beta,gamma\",\"delta\",\"omega\"\n";
        case BENCHMARK_WORKLOAD_MULTILINE:
            return "alpha,\"beta\nline\",delta,omega\n";
        case BENCHMARK_WORKLOAD_CRLF:
            return "alpha,beta,gamma,delta\r\n";
    }

    return "";
}

static double benchmark_throughput_mib_per_second(size_t bytes, double seconds)
{
    if (seconds <= 0.0)
        return 0.0;

    return ((double)bytes / (1024.0 * 1024.0)) / seconds;
}

static double benchmark_rate_per_second(size_t items, double seconds)
{
    if (seconds <= 0.0)
        return 0.0;

    return (double)items / seconds;
}

static int benchmark_copy_path(const char* source,
                               char* out_path,
                               size_t out_path_size)
{
    int written = 0;

    if (!source || !out_path || out_path_size == 0U)
        return -1;

    written = snprintf(out_path, out_path_size, "%s", source);
    if (written < 0 || (size_t)written >= out_path_size)
        return -1;

    return 0;
}

static int benchmark_join_path(const char* prefix,
                               const char* suffix,
                               char* out_path,
                               size_t out_path_size)
{
    int written = 0;

    if (!prefix || !suffix || !out_path || out_path_size == 0U)
        return -1;

    written = snprintf(out_path, out_path_size, "%s/%s", prefix, suffix);
    if (written < 0 || (size_t)written >= out_path_size)
        return -1;

    return 0;
}

static int benchmark_path_exists(const char* path, struct stat* st)
{
    if (!path || !st)
        return 0;

    return stat(path, st) == 0;
}

static int benchmark_resolve_input_path(const char* input_path,
                                        char* out_path,
                                        size_t out_path_size,
                                        struct stat* st)
{
    const char* workingDirectory = getenv("BUILD_WORKING_DIRECTORY");
    const char* runfilesDirectory = getenv("RUNFILES_DIR");
    const char* workspaceDirectory = getenv("BUILD_WORKSPACE_DIRECTORY");
    const char* testSrcDir = getenv("TEST_SRCDIR");
    const char* testWorkspace = getenv("TEST_WORKSPACE");
    char cwd[1024] = {0};
    char candidate[1024] = {0};
    char runfilesRoot[1024] = {0};

    if (!input_path || !out_path || out_path_size == 0U || !st)
        return -1;

    if (benchmark_copy_path(input_path, candidate, sizeof(candidate)) == 0 &&
        benchmark_path_exists(candidate, st))
        return benchmark_copy_path(candidate, out_path, out_path_size);

    if (runfilesDirectory && runfilesDirectory[0] != '\0' &&
        benchmark_join_path(runfilesDirectory,
                            input_path,
                            candidate,
                            sizeof(candidate)) == 0 &&
        benchmark_path_exists(candidate, st))
        return benchmark_copy_path(candidate, out_path, out_path_size);

    if (runfilesDirectory && runfilesDirectory[0] != '\0' &&
        benchmark_join_path(runfilesDirectory,
                            "_main",
                            runfilesRoot,
                            sizeof(runfilesRoot)) == 0 &&
        benchmark_join_path(runfilesRoot,
                            input_path,
                            candidate,
                            sizeof(candidate)) == 0 &&
        benchmark_path_exists(candidate, st))
        return benchmark_copy_path(candidate, out_path, out_path_size);

    if (testSrcDir && testSrcDir[0] != '\0' &&
        testWorkspace && testWorkspace[0] != '\0' &&
        benchmark_join_path(testSrcDir,
                            testWorkspace,
                            runfilesRoot,
                            sizeof(runfilesRoot)) == 0 &&
        benchmark_join_path(runfilesRoot,
                            input_path,
                            candidate,
                            sizeof(candidate)) == 0 &&
        benchmark_path_exists(candidate, st))
        return benchmark_copy_path(candidate, out_path, out_path_size);

    if (workingDirectory && workingDirectory[0] != '\0' &&
        benchmark_join_path(workingDirectory,
                            input_path,
                            candidate,
                            sizeof(candidate)) == 0 &&
        benchmark_path_exists(candidate, st))
        return benchmark_copy_path(candidate, out_path, out_path_size);

    if (workspaceDirectory && workspaceDirectory[0] != '\0' &&
        benchmark_join_path(workspaceDirectory,
                            input_path,
                            candidate,
                            sizeof(candidate)) == 0 &&
        benchmark_path_exists(candidate, st))
        return benchmark_copy_path(candidate, out_path, out_path_size);

    if (getcwd(cwd, sizeof(cwd)) != NULL &&
        benchmark_join_path(cwd, input_path, candidate, sizeof(candidate)) == 0 &&
        benchmark_path_exists(candidate, st))
        return benchmark_copy_path(candidate, out_path, out_path_size);

    return -1;
}

static int benchmark_parse_args(int argc,
                                char** argv,
                                BenchmarkOptions* options)
{
    int i = 0;

    if (!options)
        return -1;

    options->suite = BENCHMARK_SUITE_NONE;
    options->workload = BENCHMARK_WORKLOAD_SIMPLE;
    options->workloadName = benchmark_workload_name(options->workload);
    options->inputPath = NULL;
    options->rows = 200000U;
    options->warmupIterations = 1U;
    options->iterations = 5U;

    for (i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--suite") == 0)
        {
            if (i + 1 >= argc || benchmark_parse_suite(argv[i + 1], &options->suite) != 0)
                return -1;

            i++;
        }
        else if (strcmp(argv[i], "--workload") == 0)
        {
            if (i + 1 >= argc || benchmark_parse_workload(argv[i + 1], &options->workload) != 0)
                return -1;

            i++;
        }
        else if (strcmp(argv[i], "--input") == 0)
        {
            if (i + 1 >= argc || argv[i + 1][0] == '\0')
                return -1;

            options->inputPath = argv[i + 1];
            i++;
        }
        else if (strcmp(argv[i], "--rows") == 0)
        {
            if (i + 1 >= argc || benchmark_parse_size(argv[i + 1], &options->rows) != 0)
                return -1;

            i++;
        }
        else if (strcmp(argv[i], "--warmup") == 0)
        {
            if (i + 1 >= argc || benchmark_parse_size(argv[i + 1], &options->warmupIterations) != 0)
                return -1;

            i++;
        }
        else if (strcmp(argv[i], "--iterations") == 0)
        {
            if (i + 1 >= argc || benchmark_parse_size(argv[i + 1], &options->iterations) != 0)
                return -1;

            i++;
        }
        else if (strcmp(argv[i], "--help") == 0)
        {
            return 1;
        }
        else
        {
            return -1;
        }
    }

    options->workloadName = benchmark_workload_name(options->workload);
    if (((options->suite == BENCHMARK_SUITE_NONE && options->inputPath == NULL && options->rows == 0U) ||
         options->iterations == 0U))
        return -1;

    return 0;
}

static BenchmarkCase benchmark_case_from_options(const BenchmarkOptions* options)
{
    BenchmarkCase benchmarkCase = {0};

    benchmarkCase.label = options->inputPath ? options->inputPath : options->workloadName;
    benchmarkCase.inputPath = options->inputPath;
    benchmarkCase.workload = options->workload;
    benchmarkCase.rows = options->rows;
    return benchmarkCase;
}

static int benchmark_use_existing_input(const BenchmarkCase* benchmarkCase,
                                        BenchmarkInput* input)
{
    struct stat st = {0};

    if (!benchmarkCase || !input || !benchmarkCase->inputPath)
        return -1;

    if (benchmark_resolve_input_path(benchmarkCase->inputPath,
                                     input->path,
                                     sizeof(input->path),
                                     &st) != 0)
        return -1;

    input->bytes = (size_t)st.st_size;
    input->rows = 0U;
    input->isTemporary = 0;
    return 0;
}

static int benchmark_write_generated_input(const BenchmarkCase* benchmarkCase,
                                           BenchmarkInput* input)
{
    const char* rowPattern = NULL;
    FILE* file = NULL;
    int fd = -1;
    size_t rowIndex = 0U;
    size_t rowBytes = 0U;

    if (!benchmarkCase || !input)
        return -1;

    rowPattern = benchmark_row_pattern(benchmarkCase->workload);
    rowBytes = strlen(rowPattern);
    if (rowBytes == 0U)
        return -1;

    (void)snprintf(input->path, sizeof(input->path), "/tmp/csv_fast_bench_XXXXXX");
    fd = mkstemp(input->path);
    if (fd < 0)
        return -1;

    input->isTemporary = 1;

    file = fdopen(fd, "wb");
    if (!file)
    {
        (void)close(fd);
        (void)unlink(input->path);
        return -1;
    }

    for (rowIndex = 0U; rowIndex < benchmarkCase->rows; ++rowIndex)
    {
        if (fwrite(rowPattern, 1U, rowBytes, file) != rowBytes)
        {
            (void)fclose(file);
            (void)unlink(input->path);
            return -1;
        }
    }

    if (fclose(file) != 0)
    {
        (void)unlink(input->path);
        return -1;
    }

    input->bytes = rowBytes * benchmarkCase->rows;
    input->rows = benchmarkCase->rows;
    return 0;
}

static int benchmark_prepare_input(const BenchmarkCase* benchmarkCase,
                                   BenchmarkInput* input)
{
    memset(input, 0, sizeof(*input));

    if (benchmarkCase->inputPath)
        return benchmark_use_existing_input(benchmarkCase, input);

    return benchmark_write_generated_input(benchmarkCase, input);
}

static void benchmark_cleanup_input(BenchmarkInput* input)
{
    if (!input)
        return;

    if (input->isTemporary && input->path[0] != '\0')
        (void)unlink(input->path);
}

static double benchmark_elapsed_seconds(const struct timespec* start,
                                        const struct timespec* end)
{
    double seconds = 0.0;
    long nanoseconds = 0L;

    seconds = (double)(end->tv_sec - start->tv_sec);
    nanoseconds = end->tv_nsec - start->tv_nsec;
    return seconds + ((double)nanoseconds / 1000000000.0);
}

static int benchmark_run_once(const BenchmarkInput* input,
                              BenchmarkResult* result)
{
    struct timespec start = {0};
    struct timespec end = {0};
    CsvReader* reader = NULL;
    CsvStringView row = {0};
    CsvStringView field = {0};
    CsvStatus status = CSV_STATUS_OK;
    size_t rowsRead = 0U;
    size_t fieldsRead = 0U;

    if (!input || !result)
        return -1;

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0)
        return -1;

    status = csv_reader_open(&reader, input->path);
    if (status != CSV_STATUS_OK)
        return -1;

    while ((status = csv_reader_next_row(reader, &row)) == CSV_STATUS_OK)
    {
        const CsvStringView* currentRow = &row;

        rowsRead++;

        while ((status = csv_reader_next_col(reader, currentRow, &field)) == CSV_STATUS_OK)
        {
            fieldsRead++;
            currentRow = NULL;
        }

        if (status != CSV_STATUS_EOF)
        {
            csv_reader_close(reader);
            return -1;
        }
    }

    csv_reader_close(reader);
    if (status != CSV_STATUS_EOF)
        return -1;

    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0)
        return -1;

    result->rows = rowsRead;
    result->fields = fieldsRead;
    result->bytes = input->bytes;
    result->seconds = benchmark_elapsed_seconds(&start, &end);
    return 0;
}

static void benchmark_summary_add(BenchmarkSummary* summary,
                                  const BenchmarkResult* result)
{
    if (!summary || !result)
        return;

    if (summary->measuredIterations == 0U || result->seconds < summary->best.seconds)
        summary->best = *result;

    if (summary->measuredIterations == 0U || result->seconds > summary->worst.seconds)
        summary->worst = *result;

    summary->measuredIterations++;
    summary->totalSeconds += result->seconds;
}

static void benchmark_print_case_summary(const BenchmarkCase* benchmarkCase,
                                         const BenchmarkInput* input,
                                         const BenchmarkOptions* options,
                                         const BenchmarkSummary* summary)
{
    double averageSeconds = 0.0;
    double averageThroughput = 0.0;
    double bestThroughput = 0.0;
    double worstThroughput = 0.0;
    double averageRowsPerSecond = 0.0;
    double averageFieldsPerSecond = 0.0;

    if (!benchmarkCase || !input || !options || !summary || summary->measuredIterations == 0U)
        return;

    averageSeconds = summary->totalSeconds / (double)summary->measuredIterations;
    averageThroughput = benchmark_throughput_mib_per_second(summary->best.bytes, averageSeconds);
    bestThroughput = benchmark_throughput_mib_per_second(summary->best.bytes, summary->best.seconds);
    worstThroughput = benchmark_throughput_mib_per_second(summary->worst.bytes, summary->worst.seconds);
    averageRowsPerSecond = benchmark_rate_per_second(summary->best.rows, averageSeconds);
    averageFieldsPerSecond = benchmark_rate_per_second(summary->best.fields, averageSeconds);

    if (benchmarkCase->inputPath)
    {
        (void)printf("case=%s input=%s warmup=%zu iterations=%zu bytes=%zu rows=%zu\n",
                     benchmarkCase->label,
                     input->path,
                     options->warmupIterations,
                     summary->measuredIterations,
                     summary->best.bytes,
                     summary->best.rows);
    }
    else
    {
        (void)printf("case=%s workload=%s rows=%zu warmup=%zu iterations=%zu bytes=%zu\n",
                     benchmarkCase->label,
                     benchmark_workload_name(benchmarkCase->workload),
                     benchmarkCase->rows,
                     options->warmupIterations,
                     summary->measuredIterations,
                     summary->best.bytes);
    }

    (void)printf("avg=%.6f s  best=%.6f s  worst=%.6f s\n",
                 averageSeconds,
                 summary->best.seconds,
                 summary->worst.seconds);
    (void)printf("avg_throughput=%.2f MiB/s  best_throughput=%.2f MiB/s  worst_throughput=%.2f MiB/s\n",
                 averageThroughput,
                 bestThroughput,
                 worstThroughput);
    (void)printf("avg_rows/s=%.0f  avg_fields/s=%.0f\n",
                 averageRowsPerSecond,
                 averageFieldsPerSecond);
}

static int benchmark_run_case(const BenchmarkCase* benchmarkCase,
                              const BenchmarkOptions* options)
{
    BenchmarkInput input = {0};
    BenchmarkSummary summary = {0};
    size_t totalRuns = 0U;
    size_t runIndex = 0U;

    if (!benchmarkCase || !options)
        return -1;

    if (benchmark_prepare_input(benchmarkCase, &input) != 0)
        return -1;

    totalRuns = options->warmupIterations + options->iterations;
    for (runIndex = 0U; runIndex < totalRuns; ++runIndex)
    {
        BenchmarkResult current = {0};

        if (benchmark_run_once(&input, &current) != 0)
        {
            benchmark_cleanup_input(&input);
            return -1;
        }

        if (runIndex >= options->warmupIterations)
            benchmark_summary_add(&summary, &current);
    }

    benchmark_print_case_summary(benchmarkCase, &input, options, &summary);
    benchmark_cleanup_input(&input);
    return 0;
}

static int benchmark_run_suite(const BenchmarkCase* cases,
                               size_t caseCount,
                               const BenchmarkOptions* options)
{
    size_t caseIndex = 0U;

    if (!cases || !options)
        return -1;

    for (caseIndex = 0U; caseIndex < caseCount; ++caseIndex)
    {
        if (benchmark_run_case(&cases[caseIndex], options) != 0)
            return -1;

        if (caseIndex + 1U < caseCount)
            (void)printf("\n");
    }

    return 0;
}

int main(int argc, char** argv)
{
    BenchmarkOptions options = {0};
    BenchmarkCase benchmarkCase = {0};
    size_t syntheticCount = sizeof(kSyntheticCases) / sizeof(kSyntheticCases[0]);
    size_t realCount = sizeof(kRealCases) / sizeof(kRealCases[0]);
    int parseStatus = 0;

    parseStatus = benchmark_parse_args(argc, argv, &options);
    if (parseStatus == 1)
    {
        benchmark_print_usage(argv[0]);
        return 0;
    }

    if (parseStatus != 0)
    {
        benchmark_print_usage(argv[0]);
        return 2;
    }

    if (options.suite == BENCHMARK_SUITE_SYNTHETIC)
    {
        if (benchmark_run_suite(kSyntheticCases, syntheticCount, &options) != 0)
        {
            (void)fprintf(stderr, "Benchmark suite failed: synthetic\n");
            return 1;
        }
    }
    else if (options.suite == BENCHMARK_SUITE_REAL)
    {
        if (benchmark_run_suite(kRealCases, realCount, &options) != 0)
        {
            (void)fprintf(stderr, "Benchmark suite failed: real\n");
            return 1;
        }
    }
    else if (options.suite == BENCHMARK_SUITE_ALL)
    {
        if (benchmark_run_suite(kSyntheticCases, syntheticCount, &options) != 0)
        {
            (void)fprintf(stderr, "Benchmark suite failed: all (synthetic)\n");
            return 1;
        }

        (void)printf("\n");

        if (benchmark_run_suite(kRealCases, realCount, &options) != 0)
        {
            (void)fprintf(stderr, "Benchmark suite failed: all (real)\n");
            return 1;
        }
    }
    else
    {
        benchmarkCase = benchmark_case_from_options(&options);
        if (benchmark_run_case(&benchmarkCase, &options) != 0)
        {
            (void)fprintf(stderr, "Benchmark run failed.\n");
            return 1;
        }
    }

    return 0;
}
