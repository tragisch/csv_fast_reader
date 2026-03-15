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

#if defined(__APPLE__) || defined(__linux__) || defined(__unix__)
#define CSV_FAST_PLATFORM_POSIX
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if defined(__aarch64__)
#include <arm_neon.h>
#define CSV_SIMD_SEARCH
#define CSV_SIMD_WIDTH 16U
#elif defined(__amd64__) || defined(_M_AMD64)
#include <emmintrin.h>
#define CSV_SIMD_SEARCH
#define CSV_SIMD_WIDTH 16U
#endif

#include "csv.h"

/* Windows specific */
#ifdef _WIN32
#include <Windows.h>
typedef unsigned long long file_off_t;
#elif defined(CSV_FAST_PLATFORM_POSIX)
#include <sys/types.h>
typedef off_t file_off_t;
#else
#error csv_fast currently supports macOS and Linux builds only
#endif

/* max allowed buffer */
#define BUFFER_WIDTH_APROX (40 * 1024 * 1024)
#define CSV_AUXBUF_MIN_CAPACITY 256U

#if defined (__aarch64__) || defined (__amd64__) || defined (_M_AMD64)
/* unpack csv newline search */
#define CSV_UNPACK_64_SEARCH
#endif

/* private csv handle:
 * @mem: pointer to memory
 * @pos: position in buffer
 * @size: size of memory chunk
 * @context: context used when processing cols
 * @blockSize: size of mapped block
 * @fileSize: size of opened file
 * @mapSize: ...
 * @auxbuf: auxiliary buffer
 * @auxbufSize: size of aux buffer
 * @auxbufPos: position of aux buffer reader
 * @quotes: number of pending quotes parsed
 * @escapePending: whether next scanned byte is escaped within quoted data
 * @fh: file handle - descriptor
 * @delim: delimeter - ','
 * @quote: quote '"'
 * @escape: escape char
 */
struct CsvHandle_
{
    void* mem;
    size_t pos;
    size_t size;
    char* context;
    size_t blockSize;
    file_off_t fileSize;
    file_off_t mapSize;
    size_t auxbufSize;
    size_t auxbufPos;
    size_t quotes;
    int escapePending;
    int pendingEmptyCol;
    CsvStatus lastStatus;
    void* auxbuf;
    
#if defined(CSV_FAST_PLATFORM_POSIX)
    int fh;
#elif defined ( _WIN32 )
    HANDLE fh;
    HANDLE fm;
#else
    #error csv_fast currently supports macOS and Linux builds only
#endif

    char delim;
    char quote;
    char escape;
    uint8_t charClass[256];
};

static void CsvInitCharClass(CsvReader *handle)
{
    memset(handle->charClass, 0, sizeof(handle->charClass));
    handle->charClass[(unsigned char)'\n'] = 1;
    handle->charClass[(unsigned char)handle->quote] = 1;
    handle->charClass[(unsigned char)handle->escape] = 1;
}

static CsvStatus CsvReaderOpenInternal(CsvReader **out_reader,
                                       const char *filename,
                                       char delim,
                                       char quote,
                                       char escape);

const char *csv_status_string(CsvStatus status)
{
    switch (status) {
    case CSV_STATUS_OK:               return "CSV_STATUS_OK";
    case CSV_STATUS_EOF:              return "CSV_STATUS_EOF";
    case CSV_STATUS_INVALID_ARGUMENT: return "CSV_STATUS_INVALID_ARGUMENT";
    case CSV_STATUS_IO_ERROR:         return "CSV_STATUS_IO_ERROR";
    case CSV_STATUS_PARSE_ERROR:      return "CSV_STATUS_PARSE_ERROR";
    case CSV_STATUS_NO_MEMORY:        return "CSV_STATUS_NO_MEMORY";
    }
    return "CSV_STATUS_UNKNOWN";
}

CsvOptions csv_options_default(void)
{
    CsvOptions options;

    options.delim = ',';
    options.quote = '"';
    options.escape = '\\';
    return options;
}

CsvStatus csv_reader_open(CsvReader **out_reader, const char *filename)
{
    const CsvOptions options = csv_options_default();

    return csv_reader_open_with_options(out_reader, filename, &options);
}

CsvStatus csv_reader_open_with_options(CsvReader **out_reader,
                                       const char *filename,
                                       const CsvOptions *options)
{
    if (!out_reader || !filename || !options)
        return CSV_STATUS_INVALID_ARGUMENT;

    *out_reader = NULL;

    /* Reject characters that would break the parser's assumptions. */
    if (options->delim == options->quote ||
        options->delim == '\n' || options->delim == '\r' || options->delim == '\0' ||
        options->quote  == '\n' || options->quote  == '\r' || options->quote  == '\0' ||
        options->escape == '\n' || options->escape == '\r' || options->escape == '\0')
        return CSV_STATUS_INVALID_ARGUMENT;

    return CsvReaderOpenInternal(out_reader,
                                 filename,
                                 options->delim,
                                 options->quote,
                                 options->escape);
}

/* trivial macro used to get page-aligned buffer size */
#define GET_PAGE_ALIGNED( orig, page ) \
    ((((size_t)(orig)) + (((size_t)(page)) - 1U)) & ~(((size_t)(page)) - 1U))

/* thin platform dependent layer so we can use file mapping
 * with winapi and oses following posix specs.
 */
#if defined(CSV_FAST_PLATFORM_POSIX)
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static CsvStatus CsvReaderOpenInternal(CsvReader **out_reader,
                                       const char *filename,
                                       char delim,
                                       char quote,
                                       char escape)
{
    /* alloc zero-initialized mem */
    long pageSize;
    struct stat fs;
    CsvStatus status = CSV_STATUS_IO_ERROR;

    CsvReader *handle = calloc(1, sizeof(struct CsvHandle_));
    if (!handle)
        return CSV_STATUS_NO_MEMORY;

    /* set chars */
    handle->delim = delim;
    handle->quote = quote;
    handle->escape = escape;
    handle->lastStatus = CSV_STATUS_OK;
    CsvInitCharClass(handle);

    /* page size */
    pageSize = sysconf(_SC_PAGESIZE);
    if (pageSize < 0)
        goto fail;

    /* align to system page size */
    handle->blockSize = GET_PAGE_ALIGNED(BUFFER_WIDTH_APROX, pageSize);
    
    /* open new fd */
    handle->fh = open(filename, O_RDONLY);
    if (handle->fh < 0)
    {
        status = CSV_STATUS_IO_ERROR;
        goto fail;
    }

    /* get real file size */
    if (fstat(handle->fh, &fs))
    {
        close(handle->fh);
        goto fail;
    }
    
    handle->fileSize = fs.st_size;
    *out_reader = handle;
    return CSV_STATUS_OK;

  fail:
    free(handle);
    return status;
}

static void* MapMem(CsvReader *handle)
{
    void *p = mmap(0, handle->blockSize,
                   PROT_READ | PROT_WRITE,
                   MAP_PRIVATE,
                   handle->fh, handle->mapSize);
    if (p == MAP_FAILED)
        p = NULL;
    else
        madvise(p, handle->blockSize, MADV_SEQUENTIAL);

    handle->mem = p;
    return handle->mem;
}

static void UnmapMem(CsvReader *handle)
{
    if (handle->mem)
        munmap(handle->mem, handle->blockSize);
}

void csv_reader_close(CsvReader *handle)
{
    if (!handle)
        return;

    UnmapMem(handle);

    close(handle->fh);
    free(handle->auxbuf);
    free(handle);
}

#elif defined(_WIN32)

/* extra Windows specific implementations
 */
static CsvStatus CsvReaderOpenInternal(CsvReader **out_reader,
                                       const char *filename,
                                       char delim,
                                       char quote,
                                       char escape)
{
    LARGE_INTEGER fsize;
    SYSTEM_INFO info;
    CsvReader *handle = calloc(1, sizeof(struct CsvHandle_));
    if (!handle)
        return CSV_STATUS_NO_MEMORY;

    handle->delim = delim;
    handle->quote = quote;
    handle->escape = escape;
    handle->lastStatus = CSV_STATUS_OK;
    CsvInitCharClass(handle);

    GetSystemInfo(&info);
    handle->blockSize = GET_PAGE_ALIGNED(BUFFER_WIDTH_APROX, info.dwPageSize);
    handle->fh = CreateFile(filename, 
                            GENERIC_READ, 
                            FILE_SHARE_READ, 
                            NULL, 
                            OPEN_EXISTING, 
                            FILE_ATTRIBUTE_NORMAL, 
                            NULL);

    if (handle->fh == INVALID_HANDLE_VALUE)
        goto fail;

    if (GetFileSizeEx(handle->fh, &fsize) == FALSE)
        goto fail;

    handle->fileSize = fsize.QuadPart;
    if (!handle->fileSize)
        goto fail;

    handle->fm = CreateFileMapping(handle->fh, NULL, PAGE_WRITECOPY, 0, 0, NULL);
    if (handle->fm == NULL)
        goto fail;

    *out_reader = handle;
    return CSV_STATUS_OK;

fail:
    if (handle->fh != INVALID_HANDLE_VALUE)
        CloseHandle(handle->fh);

    free(handle);
    return CSV_STATUS_IO_ERROR;
}

static void* MapMem(CsvReader *handle)
{
    size_t size = handle->blockSize;
    if (handle->mapSize + size > handle->fileSize)
        size = 0;  /* last chunk, extend to file mapping max */

    handle->mem = MapViewOfFileEx(handle->fm, 
                                  FILE_MAP_COPY,
                                  (DWORD)(handle->mapSize >> 32),
                                  (DWORD)(handle->mapSize & 0xFFFFFFFF),
                                  size,
                                  NULL);
    return handle->mem;
}

static void UnmapMem(CsvReader *handle)
{
    if (handle->mem)
        UnmapViewOfFileEx(handle->mem, 0);
}

void csv_reader_close(CsvReader *handle)
{
    if (!handle)
        return;

    UnmapMem(handle);

    CloseHandle(handle->fm);
    CloseHandle(handle->fh);
    free(handle->auxbuf);
    free(handle);
}

#endif

static int CsvEnsureMapped(CsvReader *handle)
{
    file_off_t blockSize = 0;
    file_off_t newSize;
    
    /* do not need to map */
    if (handle->pos < handle->size)
        return 0;

    UnmapMem(handle);  

    handle->mem = NULL;
    if (handle->mapSize >= handle->fileSize)
        return -EINVAL;

    blockSize = (file_off_t)handle->blockSize;
    newSize = handle->mapSize + blockSize;
    if (MapMem(handle))
    {
        handle->pos = 0;
        handle->mapSize = newSize;

        /* read only up to filesize:
         * 1. mapped block size is < then filesize: (use blocksize)
         * 2. mapped block size is > then filesize: (use remaining filesize) */
        handle->size = handle->blockSize;
        if (handle->mapSize > handle->fileSize)
            handle->size = (size_t)(handle->fileSize % blockSize);
        
        return 0;
    }
    
    return -ENOMEM;
}

static size_t CsvNextCapacity(size_t current, size_t required)
{
    size_t capacity = current;

    if (capacity < CSV_AUXBUF_MIN_CAPACITY)
        capacity = CSV_AUXBUF_MIN_CAPACITY;

    while (capacity < required)
        capacity *= 2U;

    return capacity;
}

static char* CsvChunkToAuxBuf(CsvReader *handle, char* p, size_t size)
{
    size_t newSize = handle->auxbufPos + size + 1;
    if (handle->auxbufSize < newSize)
    {
        size_t newCapacity = CsvNextCapacity(handle->auxbufSize, newSize);
        void* mem = realloc(handle->auxbuf, newCapacity);
        if (!mem)
            return NULL;

        handle->auxbuf = mem;
        handle->auxbufSize = newCapacity;
    }

    memcpy((char*)handle->auxbuf + handle->auxbufPos, p, size);
    handle->auxbufPos += size;
    
    *(char*)((char*)handle->auxbuf + handle->auxbufPos) = '\0';
    return handle->auxbuf;
}

static CsvStatus CsvSetStatus(CsvReader *handle, CsvStatus status)
{
    if (handle)
        handle->lastStatus = status;

    return status;
}

static size_t CsvTerminateLine(char* line, size_t size)
{
    /* we do support standard POSIX LF sequence
     * and Windows CR LF sequence.
     * old non POSIX Mac OS CR is not supported.
     */
    char* res = line + size - 1;
    if (size >= 2 && res[-1] == '\r')
        --res;

    *res = 0;
    return (size_t)(res - line);
}

static inline int CsvRowScanInsideQuotes(const CsvReader *handle)
{
    return (int)((handle->quotes & 1U) != 0U);
}

static inline void CsvResetRowScanState(CsvReader *handle)
{
    if (!handle)
        return;

    handle->quotes = 0U;
    handle->escapePending = 0;
}

static inline char* CsvScanByte(char* p, CsvReader *handle)
{
    unsigned char ch = (unsigned char)*p;

    if (handle->escapePending)
    {
        handle->escapePending = 0;
        return NULL;
    }

    /* Fast exit for the overwhelming majority of bytes */
    if (!handle->charClass[ch])
        return NULL;

    if (CsvRowScanInsideQuotes(handle) && ch == (unsigned char)handle->escape)
    {
        handle->escapePending = 1;
        return NULL;
    }

    if (ch == (unsigned char)handle->quote)
    {
        handle->quotes++;
        return NULL;
    }

    if (ch == '\n' && !CsvRowScanInsideQuotes(handle))
        return p;

    return NULL;
}

static inline char* CsvSearchLfScalar(char* p, char* end, CsvReader *handle)
{
    for (; p < end; ++p)
    {
        char* found = CsvScanByte(p, handle);
        if (found)
            return found;
    }

    return NULL;
}

#ifdef CSV_SIMD_SEARCH
static inline int CsvSimdBlockNeedsDetailedScan(const char* p,
                                                const CsvReader *handle)
{
#if defined(__aarch64__)
    uint8x16_t block = vld1q_u8((const uint8_t*)p);
    uint8x16_t mask = vceqq_u8(block, vdupq_n_u8((uint8_t)'\n'));

    mask = vorrq_u8(mask,
                    vceqq_u8(block, vdupq_n_u8((uint8_t)handle->quote)));

    if (CsvRowScanInsideQuotes(handle))
    {
        mask = vorrq_u8(mask,
                        vceqq_u8(block, vdupq_n_u8((uint8_t)handle->escape)));
    }

    return (int)(vmaxvq_u8(mask) != 0U);
#else
    __m128i block = _mm_loadu_si128((const __m128i*)p);
    __m128i mask = _mm_cmpeq_epi8(block, _mm_set1_epi8('\n'));

    mask = _mm_or_si128(mask,
                        _mm_cmpeq_epi8(block, _mm_set1_epi8(handle->quote)));

    if (CsvRowScanInsideQuotes(handle))
    {
        mask = _mm_or_si128(mask,
                            _mm_cmpeq_epi8(block, _mm_set1_epi8(handle->escape)));
    }

    return _mm_movemask_epi8(mask) != 0;
#endif
}
#endif

#ifdef CSV_UNPACK_64_SEARCH
static inline uint64_t CsvRepeatByte(unsigned char byte)
{
    return 0x0101010101010101ULL * (uint64_t)byte;
}

static inline int CsvWordHasByte(uint64_t word, unsigned char byte)
{
    uint64_t x = word ^ CsvRepeatByte(byte);
    return (int)(((x - 0x0101010101010101ULL) & ~x & 0x8080808080808080ULL) != 0ULL);
}

static inline int CsvWordNeedsDetailedScan(uint64_t word, const CsvReader *handle)
{
    if (CsvWordHasByte(word, (unsigned char)'\n'))
        return 1;

    if (CsvWordHasByte(word, (unsigned char)handle->quote))
        return 1;

    if (CsvRowScanInsideQuotes(handle) &&
        CsvWordHasByte(word, (unsigned char)handle->escape))
        return 1;

    return 0;
}

static inline char* CsvSearchLfWord(char* p, CsvReader *handle)
{
    size_t offset = 0U;

    for (offset = 0U; offset < sizeof(uint64_t); ++offset)
    {
        char* found = CsvScanByte(p + offset, handle);
        if (found)
            return found;
    }

    return NULL;
}
#endif


static char* CsvSearchLf(char* p, size_t size, CsvReader *handle)
{
    /* TODO: this can be greatly optimized by
     * using modern SIMD instructions, but for now
     * we only fetch 8Bytes "at once"
     */
    char* end = p + size;

#ifdef CSV_SIMD_SEARCH
    char* simdEnd = p + (size & ~(CSV_SIMD_WIDTH - 1U));

    for (; p < simdEnd; p += CSV_SIMD_WIDTH)
    {
        char* found = NULL;

        if (!handle->escapePending && !CsvSimdBlockNeedsDetailedScan(p, handle))
            continue;

        found = CsvSearchLfScalar(p, p + CSV_SIMD_WIDTH, handle);
        if (found)
            return found;
    }
#endif

#ifdef CSV_UNPACK_64_SEARCH
    char* wordEnd = p + (((size_t)(end - p)) & ~(sizeof(uint64_t) - 1U));

    for (; p < wordEnd; p += sizeof(uint64_t))
    {
        uint64_t word = 0ULL;
        char* found = NULL;

        memcpy(&word, p, sizeof(word));
        if (!handle->escapePending && !CsvWordNeedsDetailedScan(word, handle))
            continue;

        found = CsvSearchLfWord(p, handle);
        if (found)
            return found;
    }
#endif

    return CsvSearchLfScalar(p, end, handle);
}

CsvStatus csv_reader_next_row(CsvReader *handle, CsvStringView *out_row)
{
    size_t size;
    size_t rowLen = 0U;
    char* p = NULL;
    char* found = NULL;

    if (!out_row)
        return CSV_STATUS_INVALID_ARGUMENT;

    out_row->ptr = NULL;
    out_row->len = 0U;

    if (!handle)
        return CSV_STATUS_INVALID_ARGUMENT;

    handle->context = NULL;
    handle->pendingEmptyCol = 0;
    CsvResetRowScanState(handle);

    do
    {
        int err = CsvEnsureMapped(handle);
        handle->context = NULL;
        handle->pendingEmptyCol = 0;
        
        if (err == -EINVAL)
        {
            /* if this is n-th iteration
             * return auxbuf (remaining bytes of the file) */
            if (p == NULL)
                break;

            if (CsvRowScanInsideQuotes(handle))
                return CsvSetStatus(handle, CSV_STATUS_PARSE_ERROR);

            CsvResetRowScanState(handle);
            out_row->ptr = (char*)handle->auxbuf;
            out_row->len = handle->auxbufPos;
            return CsvSetStatus(handle, CSV_STATUS_OK);
        }
        else if (err == -ENOMEM)
        {
            CsvSetStatus(handle, CSV_STATUS_NO_MEMORY);
            break;
        }
        
        size = handle->size - handle->pos;
        if (!size)
            break;

        /* search this chunk for NL */
        p = (char*)handle->mem + handle->pos;
        found = CsvSearchLf(p, size, handle);

        if (found)
        {
            /* prepare position for next iteration */
            size = (size_t)(found - p) + 1;
            handle->pos += size;
            CsvResetRowScanState(handle);
            
            if (handle->auxbufPos)
            {
                if (!CsvChunkToAuxBuf(handle, p, size))
                    break;
                
                p = handle->auxbuf;
                size = handle->auxbufPos;
            }

            rowLen = CsvTerminateLine(p, size);

            /* reset auxbuf position */
            handle->auxbufPos = 0;

            out_row->ptr = p;
            out_row->len = rowLen;
            return CsvSetStatus(handle, CSV_STATUS_OK);
        }
        else
        {
            /* reset on next iteration */
            handle->pos = handle->size;
        }

        /* correctly process boundries, storing
         * remaning bytes in aux buffer */
        if (!CsvChunkToAuxBuf(handle, p, size))
        {
            CsvSetStatus(handle, CSV_STATUS_NO_MEMORY);
            break;
        }

    } while (!found);

    if (handle->lastStatus == CSV_STATUS_NO_MEMORY)
        return CSV_STATUS_NO_MEMORY;

    return CsvSetStatus(handle, CSV_STATUS_EOF);
}

CsvStatus csv_reader_next_col(CsvReader *handle,
                              const CsvStringView *row,
                              CsvStringView *out_col)
{
    /* return properly escaped CSV col
     * RFC: [https://tools.ietf.org/html/rfc4180]
     */
    int trailingDelim = 0;
    char* p = NULL;
    char* d = NULL; /* destination */
    char* b = NULL; /* begin */
    int atEnd = 0;
    int quoted = 0; /* idicates quoted string */

    if (!handle || !out_col)
        return CSV_STATUS_INVALID_ARGUMENT;

    out_col->ptr = NULL;
    out_col->len = 0U;

    if (handle->pendingEmptyCol && handle->context && *handle->context == '\0')
    {
        handle->pendingEmptyCol = 0;
        out_col->ptr = handle->context;
        out_col->len = 0U;
        return CsvSetStatus(handle, CSV_STATUS_OK);
    }

    if (!row && !handle->context)
    {
        return CsvSetStatus(handle, CSV_STATUS_INVALID_ARGUMENT);
    }

    p = handle->context ? handle->context : row->ptr;
    d = p;
    b = p;

    /* An empty row (len == 0) still contains one empty field.
     * Guard: only on the very first column call (!handle->context). */
    if (*p == '\0' && !handle->context)
    {
        out_col->ptr = p;
        out_col->len = 0U;
        handle->context = p;
        return CsvSetStatus(handle, CSV_STATUS_OK);
    }

    quoted = *p == handle->quote;
    if (quoted) {
        p++;
    } else {
        /* Fast path: unquoted fields without escapes (the common case).
         * Scan forward until delimiter, NUL, quote, or escape. */
        const char delim = handle->delim;
        const char quote = handle->quote;
        const char esc = handle->escape;

        while (*p != delim && *p && *p != quote && *p != esc)
            p++;

        if (*p == quote)
            return CsvSetStatus(handle, CSV_STATUS_PARSE_ERROR);

        if (*p != esc) {
            /* Entire field scanned without escapes — skip generic loop */
            d = p;
            goto field_done;
        }
        /* Escape found — fall through to generic loop at current position */
        d = p;
    }

    for (; *p; p++, d++)
    {
        /* double quote is present if (1) */
        int dq = 0;
        int escaped = 0;

        if (!quoted && *p == handle->quote)
            return CsvSetStatus(handle, CSV_STATUS_PARSE_ERROR);
        
        /* skip escape */
        if (*p == handle->escape && p[1])
        {
            escaped = 1;
            p++;
        }

        /* skip double-quote */
        if (!escaped && *p == handle->quote && p[1] == handle->quote)
        {
            dq = 1;
            p++;
        }

        /* check if we should end */
        if (quoted && !dq && !escaped)
        {
            if (*p == handle->quote)
                break;
        }
        else if (!escaped && *p == handle->delim)
        {
            break;
        }

        /* copy if required */
        if (d != p)
            *d = *p;
    }

field_done:
    atEnd = (*p == '\0');
    *d = '\0';
    
    if (atEnd)
    {
        /* nothing to do */
        if (p == b)
        {
            return CsvSetStatus(handle, CSV_STATUS_EOF);
        }

        handle->context = p;
        handle->pendingEmptyCol = 0;
    }
    else
    {
        /* end reached, skip */
        *d = '\0';
        if (quoted)
        {
            for (p++; *p; p++)
            {
                if (*p == handle->delim)
                {
                    trailingDelim = 1;
                    break;
                }

                return CsvSetStatus(handle, CSV_STATUS_PARSE_ERROR);
            }

            if (*p)
                p++;
            
            handle->context = p;
            handle->pendingEmptyCol = trailingDelim && (*handle->context == '\0');
        }
        else
        {
            handle->context = p + 1;
            handle->pendingEmptyCol = (*handle->context == '\0');
        }
    }

    out_col->ptr = b;
    out_col->len = (size_t)(d - b);
    return CsvSetStatus(handle, CSV_STATUS_OK);
}
