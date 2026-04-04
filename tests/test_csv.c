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

static CsvReader* csv_reader_open_temp_file_with_options(const char* contents,
                                                         CsvTempFile* temp_file,
                                                         const CsvOptions* options)
{
    CsvReader* reader = NULL;

    csv_write_temp_file(temp_file, contents);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_reader_open_with_options(&reader,
                                                       temp_file->path,
                                                       options));
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
    size_t field_index = 0U;
    CsvStatus status = CSV_STATUS_OK;

    TEST_ASSERT_NOT_NULL(reader);
    TEST_ASSERT_NOT_NULL(row);
    TEST_ASSERT_NOT_NULL(expected_fields);

    status = csv_reader_first_col(reader, row, &field);
    while (status == CSV_STATUS_OK)
    {
        TEST_ASSERT_LESS_THAN_UINT(expected_field_count, field_index);
        TEST_ASSERT_EQUAL_UINT(strlen(expected_fields[field_index]), field.len);
        TEST_ASSERT_EQUAL_STRING(expected_fields[field_index], field.ptr);
        field_index++;
        status = csv_reader_next_col_in_row(reader, &field);
    }

    TEST_ASSERT_EQUAL_UINT(expected_field_count, field_index);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, status);
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

void test_csv_reader_reads_empty_quoted_field(void)
{
    static const char* const expected_fields[] = {"", "tail"};
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("\"\",tail\n", &temp_file);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected_fields, 2U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_reads_trailing_empty_field_after_quoted_field(void)
{
    static const char* const expected_fields[] = {"a", ""};
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("\"a\",\n", &temp_file);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected_fields, 2U);
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

void test_csv_reader_rejects_escape_equals_quote(void)
{
    CsvReader* reader = NULL;
    CsvTempFile temp_file = {{0}};
    CsvOptions options = csv_options_default();

    csv_write_temp_file(&temp_file, "a,b\n");
    options.escape = '"';

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_INVALID_ARGUMENT,
                          csv_reader_open_with_options(&reader,
                                                      temp_file.path,
                                                      &options));
    TEST_ASSERT_NULL(reader);
    TEST_ASSERT_EQUAL_INT(0, unlink(temp_file.path));
}

void test_csv_reader_next_col_requires_active_row_context(void)
{
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("a,b\n", &temp_file);
    CsvStringView field = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_INVALID_ARGUMENT,
                          csv_reader_next_col(reader, NULL, &field));

    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_next_col_in_row_requires_started_row(void)
{
    CsvTempFile temp_file = {{0}};
    CsvReader* reader = csv_reader_open_temp_file("a,b\n", &temp_file);
    CsvStringView field = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_INVALID_ARGUMENT,
                          csv_reader_next_col_in_row(reader, &field));

    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_reads_semicolon_delimited_rows(void)
{
    static const char* const expected_fields[] = {"left", "middle", "right"};
    CsvTempFile temp_file = {{0}};
    CsvOptions options = csv_options_default();
    CsvReader* reader = NULL;
    CsvStringView row = {0};

    options.delim = ';';
    reader = csv_reader_open_temp_file_with_options("left;middle;right\n",
                                                    &temp_file,
                                                    &options);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected_fields, 3U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_reads_single_quote_quoted_fields(void)
{
    static const char* const expected_fields[] = {"a", "b,c", "d"};
    CsvTempFile temp_file = {{0}};
    CsvOptions options = csv_options_default();
    CsvReader* reader = NULL;
    CsvStringView row = {0};

    options.quote = '\'';
    reader = csv_reader_open_temp_file_with_options("a,'b,c',d\n",
                                                    &temp_file,
                                                    &options);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected_fields, 3U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close_temp_file(reader, &temp_file);
}

void test_csv_reader_reads_custom_escape_character(void)
{
    static const char* const expected_fields[] = {"a\"b", "tail"};
    CsvTempFile temp_file = {{0}};
    CsvOptions options = csv_options_default();
    CsvReader* reader = NULL;
    CsvStringView row = {0};

    options.escape = '!';
    reader = csv_reader_open_temp_file_with_options("\"a!\"b\",tail\n",
                                                    &temp_file,
                                                    &options);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected_fields, 2U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close_temp_file(reader, &temp_file);
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

/* ------------------------------------------------------------------ */
/* Stream-based reader tests                                          */
/* ------------------------------------------------------------------ */

static FILE* csv_stream_from_string(const char* contents)
{
    FILE* stream = tmpfile();
    size_t length;

    TEST_ASSERT_NOT_NULL(stream);
    length = strlen(contents);
    if (length > 0)
        TEST_ASSERT_EQUAL_UINT64(length,
                                 fwrite(contents, 1U, length, stream));
    rewind(stream);
    return stream;
}

static CsvReader* csv_reader_open_from_stream(const char* contents,
                                              FILE** out_stream)
{
    CsvReader* reader = NULL;

    *out_stream = csv_stream_from_string(contents);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_reader_open_stream(&reader, *out_stream));
    TEST_ASSERT_NOT_NULL(reader);
    return reader;
}

void test_csv_stream_reads_simple_rows(void)
{
    static const char* const row1_fields[] = {"a", "b", "c"};
    static const char* const row2_fields[] = {"1", "2", "3"};
    FILE* stream = NULL;
    CsvReader* reader = csv_reader_open_from_stream("a,b,c\n1,2,3\n",
                                                    &stream);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, row1_fields, 3U);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, row2_fields, 3U);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close(reader);
    fclose(stream);
}

void test_csv_stream_reads_quoted_delimiter(void)
{
    static const char* const expected[] = {"a", "b,c", "d"};
    FILE* stream = NULL;
    CsvReader* reader = csv_reader_open_from_stream("a,\"b,c\",d\n",
                                                    &stream);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected, 3U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close(reader);
    fclose(stream);
}

void test_csv_stream_reads_embedded_newlines(void)
{
    static const char* const expected[] = {"a", "b\nc", "d"};
    FILE* stream = NULL;
    CsvReader* reader = csv_reader_open_from_stream("a,\"b\nc\",d\n",
                                                    &stream);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected, 3U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close(reader);
    fclose(stream);
}

void test_csv_stream_reads_trailing_empty_field(void)
{
    static const char* const expected[] = {"a", ""};
    FILE* stream = NULL;
    CsvReader* reader = csv_reader_open_from_stream("a,\n", &stream);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected, 2U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close(reader);
    fclose(stream);
}

void test_csv_stream_reads_crlf(void)
{
    static const char* const row1[] = {"left", "right"};
    static const char* const row2[] = {"up", "down"};
    FILE* stream = NULL;
    CsvReader* reader = csv_reader_open_from_stream("left,right\r\nup,down\r\n",
                                                    &stream);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, row1, 2U);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, row2, 2U);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close(reader);
    fclose(stream);
}

void test_csv_stream_empty_returns_eof(void)
{
    FILE* stream = NULL;
    CsvReader* reader = csv_reader_open_from_stream("", &stream);
    CsvStringView row = {0};

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close(reader);
    fclose(stream);
}

void test_csv_stream_rejects_null_arguments(void)
{
    CsvReader* reader = NULL;
    FILE* stream = tmpfile();

    TEST_ASSERT_NOT_NULL(stream);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_INVALID_ARGUMENT,
                          csv_reader_open_stream(NULL, stream));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_INVALID_ARGUMENT,
                          csv_reader_open_stream(&reader, NULL));
    TEST_ASSERT_NULL(reader);

    fclose(stream);
}

void test_csv_stream_close_does_not_close_stream(void)
{
    FILE* stream = tmpfile();
    CsvReader* reader = NULL;
    int ch;

    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_EQUAL_UINT64(3U, fwrite("hi\n", 1U, 3U, stream));
    rewind(stream);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_reader_open_stream(&reader, stream));
    TEST_ASSERT_NOT_NULL(reader);
    csv_reader_close(reader);

    /* Stream must still be usable after csv_reader_close */
    rewind(stream);
    ch = fgetc(stream);
    TEST_ASSERT_EQUAL_INT('h', ch);

    fclose(stream);
}

void test_csv_stream_with_options(void)
{
    static const char* const expected[] = {"a", "b;c", "d"};
    FILE* stream = NULL;
    CsvReader* reader = NULL;
    CsvOptions options = csv_options_default();
    CsvStringView row = {0};

    options.delim = ';';
    stream = csv_stream_from_string("a;\"b;c\";d\n");
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_reader_open_stream_with_options(&reader,
                                                              stream,
                                                              &options));
    TEST_ASSERT_NOT_NULL(reader);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    csv_assert_row_fields_view(reader, &row, expected, 3U);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close(reader);
    fclose(stream);
}

void test_csv_stream_reads_long_row_across_chunks(void)
{
    /* Build a single field larger than the internal stream buffer (256 KB)
     * so that the row spans at least two fread() calls. */
    const size_t field_len = 300000U;
    char* csv_data = NULL;
    char* expected_field = NULL;
    FILE* stream = NULL;
    CsvReader* reader = NULL;
    CsvStringView row = {0};
    CsvStringView field = {0};
    size_t i;

    csv_data = malloc(field_len + 4);
    expected_field = malloc(field_len + 1);
    TEST_ASSERT_NOT_NULL(csv_data);
    TEST_ASSERT_NOT_NULL(expected_field);

    for (i = 0; i < field_len; ++i)
    {
        char ch = 'A' + (char)(i % 26);
        csv_data[i] = ch;
        expected_field[i] = ch;
    }
    expected_field[field_len] = '\0';
    csv_data[field_len] = '\n';
    csv_data[field_len + 1] = '\0';

    stream = tmpfile();
    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_EQUAL_UINT64(field_len + 1,
                             fwrite(csv_data, 1U, field_len + 1, stream));
    rewind(stream);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_reader_open_stream(&reader, stream));
    TEST_ASSERT_NOT_NULL(reader);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_reader_next_row(reader, &row));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_reader_first_col(reader, &row, &field));
    TEST_ASSERT_EQUAL_UINT(field_len, field.len);
    TEST_ASSERT_EQUAL_MEMORY(expected_field, field.ptr, field_len);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF, csv_reader_next_row(reader, &row));

    csv_reader_close(reader);
    fclose(stream);
    free(csv_data);
    free(expected_field);
}

/* ================================================================== */
/* Writer tests                                                       */
/* ================================================================== */

void test_csv_writer_writes_simple_rows(void)
{
    CsvTempFile temp_file = {{0}};
    CsvWriter* writer = NULL;
    FILE* fp = NULL;
    char buf[256] = {0};

    (void)snprintf(temp_file.path, sizeof(temp_file.path),
                   "/tmp/csv_fast_w_XXXXXX");
    int fd = mkstemp(temp_file.path);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, fd);
    close(fd);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_writer_open(&writer, temp_file.path));
    TEST_ASSERT_NOT_NULL(writer);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "a"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "b"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "c"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_end_row(writer));

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "1"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "2"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "3"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_end_row(writer));

    csv_writer_close(writer);

    fp = fopen(temp_file.path, "rb");
    TEST_ASSERT_NOT_NULL(fp);
    size_t n = fread(buf, 1, sizeof(buf) - 1, fp);
    buf[n] = '\0';
    fclose(fp);

    TEST_ASSERT_EQUAL_STRING("a,b,c\n1,2,3\n", buf);
    TEST_ASSERT_EQUAL_INT(0, unlink(temp_file.path));
}

void test_csv_writer_quotes_delimiter_in_field(void)
{
    FILE* stream = tmpfile();
    CsvWriter* writer = NULL;
    char buf[256] = {0};

    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_writer_open_stream(&writer, stream));

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "a,b"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "c"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_end_row(writer));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_flush(writer));

    rewind(stream);
    size_t n = fread(buf, 1, sizeof(buf) - 1, stream);
    buf[n] = '\0';

    TEST_ASSERT_EQUAL_STRING("\"a,b\",c\n", buf);

    csv_writer_close(writer);
    fclose(stream);
}

void test_csv_writer_quotes_newline_in_field(void)
{
    FILE* stream = tmpfile();
    CsvWriter* writer = NULL;
    char buf[256] = {0};

    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_writer_open_stream(&writer, stream));

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "line1\nline2"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_end_row(writer));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_flush(writer));

    rewind(stream);
    size_t n = fread(buf, 1, sizeof(buf) - 1, stream);
    buf[n] = '\0';

    TEST_ASSERT_EQUAL_STRING("\"line1\nline2\"\n", buf);

    csv_writer_close(writer);
    fclose(stream);
}

void test_csv_writer_escapes_quote_in_field(void)
{
    FILE* stream = tmpfile();
    CsvWriter* writer = NULL;
    char buf[256] = {0};

    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_writer_open_stream(&writer, stream));

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_writer_field_str(writer, "say \"hello\""));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_end_row(writer));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_flush(writer));

    rewind(stream);
    size_t n = fread(buf, 1, sizeof(buf) - 1, stream);
    buf[n] = '\0';

    TEST_ASSERT_EQUAL_STRING("\"say \"\"hello\"\"\"\n", buf);

    csv_writer_close(writer);
    fclose(stream);
}

void test_csv_writer_writes_empty_fields(void)
{
    FILE* stream = tmpfile();
    CsvWriter* writer = NULL;
    char buf[256] = {0};

    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_writer_open_stream(&writer, stream));

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field(writer, NULL, 0));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, ""));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "x"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_end_row(writer));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_flush(writer));

    rewind(stream);
    size_t n = fread(buf, 1, sizeof(buf) - 1, stream);
    buf[n] = '\0';

    TEST_ASSERT_EQUAL_STRING(",,x\n", buf);

    csv_writer_close(writer);
    fclose(stream);
}

void test_csv_writer_custom_delimiter(void)
{
    FILE* stream = tmpfile();
    CsvWriter* writer = NULL;
    CsvOptions options = csv_options_default();
    char buf[256] = {0};

    TEST_ASSERT_NOT_NULL(stream);
    options.delim = ';';
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_writer_open_stream_with_options(&writer, stream,
                                                              &options));

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "a"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "b;c"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_end_row(writer));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_flush(writer));

    rewind(stream);
    size_t n = fread(buf, 1, sizeof(buf) - 1, stream);
    buf[n] = '\0';

    TEST_ASSERT_EQUAL_STRING("a;\"b;c\"\n", buf);

    csv_writer_close(writer);
    fclose(stream);
}

void test_csv_writer_rejects_null_arguments(void)
{
    CsvWriter* writer = NULL;
    FILE* stream = tmpfile();

    TEST_ASSERT_NOT_NULL(stream);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_INVALID_ARGUMENT,
                          csv_writer_open(NULL, "/tmp/csv_nope.csv"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_INVALID_ARGUMENT,
                          csv_writer_open(&writer, NULL));
    TEST_ASSERT_NULL(writer);

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_INVALID_ARGUMENT,
                          csv_writer_open_stream(NULL, stream));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_INVALID_ARGUMENT,
                          csv_writer_open_stream(&writer, NULL));
    TEST_ASSERT_NULL(writer);

    fclose(stream);
}

void test_csv_writer_rejects_invalid_options(void)
{
    CsvWriter* writer = NULL;
    CsvOptions options = csv_options_default();

    options.delim = '"';
    options.quote = '"';
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_INVALID_ARGUMENT,
                          csv_writer_open_with_options(&writer,
                                                       "/tmp/csv_nope.csv",
                                                       &options));
    TEST_ASSERT_NULL(writer);
}

void test_csv_writer_close_null_is_noop(void)
{
    csv_writer_close(NULL);
    /* If we get here without crashing, the test passed */
}

void test_csv_writer_roundtrip_with_reader(void)
{
    CsvTempFile temp_file = {{0}};
    CsvWriter* writer = NULL;
    CsvReader* reader = NULL;
    CsvStringView row = {0};

    (void)snprintf(temp_file.path, sizeof(temp_file.path),
                   "/tmp/csv_fast_rt_XXXXXX");
    int fd = mkstemp(temp_file.path);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(0, fd);
    close(fd);

    /* Write */
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_writer_open(&writer, temp_file.path));

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "name"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "city"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_end_row(writer));

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "Alice"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "New York, NY"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_end_row(writer));

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_writer_field_str(writer, "Bob \"The Builder\""));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "LA"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_end_row(writer));

    csv_writer_close(writer);

    /* Read back & verify */
    {
        static const char* const hdr[] = {"name", "city"};
        static const char* const r1[]  = {"Alice", "New York, NY"};
        static const char* const r2[]  = {"Bob \"The Builder\"", "LA"};

        TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                              csv_reader_open(&reader, temp_file.path));

        TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                              csv_reader_next_row(reader, &row));
        csv_assert_row_fields_view(reader, &row, hdr, 2U);

        TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                              csv_reader_next_row(reader, &row));
        csv_assert_row_fields_view(reader, &row, r1, 2U);

        TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                              csv_reader_next_row(reader, &row));
        csv_assert_row_fields_view(reader, &row, r2, 2U);

        TEST_ASSERT_EQUAL_INT(CSV_STATUS_EOF,
                              csv_reader_next_row(reader, &row));

        csv_reader_close(reader);
    }

    TEST_ASSERT_EQUAL_INT(0, unlink(temp_file.path));
}

void test_csv_writer_stream_not_closed(void)
{
    FILE* stream = tmpfile();
    CsvWriter* writer = NULL;
    int ch;

    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_writer_open_stream(&writer, stream));

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_field_str(writer, "hi"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_end_row(writer));
    csv_writer_close(writer);

    /* Stream must still be usable after csv_writer_close */
    rewind(stream);
    ch = fgetc(stream);
    TEST_ASSERT_EQUAL_INT('h', ch);

    fclose(stream);
}

void test_csv_writer_quotes_cr_in_field(void)
{
    FILE* stream = tmpfile();
    CsvWriter* writer = NULL;
    char buf[256] = {0};

    TEST_ASSERT_NOT_NULL(stream);
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_writer_open_stream(&writer, stream));

    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK,
                          csv_writer_field_str(writer, "a\rb"));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_end_row(writer));
    TEST_ASSERT_EQUAL_INT(CSV_STATUS_OK, csv_writer_flush(writer));

    rewind(stream);
    size_t n = fread(buf, 1, sizeof(buf) - 1, stream);
    buf[n] = '\0';

    TEST_ASSERT_EQUAL_STRING("\"a\rb\"\n", buf);

    csv_writer_close(writer);
    fclose(stream);
}
