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
#include <stdio.h>

/* simple and fast CSV reader:
 * 1. Open a CSV file by calling csv_reader_open() or
 *    csv_reader_open_with_options()
 * 2. Read a logical CSV row with csv_reader_next_row()
 * 3. Iterate that row's columns with csv_reader_first_col() and
 *    csv_reader_next_col_in_row()
 *
 * Alternatively, open a reader from a FILE* stream via
 * csv_reader_open_stream() or csv_reader_open_stream_with_options().
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

/**
 * Borrowed view into reader-owned storage.
 * The pointer must not be freed by the caller and may be modified in-place by
 * csv_reader_next_col(). The data is only valid for the lifetime documented by
 * the API function that produced it.
 */
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
 * @options: Parsing options. Must not be NULL. Delimiter, quote and escape
 *           must not be '\0', '\r' or '\n'. Quote and escape must differ.
 * @return: CSV_STATUS_OK on success, otherwise a failure status.
 * @notes: On success the caller owns `*out_reader` and must release it with
 *         csv_reader_close().
 */
CsvStatus csv_reader_open_with_options(CsvReader **out_reader, const char *filename,
                                       const CsvOptions *options);

/**
 * Opens a CSV reader that reads from a FILE* stream with default options.
 * @out_reader: Output location receiving an owned reader handle.
 * @stream: Borrowed FILE* stream open for reading. The caller retains
 *          ownership; csv_reader_close() will NOT close the stream.
 * @return: CSV_STATUS_OK on success, otherwise a failure status.
 * @notes: On success the caller owns `*out_reader` and must release it with
 *         csv_reader_close().
 */
CsvStatus csv_reader_open_stream(CsvReader **out_reader, FILE *stream);

/**
 * Opens a CSV reader that reads from a FILE* stream with caller-provided
 * options.
 * @out_reader: Output location receiving an owned reader handle.
 * @stream: Borrowed FILE* stream open for reading. The caller retains
 *          ownership; csv_reader_close() will NOT close the stream.
 * @options: Parsing options. Same constraints as csv_reader_open_with_options().
 * @return: CSV_STATUS_OK on success, otherwise a failure status.
 * @notes: On success the caller owns `*out_reader` and must release it with
 *         csv_reader_close().
 */
CsvStatus csv_reader_open_stream_with_options(CsvReader **out_reader, FILE *stream,
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
 *         csv_reader_next_col() parses fields in-place and may overwrite
 *         delimiter or quote boundaries inside this row buffer with '\0'.
 */
CsvStatus csv_reader_next_row(CsvReader *reader, CsvStringView *out_row);

/**
 * Convenience wrapper for reading the first parsed column from a row.
 * @reader: Reader handle.
 * @row: Row view returned by csv_reader_next_row(). Must not be NULL.
 * @out_col: Borrowed field view backed by reader-owned row storage.
 * @return: Same status contract as csv_reader_next_col().
 * @notes: Equivalent to calling csv_reader_next_col(@reader, @row, @out_col).
 */
CsvStatus csv_reader_first_col(CsvReader *reader,
                               const CsvStringView *row,
                               CsvStringView *out_col);

/**
 * Convenience wrapper for reading subsequent parsed columns from the current
 * row.
 * @reader: Reader handle.
 * @out_col: Borrowed field view backed by reader-owned row storage.
 * @return: Same status contract as csv_reader_next_col().
 * @notes: Equivalent to calling csv_reader_next_col(@reader, NULL, @out_col).
 */
CsvStatus csv_reader_next_col_in_row(CsvReader *reader, CsvStringView *out_col);

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
 *         The first call for a row requires @row; subsequent calls for the
 *         same row must pass NULL while the reader's internal column-iterator
 *         state is active. This function mutates the row buffer in-place and
 *         is not reentrant for the same reader.
 */
CsvStatus csv_reader_next_col(CsvReader *reader, const CsvStringView *row, CsvStringView *out_col);

/* ------------------------------------------------------------------ */
/* CSV Writer                                                         */
/* ------------------------------------------------------------------ */

typedef struct CsvWriter_ CsvWriter;

/**
 * Opens a CSV writer that writes to a file path with default options.
 * @out_writer: Output location receiving an owned writer handle.
 * @filename: Pathname of the output file. The file is created or truncated.
 * @return: CSV_STATUS_OK on success, otherwise a failure status.
 * @notes: On success the caller owns `*out_writer` and must release it with
 *         csv_writer_close().
 */
CsvStatus csv_writer_open(CsvWriter **out_writer, const char *filename);

/**
 * Opens a CSV writer that writes to a file path with caller-provided options.
 * @out_writer: Output location receiving an owned writer handle.
 * @filename: Pathname of the output file. The file is created or truncated.
 * @options: Formatting options (delimiter, quote, escape characters).
 *           Same character constraints as csv_reader_open_with_options().
 * @return: CSV_STATUS_OK on success, otherwise a failure status.
 * @notes: On success the caller owns `*out_writer` and must release it with
 *         csv_writer_close().
 */
CsvStatus csv_writer_open_with_options(CsvWriter **out_writer, const char *filename,
                                       const CsvOptions *options);

/**
 * Opens a CSV writer over a borrowed FILE* stream with default options.
 * @out_writer: Output location receiving an owned writer handle.
 * @stream: Borrowed FILE* stream open for writing. The caller retains
 *          ownership; csv_writer_close() will NOT close the stream.
 * @return: CSV_STATUS_OK on success, otherwise a failure status.
 */
CsvStatus csv_writer_open_stream(CsvWriter **out_writer, FILE *stream);

/**
 * Opens a CSV writer over a borrowed FILE* stream with caller-provided options.
 * @out_writer: Output location receiving an owned writer handle.
 * @stream: Borrowed FILE* stream open for writing. The caller retains
 *          ownership; csv_writer_close() will NOT close the stream.
 * @options: Formatting options. Same character constraints as
 *           csv_reader_open_with_options().
 * @return: CSV_STATUS_OK on success, otherwise a failure status.
 */
CsvStatus csv_writer_open_stream_with_options(CsvWriter **out_writer, FILE *stream,
                                              const CsvOptions *options);

/**
 * Writes a single field to the current row.
 * Fields containing the delimiter, quote character, or newlines are
 * automatically quoted. Quote characters within the data are escaped by
 * doubling them (RFC 4180).
 * @writer: Writer handle.
 * @data: Pointer to the field contents. May be NULL if @len is 0.
 * @len: Length of the field in bytes.
 * @return: CSV_STATUS_OK on success, otherwise a failure status.
 */
CsvStatus csv_writer_field(CsvWriter *writer, const char *data, size_t len);

/**
 * Convenience wrapper: writes a NUL-terminated string as a single field.
 * @writer: Writer handle.
 * @str: NUL-terminated field string. Must not be NULL.
 * @return: CSV_STATUS_OK on success, otherwise a failure status.
 */
CsvStatus csv_writer_field_str(CsvWriter *writer, const char *str);

/**
 * Terminates the current row by writing a newline (LF).
 * @writer: Writer handle.
 * @return: CSV_STATUS_OK on success, otherwise a failure status.
 */
CsvStatus csv_writer_end_row(CsvWriter *writer);

/**
 * Flushes the internal write buffer to the underlying file/stream.
 * @writer: Writer handle.
 * @return: CSV_STATUS_OK on success, CSV_STATUS_IO_ERROR on write failure.
 */
CsvStatus csv_writer_flush(CsvWriter *writer);

/**
 * Flushes remaining data and releases all resources.
 * If the writer owns the file it was opened with, the file is closed.
 * @writer: Owned writer handle. May be NULL (no-op).
 */
void csv_writer_close(CsvWriter *writer);

#ifdef __cplusplus
};
#endif
