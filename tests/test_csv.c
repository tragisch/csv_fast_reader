#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "unity.h"

#include "csv.h"

typedef struct CsvTempFile_ {
    char path[64];
} CsvTempFile;

void setUp(void)
{
}

void tearDown(void)
{
}

static void csv_write_temp_file(CsvTempFile* temp_file, const char* contents)
{
    int fd = -1;
    FILE* file = NULL;
    size_t length = 0U;

    TEST_ASSERT_NOT_NULL(temp_file);
    TEST_ASSERT_NOT_NULL(contents);

    (void)snprintf(temp_file->path, sizeof(temp_file->path),
                   "/tmp/csv_fast_XXXXXX");

    fd = mkstemp(temp_file->path);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, fd);

    file = fdopen(fd, "wb");
    TEST_ASSERT_NOT_NULL(file);

    length = strlen(contents);
    TEST_ASSERT_EQUAL_UINT64(length,
                             fwrite(contents, 1U, length, file));
    TEST_ASSERT_EQUAL_INT(0, fclose(file));
}

static void csv_fixture_path(const char* relative_path,
                             char* out_path,
                             size_t out_path_size)
{
    const char* srcdir = getenv("TEST_SRCDIR");
    const char* workspace = getenv("TEST_WORKSPACE");
    int written = 0;

    TEST_ASSERT_NOT_NULL(relative_path);
    TEST_ASSERT_NOT_NULL(out_path);
    TEST_ASSERT_NOT_NULL(srcdir);
    TEST_ASSERT_NOT_NULL(workspace);

    written = snprintf(out_path, out_path_size, "%s/%s/%s",
                       srcdir, workspace, relative_path);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, written);
    TEST_ASSERT_TRUE((size_t)written < out_path_size);
}

static CsvReader* csv_reader_open_temp_file(const char* contents,
                                            CsvTempFile* temp_file)
{
    CsvReader* reader = NULL;

    csv_write_temp_file(temp_file, contents);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_reader_open(&reader, temp_file->path));
    TEST_ASSERT_NOT_NULL(reader);
    return reader;
}

static void csv_reader_close_temp_file(CsvReader* reader, CsvTempFile* temp_file)
{
    csv_reader_close(reader);
    TEST_ASSERT_EQUAL_INT(0, unlink(temp_file->path));
}

static CsvReader* csv_reader_open_fixture(const char* relative_path)
{
    char path[1024] = {0};
    CsvReader* reader = NULL;

    csv_fixture_path(relative_path, path, sizeof(path));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_open(&reader, path));
    TEST_ASSERT_NOT_NULL(reader);
    return reader;
}

static void csv_assert_row_fields_view(CsvReader* reader,
                                       const CsvStringView* row,
                                       const char* const* expected_fields,
                                       size_t expected_field_count)
{
    CsvStringView field = {0};
    const CsvStringView* current_row = row;
    size_t field_index = 0U;

    TEST_ASSERT_NOT_NULL(reader);
    TEST_ASSERT_NOT_NULL(row);
    TEST_ASSERT_NOT_NULL(expected_fields);

    while (csv_reader_next_col(reader, current_row, &field) == CSV_STATUS_OK)
    {
        TEST_ASSERT_LESS_THAN_UINT(expected_field_count, field_index);
        TEST_ASSERT_EQUAL_UINT(strlen(expected_fields[field_index]), field.len);
        TEST_ASSERT_EQUAL_STRING(expected_fields[field_index], field.ptr);
        field_index++;
        current_row = NULL;
    }

    TEST_ASSERT_EQUAL_UINT(expected_field_count, field_index);
}

static void csv_assert_fixture_rows_view(const char* relative_path,
                                         const char* const* const* expected_rows,
                                         const size_t* expected_field_counts,
                                         size_t expected_row_count)
{
    CsvReader* reader = NULL;
    CsvStringView row = {0};
    size_t row_index = 0U;

    TEST_ASSERT_NOT_NULL(expected_rows);
    TEST_ASSERT_NOT_NULL(expected_field_counts);

    reader = csv_reader_open_fixture(relative_path);
    for (row_index = 0U; row_index < expected_row_count; ++row_index)
    {
        TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
        csv_assert_row_fields_view(reader,
                                   &row,
                                   expected_rows[row_index],
                                   expected_field_counts[row_index]);
    }

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));
    csv_reader_close(reader);
}

static const char* const fixture_simple_row0[] = {"a", "b", "c"};
static const char* const fixture_simple_row1[] = {"1", "2", "3"};
static const char* const* const fixture_simple_rows[] = {
    fixture_simple_row0,
    fixture_simple_row1,
};
static const size_t fixture_simple_counts[] = {3U, 3U};

static const char* const fixture_comma_in_quotes_row0[] = {
    "first", "last", "address", "city", "zip",
};
static const char* const fixture_comma_in_quotes_row1[] = {
    "John", "Doe", "120 any st.", "Anytown, WW", "08123",
};
static const char* const* const fixture_comma_in_quotes_rows[] = {
    fixture_comma_in_quotes_row0,
    fixture_comma_in_quotes_row1,
};
static const size_t fixture_comma_in_quotes_counts[] = {5U, 5U};

static const char* const fixture_escaped_quotes_row0[] = {"a", "b"};
static const char* const fixture_escaped_quotes_row1[] = {"1", "ha \"ha\" ha"};
static const char* const fixture_escaped_quotes_row2[] = {"3", "4"};
static const char* const* const fixture_escaped_quotes_rows[] = {
    fixture_escaped_quotes_row0,
    fixture_escaped_quotes_row1,
    fixture_escaped_quotes_row2,
};
static const size_t fixture_escaped_quotes_counts[] = {2U, 2U, 2U};

static const char* const fixture_newlines_row0[] = {"a", "b", "c"};
static const char* const fixture_newlines_row1[] = {"1", "2", "3"};
static const char* const fixture_newlines_row2[] = {"Once upon \na time", "5", "6"};
static const char* const fixture_newlines_row3[] = {"7", "8", "9"};
static const char* const* const fixture_newlines_rows[] = {
    fixture_newlines_row0,
    fixture_newlines_row1,
    fixture_newlines_row2,
    fixture_newlines_row3,
};
static const size_t fixture_newlines_counts[] = {3U, 3U, 3U, 3U};

static const char* const fixture_quotes_and_newlines_row0[] = {"a", "b"};
static const char* const fixture_quotes_and_newlines_row1[] = {
    "1", "ha \n\"ha\" \nha",
};
static const char* const fixture_quotes_and_newlines_row2[] = {"3", "4"};
static const char* const* const fixture_quotes_and_newlines_rows[] = {
    fixture_quotes_and_newlines_row0,
    fixture_quotes_and_newlines_row1,
    fixture_quotes_and_newlines_row2,
};
static const size_t fixture_quotes_and_newlines_counts[] = {2U, 2U, 2U};

void test_csv_reader_reads_crlf_rows(void)
{
    static const char* const row1_fields[] = {"left", "right"};
    static const char* const row2_fields[] = {"up", "down"};
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("left,right\r\nup,down\r\n",
                                                  &temp_file);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    TEST_ASSERT_EQUAL_STRING("left,right", row.ptr);
    csv_assert_row_fields_view(reader, &row, row1_fields, 2U);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    TEST_ASSERT_EQUAL_STRING("up,down", row.ptr);
    csv_assert_row_fields_view(reader, &row, row2_fields, 2U);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));
    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_matches_csv_spectrum_simple_fixture(void)
{
    csv_assert_fixture_rows_view("tests/fixtures/csv_spectrum/simple.csv",
                                 fixture_simple_rows,
                                 fixture_simple_counts,
                                 2U);
}

void test_csv_reader_matches_csv_spectrum_comma_in_quotes_fixture(void)
{
    csv_assert_fixture_rows_view("tests/fixtures/csv_spectrum/comma_in_quotes.csv",
                                 fixture_comma_in_quotes_rows,
                                 fixture_comma_in_quotes_counts,
                                 2U);
}

void test_csv_reader_matches_csv_spectrum_escaped_quotes_fixture(void)
{
    csv_assert_fixture_rows_view("tests/fixtures/csv_spectrum/escaped_quotes.csv",
                                 fixture_escaped_quotes_rows,
                                 fixture_escaped_quotes_counts,
                                 3U);
}

void test_csv_reader_matches_csv_spectrum_newlines_fixture(void)
{
    csv_assert_fixture_rows_view("tests/fixtures/csv_spectrum/newlines.csv",
                                 fixture_newlines_rows,
                                 fixture_newlines_counts,
                                 4U);
}

void test_csv_reader_matches_csv_spectrum_quotes_and_newlines_fixture(void)
{
    csv_assert_fixture_rows_view(
        "tests/fixtures/csv_spectrum/quotes_and_newlines.csv",
        fixture_quotes_and_newlines_rows,
        fixture_quotes_and_newlines_counts,
        3U);
}

void test_csv_reader_reads_doubled_quotes(void)
{
    static const char* const expected_fields[] = {"a\"b", "tail"};
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("\"a\"\"b\",tail\n",
                                                  &temp_file);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected_fields, 2U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_reads_escaped_quotes(void)
{
    static const char* const expected_fields[] = {"a\"b", "tail"};
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("\"a\\\"b\",tail\n",
                                                  &temp_file);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected_fields, 2U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_reports_parse_error_for_unterminated_quote(void)
{
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("a,\"unterminated\n",
                                                  &temp_file);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_PARSE_ERROR,
                          csv_reader_next_row(reader, &row));
    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_reports_parse_error_for_garbage_after_quote(void)
{
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("\"ok\"oops,tail\n",
                                                  &temp_file);
    CsvStringView row = {0};
    CsvStringView field = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_PARSE_ERROR,
                          csv_reader_next_col(reader, &row, &field));
    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_reports_missing_file(void)
{
    CsvReader* reader = NULL;

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_IO_ERROR,
                          csv_reader_open(&reader, "/tmp/csv_fast_missing.csv"));
    TEST_ASSERT_NULL(reader);
}

void test_csv_reader_reads_string_views(void)
{
    static const char* const expected_fields[] = {"alpha", "", "omega"};
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("alpha,,omega\n", &temp_file);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    TEST_ASSERT_EQUAL_UINT(strlen("alpha,,omega"), row.len);
    TEST_ASSERT_EQUAL_STRING("alpha,,omega", row.ptr);
    csv_assert_row_fields_view(reader, &row, expected_fields, 3U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_reads_trailing_empty_field_view(void)
{
    static const char* const expected_fields[] = {"a", ""};
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("a,\n", &temp_file);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected_fields, 2U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_reads_middle_empty_field(void)
{
    static const char* const expected_fields[] = {"a", "", "c"};
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("a,,c\n", &temp_file);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected_fields, 3U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_reads_only_empty_fields(void)
{
    static const char* const expected_fields[] = {"", "", ""};
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file(",,\n", &temp_file);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected_fields, 3U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_reads_quoted_comma_and_newline(void)
{
    static const char* const expected_fields[] = {"a", "b\nc", "d"};
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("a,\"b\nc\",d\n", &temp_file);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected_fields, 3U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_reads_last_row_without_trailing_newline(void)
{
    static const char* const expected_fields[] = {"left", "right"};
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("left,right", &temp_file);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected_fields, 2U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_empty_file_has_no_rows(void)
{
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("", &temp_file);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_empty_row_yields_one_empty_field(void)
{
    static const char* const expected_fields[] = {""};
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("\n", &temp_file);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    TEST_ASSERT_EQUAL_UINT(0U, row.len);
    csv_assert_row_fields_view(reader, &row, expected_fields, 1U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_empty_row_between_data_rows(void)
{
    static const char* const row1_fields[] = {"a"};
    static const char* const row2_fields[] = {""};
    static const char* const row3_fields[] = {"b"};
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("a\n\nb\n", &temp_file);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, row1_fields, 1U);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, row2_fields, 1U);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, row3_fields, 1U);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));
    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_rejects_delim_equals_quote(void)
{
    CsvReader* reader = NULL;
    CsvTempFile temp_file = {{0}};
    CsvOptions options = csv_options_default();

    csv_write_temp_file(&temp_file, "a,b\n");
    options.delim = '"';
    options.quote = '"';

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_INVALID_ARGUMENT,
                          csv_reader_open_with_options(&reader,
                                                      temp_file.path,
                                                      &options));
    TEST_ASSERT_NULL(reader);
    TEST_ASSERT_EQUAL_INT(0, unlink(temp_file.path));
}

void test_csv_reader_rejects_newline_delim(void)
{
    CsvReader* reader = NULL;
    CsvTempFile temp_file = {{0}};
    CsvOptions options = csv_options_default();

    csv_write_temp_file(&temp_file, "a,b\n");
    options.delim = '\n';

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_INVALID_ARGUMENT,
                          csv_reader_open_with_options(&reader,
                                                      temp_file.path,
                                                      &options));
    TEST_ASSERT_NULL(reader);
    TEST_ASSERT_EQUAL_INT(0, unlink(temp_file.path));
}

void test_csv_reader_rejects_null_quote(void)
{
    CsvReader* reader = NULL;
    CsvTempFile temp_file = {{0}};
    CsvOptions options = csv_options_default();

    csv_write_temp_file(&temp_file, "a,b\n");
    options.quote = '\0';

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_INVALID_ARGUMENT,
                          csv_reader_open_with_options(&reader,
                                                      temp_file.path,
                                                      &options));
    TEST_ASSERT_NULL(reader);
    TEST_ASSERT_EQUAL_INT(0, unlink(temp_file.path));
}

void test_csv_status_string_returns_name(void)
{
    TEST_ASSERT_EQUAL_STRING("CSV_STATUS_OK",
                             csv_status_string(CSV_STATUS_OK));
    TEST_ASSERT_EQUAL_STRING("CSV_STATUS_EOF",
                             csv_status_string(CSV_STATUS_EOF));
    TEST_ASSERT_EQUAL_STRING("CSV_STATUS_INVALID_ARGUMENT",
                             csv_status_string(CSV_STATUS_INVALID_ARGUMENT));
    TEST_ASSERT_EQUAL_STRING("CSV_STATUS_IO_ERROR",
                             csv_status_string(CSV_STATUS_IO_ERROR));
    TEST_ASSERT_EQUAL_STRING("CSV_STATUS_PARSE_ERROR",
                             csv_status_string(CSV_STATUS_PARSE_ERROR));
    TEST_ASSERT_EQUAL_STRING("CSV_STATUS_NO_MEMORY",
                             csv_status_string(CSV_STATUS_NO_MEMORY));
    TEST_ASSERT_EQUAL_STRING("CSV_STATUS_UNKNOWN",
                             csv_status_string((CsvStatus)99));
}
