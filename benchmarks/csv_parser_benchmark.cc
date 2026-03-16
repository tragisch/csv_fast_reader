#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <time.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <sys/stat.h>
#include <unistd.h>

#include "csv.hpp"

namespace {

enum BenchmarkSuite {
    BENCHMARK_SUITE_NONE = 0,
    BENCHMARK_SUITE_REAL,
    BENCHMARK_SUITE_ALL,
};

struct BenchmarkOptions {
    BenchmarkSuite suite;
    const char* inputPath;
    std::size_t warmupIterations;
    std::size_t iterations;
};

struct BenchmarkCase {
    const char* label;
    const char* inputPath;
};

struct BenchmarkInput {
    char path[1024];
    std::size_t bytes;
};

struct BenchmarkResult {
    std::size_t rows;
    std::size_t fields;
    std::size_t bytes;
    double seconds;
};

struct BenchmarkSummary {
    BenchmarkResult best;
    BenchmarkResult worst;
    std::size_t measuredIterations;
    double totalSeconds;
};

const BenchmarkCase kRealCases[] = {
    {"cbp23co", "data/cbp23co.csv"},
    {"zbp23detail", "data/zbp23detail.csv"},
    {"star2002-full", "data/star2002-full.csv"},
};

void benchmark_print_usage(const char* argv0)
{
    std::fprintf(stderr,
                 "Usage: %s [--suite real|all] [--input PATH]"
                 " [--warmup N] [--iterations N]\n",
                 argv0);
}

int benchmark_parse_size(const char* text, std::size_t* out_value)
{
    char* end = nullptr;
    unsigned long long value = 0ULL;

    if (!text || !out_value || *text == '\0')
        return -1;

    errno = 0;
    value = std::strtoull(text, &end, 10);
    if (errno != 0 || !end || *end != '\0')
        return -1;

    *out_value = static_cast<std::size_t>(value);
    return 0;
}

int benchmark_parse_suite(const char* text, BenchmarkSuite* out_suite)
{
    if (!text || !out_suite)
        return -1;

    if (std::strcmp(text, "real") == 0)
        *out_suite = BENCHMARK_SUITE_REAL;
    else if (std::strcmp(text, "all") == 0)
        *out_suite = BENCHMARK_SUITE_ALL;
    else
        return -1;

    return 0;
}

double benchmark_throughput_mib_per_second(std::size_t bytes, double seconds)
{
    if (seconds <= 0.0)
        return 0.0;

    return (static_cast<double>(bytes) / (1024.0 * 1024.0)) / seconds;
}

double benchmark_rate_per_second(std::size_t items, double seconds)
{
    if (seconds <= 0.0)
        return 0.0;

    return static_cast<double>(items) / seconds;
}

int benchmark_copy_path(const char* source, char* out_path, std::size_t out_path_size)
{
    const int written = 0;
    int actual = 0;

    (void)written;

    if (!source || !out_path || out_path_size == 0U)
        return -1;

    actual = std::snprintf(out_path, out_path_size, "%s", source);
    if (actual < 0 || static_cast<std::size_t>(actual) >= out_path_size)
        return -1;

    return 0;
}

int benchmark_join_path(const char* prefix,
                        const char* suffix,
                        char* out_path,
                        std::size_t out_path_size)
{
    int written = 0;

    if (!prefix || !suffix || !out_path || out_path_size == 0U)
        return -1;

    written = std::snprintf(out_path, out_path_size, "%s/%s", prefix, suffix);
    if (written < 0 || static_cast<std::size_t>(written) >= out_path_size)
        return -1;

    return 0;
}

int benchmark_path_exists(const char* path, struct stat* st)
{
    if (!path || !st)
        return 0;

    return stat(path, st) == 0;
}

int benchmark_resolve_input_path(const char* input_path,
                                 char* out_path,
                                 std::size_t out_path_size,
                                 struct stat* st)
{
    const char* workingDirectory = std::getenv("BUILD_WORKING_DIRECTORY");
    const char* runfilesDirectory = std::getenv("RUNFILES_DIR");
    const char* workspaceDirectory = std::getenv("BUILD_WORKSPACE_DIRECTORY");
    const char* testSrcDir = std::getenv("TEST_SRCDIR");
    const char* testWorkspace = std::getenv("TEST_WORKSPACE");
    char cwd[1024] = {0};
    char candidate[1024] = {0};
    char runfilesRoot[1024] = {0};

    if (!input_path || !out_path || out_path_size == 0U || !st)
        return -1;

    if (benchmark_copy_path(input_path, candidate, sizeof(candidate)) == 0 &&
        benchmark_path_exists(candidate, st))
        return benchmark_copy_path(candidate, out_path, out_path_size);

    if (runfilesDirectory && runfilesDirectory[0] != '\0' &&
        benchmark_join_path(runfilesDirectory, input_path, candidate, sizeof(candidate)) == 0 &&
        benchmark_path_exists(candidate, st))
        return benchmark_copy_path(candidate, out_path, out_path_size);

    if (runfilesDirectory && runfilesDirectory[0] != '\0' &&
        benchmark_join_path(runfilesDirectory, "_main", runfilesRoot, sizeof(runfilesRoot)) == 0 &&
        benchmark_join_path(runfilesRoot, input_path, candidate, sizeof(candidate)) == 0 &&
        benchmark_path_exists(candidate, st))
        return benchmark_copy_path(candidate, out_path, out_path_size);

    if (testSrcDir && testSrcDir[0] != '\0' &&
        testWorkspace && testWorkspace[0] != '\0' &&
        benchmark_join_path(testSrcDir, testWorkspace, runfilesRoot, sizeof(runfilesRoot)) == 0 &&
        benchmark_join_path(runfilesRoot, input_path, candidate, sizeof(candidate)) == 0 &&
        benchmark_path_exists(candidate, st))
        return benchmark_copy_path(candidate, out_path, out_path_size);

    if (workingDirectory && workingDirectory[0] != '\0' &&
        benchmark_join_path(workingDirectory, input_path, candidate, sizeof(candidate)) == 0 &&
        benchmark_path_exists(candidate, st))
        return benchmark_copy_path(candidate, out_path, out_path_size);

    if (workspaceDirectory && workspaceDirectory[0] != '\0' &&
        benchmark_join_path(workspaceDirectory, input_path, candidate, sizeof(candidate)) == 0 &&
        benchmark_path_exists(candidate, st))
        return benchmark_copy_path(candidate, out_path, out_path_size);

    if (getcwd(cwd, sizeof(cwd)) != nullptr &&
        benchmark_join_path(cwd, input_path, candidate, sizeof(candidate)) == 0 &&
        benchmark_path_exists(candidate, st))
        return benchmark_copy_path(candidate, out_path, out_path_size);

    return -1;
}

int benchmark_parse_args(int argc, char** argv, BenchmarkOptions* options)
{
    int i = 0;

    if (!options)
        return -1;

    options->suite = BENCHMARK_SUITE_NONE;
    options->inputPath = nullptr;
    options->warmupIterations = 1U;
    options->iterations = 5U;

    for (i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--suite") == 0)
        {
            if (i + 1 >= argc || benchmark_parse_suite(argv[i + 1], &options->suite) != 0)
                return -1;

            i++;
        }
        else if (std::strcmp(argv[i], "--input") == 0)
        {
            if (i + 1 >= argc || argv[i + 1][0] == '\0')
                return -1;

            options->inputPath = argv[i + 1];
            i++;
        }
        else if (std::strcmp(argv[i], "--warmup") == 0)
        {
            if (i + 1 >= argc || benchmark_parse_size(argv[i + 1], &options->warmupIterations) != 0)
                return -1;

            i++;
        }
        else if (std::strcmp(argv[i], "--iterations") == 0)
        {
            if (i + 1 >= argc || benchmark_parse_size(argv[i + 1], &options->iterations) != 0)
                return -1;

            i++;
        }
        else if (std::strcmp(argv[i], "--help") == 0)
        {
            return 1;
        }
        else
        {
            return -1;
        }
    }

    if (((options->suite == BENCHMARK_SUITE_NONE && options->inputPath == nullptr) ||
         options->iterations == 0U))
        return -1;

    return 0;
}

int benchmark_prepare_input(const BenchmarkCase* benchmarkCase, BenchmarkInput* input)
{
    struct stat st = {0};

    if (!benchmarkCase || !input || !benchmarkCase->inputPath)
        return -1;

    std::memset(input, 0, sizeof(*input));
    if (benchmark_resolve_input_path(benchmarkCase->inputPath,
                                     input->path,
                                     sizeof(input->path),
                                     &st) != 0)
        return -1;

    input->bytes = static_cast<std::size_t>(st.st_size);
    return 0;
}

double benchmark_elapsed_seconds(const struct timespec* start, const struct timespec* end)
{
    const double seconds = static_cast<double>(end->tv_sec - start->tv_sec);
    const long nanoseconds = end->tv_nsec - start->tv_nsec;
    return seconds + (static_cast<double>(nanoseconds) / 1000000000.0);
}

int benchmark_run_once(const BenchmarkInput* input, BenchmarkResult* result)
{
    struct timespec start = {0};
    struct timespec end = {0};
    csv::CSVFormat format;
    std::size_t rowsRead = 0U;
    std::size_t fieldsRead = 0U;

    if (!input || !result)
        return -1;

    format.delimiter(',');
    format.quote('"');
    format.header_row(-1);
    format.variable_columns(csv::VariableColumnPolicy::KEEP);

    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0)
        return -1;

    try
    {
        csv::CSVReader reader(input->path, format);
        for (csv::CSVRow& row : reader)
        {
            rowsRead++;
            fieldsRead += row.size();
        }
    }
    catch (const std::exception&)
    {
        return -1;
    }

    if (clock_gettime(CLOCK_MONOTONIC, &end) != 0)
        return -1;

    result->rows = rowsRead;
    result->fields = fieldsRead;
    result->bytes = input->bytes;
    result->seconds = benchmark_elapsed_seconds(&start, &end);
    return 0;
}

void benchmark_summary_add(BenchmarkSummary* summary, const BenchmarkResult* result)
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

void benchmark_print_case_summary(const BenchmarkCase* benchmarkCase,
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

    averageSeconds = summary->totalSeconds / static_cast<double>(summary->measuredIterations);
    averageThroughput = benchmark_throughput_mib_per_second(summary->best.bytes, averageSeconds);
    bestThroughput = benchmark_throughput_mib_per_second(summary->best.bytes, summary->best.seconds);
    worstThroughput = benchmark_throughput_mib_per_second(summary->worst.bytes, summary->worst.seconds);
    averageRowsPerSecond = benchmark_rate_per_second(summary->best.rows, averageSeconds);
    averageFieldsPerSecond = benchmark_rate_per_second(summary->best.fields, averageSeconds);

    std::printf("case=%s input=%s warmup=%zu iterations=%zu bytes=%zu rows=%zu\n",
                benchmarkCase->label,
                input->path,
                options->warmupIterations,
                summary->measuredIterations,
                summary->best.bytes,
                summary->best.rows);
    std::printf("avg=%.6f s  best=%.6f s  worst=%.6f s\n",
                averageSeconds,
                summary->best.seconds,
                summary->worst.seconds);
    std::printf("avg_throughput=%.2f MiB/s  best_throughput=%.2f MiB/s  worst_throughput=%.2f MiB/s\n",
                averageThroughput,
                bestThroughput,
                worstThroughput);
    std::printf("avg_rows/s=%.0f  avg_fields/s=%.0f\n",
                averageRowsPerSecond,
                averageFieldsPerSecond);
}

int benchmark_run_case(const BenchmarkCase* benchmarkCase, const BenchmarkOptions* options)
{
    BenchmarkInput input = {};
    BenchmarkSummary summary = {};
    std::size_t totalRuns = 0U;
    std::size_t runIndex = 0U;

    if (!benchmarkCase || !options)
        return -1;

    if (benchmark_prepare_input(benchmarkCase, &input) != 0)
        return -1;

    totalRuns = options->warmupIterations + options->iterations;
    for (runIndex = 0U; runIndex < totalRuns; ++runIndex)
    {
        BenchmarkResult current = {0};

        if (benchmark_run_once(&input, &current) != 0)
            return -1;

        if (runIndex >= options->warmupIterations)
            benchmark_summary_add(&summary, &current);
    }

    benchmark_print_case_summary(benchmarkCase, &input, options, &summary);
    return 0;
}

int benchmark_run_suite(const BenchmarkCase* cases,
                        std::size_t caseCount,
                        const BenchmarkOptions* options)
{
    std::size_t caseIndex = 0U;

    if (!cases || !options)
        return -1;

    for (caseIndex = 0U; caseIndex < caseCount; ++caseIndex)
    {
        if (benchmark_run_case(&cases[caseIndex], options) != 0)
            return -1;

        if (caseIndex + 1U < caseCount)
            std::printf("\n");
    }

    return 0;
}

}  // namespace

int main(int argc, char** argv)
{
    BenchmarkOptions options = {};
    BenchmarkCase benchmarkCase = {};
    const std::size_t realCount = sizeof(kRealCases) / sizeof(kRealCases[0]);
    const int parseStatus = benchmark_parse_args(argc, argv, &options);

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

    if (options.suite == BENCHMARK_SUITE_REAL || options.suite == BENCHMARK_SUITE_ALL)
    {
        if (benchmark_run_suite(kRealCases, realCount, &options) != 0)
        {
            std::fprintf(stderr, "Benchmark suite failed: real\n");
            return 1;
        }

        return 0;
    }

    benchmarkCase.label = options.inputPath;
    benchmarkCase.inputPath = options.inputPath;
    if (benchmark_run_case(&benchmarkCase, &options) != 0)
    {
        std::fprintf(stderr, "Benchmark run failed.\n");
        return 1;
    }

    return 0;
}
