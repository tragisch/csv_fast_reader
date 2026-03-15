# csv_fast

`csv_fast` is a small CSV parsing library in C.
It is built with Bazel and currently targets **macOS** and **Linux**.

## Build

```bash
bazel build //apps/csv_fast:csv_fast
```

Run the parser tests with:

```bash
bazel test //tests:test_csv
```

## Recommended API

The preferred API is the status-based reader interface from `apps/csv_fast/csv.h`:

- `csv_reader_open()`
- `csv_reader_open_with_options()`
- `csv_reader_next_row()`
- `csv_reader_next_col()`
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

### Lifetime rules

- `row.ptr` stays valid until the next `csv_reader_next_row()` call.
- `field.ptr` stays valid until the next `csv_reader_next_row()` call.
- The first `csv_reader_next_col()` call for a row needs that row.
- Later `csv_reader_next_col()` calls for the same row use `NULL`.

## Notes

- Quoted fields, escaped quotes, CRLF input and embedded newlines are covered by tests.
- Benchmarks live under `benchmarks/` (`benchmarks/csv_benchmark.c` for the runner, `benchmarks/results/` for local outputs), but the main focus of this project is the parsing API.
