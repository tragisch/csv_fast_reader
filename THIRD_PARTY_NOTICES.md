# Third-Party Notices

This repository includes or depends on third-party software.

## Original work

Parts of `apps/csv_fast/csv.c` and `apps/csv_fast/csv.h` are derived from earlier MIT-licensed work by Jan Doczy. The source headers document this provenance.

## Vendored files

- `third_party/csv-parser/`
  - includes `LICENSE-3.txt`
  - retained under its upstream license terms

## Bazel-fetched dependencies

These dependencies are fetched during the build and are declared in `MODULE.bazel`:

- Unity v2.5.2
  - source: `https://github.com/ThrowTheSwitch/Unity`
  - used for C unit tests
- dbg-macro v0.13.0
  - source: `https://github.com/eerimoq/dbg-macro`
  - used as a development/debugging dependency
- libcsv
  - source: `https://github.com/rgamble/libcsv`
  - license: LGPL-2.1-or-later
  - used for benchmark comparisons
- rapidcsv
  - source: `https://github.com/d99kris/rapidcsv`
  - license: BSD-3-Clause
  - used for benchmark comparisons

When distributing this project, review the upstream licenses of fetched dependencies in addition to the root `LICENSE` file.
