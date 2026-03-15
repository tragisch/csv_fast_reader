#pragma once

/*
 * SPDX-License-Identifier: MIT
 *
 * Original work:
 * Copyright (c) 2019 Jan Doczy
 *
 * Modifications and extended rework:
 * Copyright (c) 2025-2026 @tragisch <https://github.com/tragisch>
 *
 * This file contains substantial modifications of the original MIT-licensed
 * work. See the LICENSE file in the project root for license details.
 */

#include <stddef.h>

/* simple and fast CSV reader:
 * 1. Open a CSV file by calling csv_reader_open() or
 *    csv_reader_open_with_options()
 * 2. Read a logical CSV row with csv_reader_next_row()
 * 3. Iterate that row's columns with csv_reader_next_col()
 */

#ifdef __cplusplus
extern "C" { /* C++ name mangling */
#endif

typedef struct CsvHandle_ CsvReader;

typedef enum CsvStatus {
    CSV_STATUS_OK = 0,
    CSV_STATUS_EOF,
    CSV_STATUS_INVALID_ARGUMENT,
    CSV_STATUS_IO_ERROR,
    CSV_STATUS_PARSE_ERROR,
    CSV_STATUS_NO_MEMORY,
} CsvStatus;

typedef struct CsvStringView {
    char *ptr;
    size_t len;
} CsvStringView;

typedef struct CsvOptions {
    char delim;
    char quote;
    char escape;
} CsvOptions;

/**
 * Returns a human-readable string for @status (e.g. "CSV_STATUS_OK").
 * The returned pointer is valid for the lifetime of the program.
 */
const char *csv_status_string(CsvStatus status);

/**
 * Returns the default CSV parsing options.
 * @return: Value object containing delimiter, quote and escape defaults.
 */
CsvOptions csv_options_default(void);

/**
 * Opens a CSV file with default CSV characters.
 * @out_reader: Output location receiving an owned reader handle.
 * @filename: Pathname of the file.
 * @return: CSV_STATUS_OK on success, otherwise a failure status.
 * @notes: On success the caller owns `*out_reader` and must release it with
 *         csv_reader_close().
 */
CsvStatus csv_reader_open(CsvReader **out_reader, const char *filename);

/**
 * Opens a CSV file with caller-provided options.
 * @out_reader: Output location receiving an owned reader handle.
 * @filename: Pathname of the file.
 * @options: Parsing options. Must not be NULL.
 * @return: CSV_STATUS_OK on success, otherwise a failure status.
 * @notes: On success the caller owns `*out_reader` and must release it with
 *         csv_reader_close().
 */
CsvStatus csv_reader_open_with_options(CsvReader **out_reader, const char *filename,
                                       const CsvOptions *options);

/**
 * Closes a reader and releases all associated resources.
 * @reader: Owned reader previously returned by csv_reader_open() or
 *          csv_reader_open_with_options().
 */
void csv_reader_close(CsvReader *reader);

/**
 * Reads the first or next logical CSV row.
 * @reader: Reader handle.
 * @out_row: Borrowed row view backed by reader-owned storage.
 * @return: CSV_STATUS_OK on success, CSV_STATUS_EOF when no more rows are
 *          available, CSV_STATUS_PARSE_ERROR for malformed quoted rows,
 *          otherwise a failure status.
 * @notes: The returned row view is valid until the next csv_reader_next_row()
 *         call on the same reader or until csv_reader_close().
 */
CsvStatus csv_reader_next_row(CsvReader *reader, CsvStringView *out_row);

/**
 * Returns the next parsed column from a row.
 * @reader: Reader handle.
 * @row: Row view returned by csv_reader_next_row(). Required for the first
 *       column of a row; may be NULL for subsequent columns while parser
 *       context is active.
 * @out_col: Borrowed field view backed by reader-owned row storage.
 * @return: CSV_STATUS_OK on success, CSV_STATUS_EOF when no more columns are
 *          available in the current row, CSV_STATUS_PARSE_ERROR for malformed
 *          field quoting, otherwise a failure status.
 * @notes: The returned field view is valid until the next csv_reader_next_row()
 *         call on the same reader or until csv_reader_close().
 */
CsvStatus csv_reader_next_col(CsvReader *reader, const CsvStringView *row, CsvStringView *out_col);

#ifdef __cplusplus
};
#endif
