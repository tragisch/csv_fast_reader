# csv-spectrum fixtures

This directory contains a curated subset of test fixtures derived from
[`max-mapper/csv-spectrum`](https://github.com/max-mapper/csv-spectrum).

Purpose:
- parser correctness and regression tests
- special-case coverage for quotes, commas and embedded newlines
- not intended as the primary performance corpus

Source repository:
- https://github.com/max-mapper/csv-spectrum

Upstream package metadata declares the license as **BSD-2-Clause**.
These files are vendored here in a small subset for test-fixture use.

Included fixtures:
- `simple.csv`
- `comma_in_quotes.csv`
- `escaped_quotes.csv`
- `newlines.csv`
- `quotes_and_newlines.csv`
