# csv_fast

`csv_fast` is a small CSV parsing library in C with a simple row/field API.

- Build system: Bazel
- Platforms: macOS and Linux
- Scope: CSV parsing with support for quoted fields, CRLF, and embedded newlines
- Input: file path (mmap fast path) or `FILE*` stream

## Build

```bash
bazel build --config=opt //apps/csv_fast:csv_fast
```

## Test

```bash
bazel test //tests:test_csv
```

## Benchmarks

Die Benchmarks verwenden drei Real-World-Datensätze, die **nicht** im Repository enthalten sind.
Lade sie herunter und lege sie unter `data/` ab:

| Datei | Quelle |
|---|---|
| `cbp23co.csv` | [Census Bureau – County Business Patterns 2023](https://www.census.gov/data/datasets/2023/econ/cbp/2023-cbp.html) |
| `zbp23detail.csv` | [Census Bureau – ZIP Code Business Patterns 2023](https://www.census.gov/data/datasets/2023/econ/zbp/2023-zbp.html) |
| `star2002-full.csv` | [LBNL / FastBit – STAR 2000](http://sdm.lbl.gov/fastbit/data/star2000.csv.gz) |

```bash
bazel run //benchmarks:csv_benchmark -- --suite real
bazel run //benchmarks:csv_parser_benchmark -- --suite real
bazel run //benchmarks:libcsv_benchmark -- --suite real
bazel run //benchmarks:rapidcsv_benchmark -- --suite real
bazel run //benchmarks:fastcsv_benchmark -- --suite real
```

Die C++-Vergleichsbenchmarks verwenden `csv-parser` und `rapidcsv`; der FastCSV-Benchmark verwendet `de.siegmar:fastcsv:4.1.1` und wird beim ersten Build reproduzierbar aus Maven Central geladen.

## Install

```bash
bazel run //:install_csv_fast -- /usr/local
```

Headers are installed to `include/` and the library to `lib/`.

## API

Main functions:

- `csv_reader_open()`
- `csv_reader_open_with_options()`
- `csv_reader_open_stream()`
- `csv_reader_open_stream_with_options()`
- `csv_reader_next_row()`
- `csv_reader_first_col()`
- `csv_reader_next_col_in_row()`
- `csv_reader_close()`

Example:

```c
#include <stdio.h>

#include "csv.h"

int main(void)
{
    CsvReader *reader = NULL;
    CsvStringView row = {0};
    CsvStringView field = {0};
    CsvStatus status = CSV_STATUS_OK;

    if (csv_reader_open(&reader, "data.csv") != CSV_STATUS_OK)
        return 1;

    while (csv_reader_next_row(reader, &row) == CSV_STATUS_OK) {
        status = csv_reader_first_col(reader, &row, &field);
        if (status == CSV_STATUS_EOF)
            continue;
        if (status != CSV_STATUS_OK)
            break;

        do {
            printf("%.*s\n", (int)field.len, field.ptr);
            status = csv_reader_next_col_in_row(reader, &field);
        } while (status == CSV_STATUS_OK);

        if (status != CSV_STATUS_EOF)
            break;
    }

    csv_reader_close(reader);
    return 0;
}
```

## Notes

- `CsvStringView` points to reader-owned memory.
- `row.ptr` and `field.ptr` stay valid until the next `csv_reader_next_row()` call or `csv_reader_close()`.
- Field parsing modifies the row buffer in place.
- File-based input uses memory-mapped I/O for maximum throughput.
- Stream-based input (`FILE*`) is borrowed, not owned: `csv_reader_close()` does **not** close the stream. The caller is responsible for closing the `FILE*` after the reader is closed.
