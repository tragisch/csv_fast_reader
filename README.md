# csv_fast

[![CI](https://github.com/tragisch/csv_fast_reader/actions/workflows/ci.yml/badge.svg?branch=master)](https://github.com/tragisch/csv_fast_reader/actions/workflows/ci.yml)

`csv_fast` is a small and fast CSV parsing library in C.

- build system: Bazel
- platforms: macOS and Linux
- focus: simple reader API with tested handling for quotes, escaped quotes, CRLF and embedded newlines

## Quick start

Build the library:

```bash
bazel build //apps/csv_fast:csv_fast
```

Run the tests:

```bash
bazel test //tests:test_csv
```

Install:

```bash
bazel run //:install_csv_fast -- /usr/local
```
The installer places headers under `include/` and libraries under `lib/`.

## API

The recommended API is defined in `apps/csv_fast/csv.h`:

- `csv_reader_open()`
- `csv_reader_open_with_options()`
- `csv_reader_next_row()`
- `csv_reader_next_col()`
- `csv_reader_close()`

Minimal example:

```c
#include <stdio.h>

#include "csv.h"

int main(void)
{
	CsvReader *reader = NULL;
	CsvStringView row = {0};
	CsvStringView field = {0};

	if (csv_reader_open(&reader, "data.csv") != CSV_STATUS_OK) {
		return 1;
	}

	while (csv_reader_next_row(reader, &row) == CSV_STATUS_OK) {
		const CsvStringView *current_row = &row;

		while (csv_reader_next_col(reader, current_row, &field) == CSV_STATUS_OK) {
			printf("%.*s\n", (int)field.len, field.ptr);
			current_row = NULL;
		}
	}

	csv_reader_close(reader);
	return 0;
}
```

## Important rules

- `row.ptr` remains valid until the next call to `csv_reader_next_row()`.
- `field.ptr` remains valid until the next call to `csv_reader_next_row()`.
- The first call to `csv_reader_next_col()` for a row needs that row as an argument.
- Later calls for the same row use `NULL`.

## Project structure

- `apps/csv_fast/` – library
- `tests/` – Unity tests
- `benchmarks/` – benchmarks
- `tools/install/` – Bazel installer

