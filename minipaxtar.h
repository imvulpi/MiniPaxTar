#define MPTAR_SUPPORT_EXTRA_TIMES

#ifdef MPTAR_NO_STD
    typedef unsigned char mptar_uint8;
    typedef signed char mptar_int8;
    
    #if defined(__INT_MAX__) && (__INT_MAX__ == 32767)
        typedef unsigned int mptar_uint16;
        typedef signed int mptar_int16;
    #else
        typedef unsigned short mptar_uint16;
        typedef signed short mptar_int16;
    #endif

    #if defined(__INT_MAX__) && (__INT_MAX__ == 2147483647)
        typedef unsigned int mptar_uint32;
        typedef signed int mptar_int32;
    #else
        typedef unsigned long mptar_uint32;
        typedef signed long mptar_int32;
    #endif

    typedef unsigned long long mptar_uint64;
    typedef signed long long mptar_int64;

    #if defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ == 8) || defined(_WIN64)
        typedef unsigned long long mptar_size_t;
    #elif defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ == 2) 
        typedef unsigned short mptar_size_t;
    #else
        typedef unsigned long mptar_size_t;
    #endif

    #define MPTAR_NULL ((void*)0)

    #define MPTAR_INT64_MAX  (9223372036854775807LL)
    #define MPTAR_INT64_MIN  (-MPTAR_INT64_MAX - 1LL)
    #define MPTAR_UINT64_MAX (0xFFFFFFFFFFFFFFFFULL)
#ifndef bool
    typedef _Bool bool;
    #define true  1
    #define false 0
#endif

#else
    #include <stdint.h>
    #include <stddef.h>
    #include <stdbool.h>

    typedef int64_t mptar_int64;
    typedef int32_t mptar_int32;
    typedef int16_t mptar_int16;
    typedef int8_t  mptar_int8;

    typedef uint64_t mptar_uint64;
    typedef uint32_t mptar_uint32;
    typedef uint16_t mptar_uint16;
    typedef uint8_t  mptar_uint8;
    typedef size_t   mptar_size_t;
    
    #define MPTAR_NULL       NULL

    #define MPTAR_INT64_MIN  INT64_MIN
    #define MPTAR_INT64_MAX  INT64_MAX
    #define MPTAR_UINT64_MAX UINT64_MAX
#endif

#define MPTAR_OK                       0   /* Operation completed successfully */
#define MPTAR_EOF                      1   /* End of Archive reached (two consecutive null blocks) */
#define MPTAR_NEEDS_PAX                2   /* Item cannot fit in standard USTAR and requires a PAX extended header. */

#define MPTAR_ERR_INVALID_ARG         -1   /* NULL context pointers or zero-length arguments passed */
#define MPTAR_ERR_ALLOC               -2   /* Memory allocation failed or returned an invalid pointer */
#define MPTAR_ERR_IO_READ             -3   /* Read callback failed to return the requested number of bytes */
#define MPTAR_ERR_IO_WRITE            -4   /* Write callback failed to write the complete chunk to storage */
#define MPTAR_ERR_CHECKSUM            -5   /* Header checksum verification failed; record is corrupted */
#define MPTAR_ERR_UNSUPPORTED_TYPE    -6   /* Encountered an explicit special file type (FIFO, Block, Char) not supported */
#define MPTAR_ERR_WRITE_OVERFLOW      -7   /* Requested write size exceeds remaining available bytes_left for current file payload */
#define MPTAR_ERR_INCOMPLETE_PAYLOAD  -8   /* mptar_write_finalize called while bytes_left is still greater than 0 */
#define MPTAR_ERR_MALFORMED           -9   /* Archive format anomaly or structural corruption was detected. */
#define MPTAR_ERR_OVERFLOW            -10  /* An arithmetic wrap-around or internal buffer write boundary violation detected */
#define MPTAR_ERR_PATH_TOO_LONG       -11  /* File path exceeds the native physical limits of a standard USTAR block. */

#define MPTAR_PAX_KEY_LEN_PATH   4
#define MPTAR_PAX_KEY_LEN_SIZE   4
#define MPTAR_PAX_KEY_LEN_LINK   8
#define MPTAR_PAX_KEY_LEN_TIME   5
#define MPTAR_PAX_KEY_LEN_UID    3 
#define MPTAR_PAX_KEY_LEN_GID    3 
#define MPTAR_PAX_KEY_LEN_UNAME  5 
#define MPTAR_PAX_KEY_LEN_GNAME  5 

#define MPTAR_PAX_STATIC_CHARS   3 // Space (' '), Equals ('='), and Newline ('\n')

#define MPTAR_PAX_OVERHEAD_PATH  (MPTAR_PAX_KEY_LEN_PATH + MPTAR_PAX_STATIC_CHARS)  // 7
#define MPTAR_PAX_OVERHEAD_SIZE  (MPTAR_PAX_KEY_LEN_SIZE + MPTAR_PAX_STATIC_CHARS)  // 7
#define MPTAR_PAX_OVERHEAD_LINK  (MPTAR_PAX_KEY_LEN_LINK + MPTAR_PAX_STATIC_CHARS)  // 11
#define MPTAR_PAX_OVERHEAD_TIME  (MPTAR_PAX_KEY_LEN_TIME + MPTAR_PAX_STATIC_CHARS)  // 8
#define MPTAR_PAX_OVERHEAD_UID   (MPTAR_PAX_KEY_LEN_UID   + MPTAR_PAX_STATIC_CHARS) // 6
#define MPTAR_PAX_OVERHEAD_GID   (MPTAR_PAX_KEY_LEN_GID   + MPTAR_PAX_STATIC_CHARS) // 6
#define MPTAR_PAX_OVERHEAD_UNAME (MPTAR_PAX_KEY_LEN_UNAME + MPTAR_PAX_STATIC_CHARS) // 8
#define MPTAR_PAX_OVERHEAD_GNAME (MPTAR_PAX_KEY_LEN_GNAME + MPTAR_PAX_STATIC_CHARS) // 8

#define MPTAR_USTAR_SIZE_LINKNAME 100
#define MPTAR_USTAR_SIZE_NAME 100
#define MPTAR_USTAR_SIZE_PREFIX 155
#define MPTAR_USTAR_SIZE_UNAME 32
#define MPTAR_USTAR_SIZE_GNAME 32
#define MPTAR_USTAR_MAX_LEN_NAME     (MPTAR_USTAR_SIZE_NAME - 1)     // 99
#define MPTAR_USTAR_MAX_LEN_LINKNAME (MPTAR_USTAR_SIZE_LINKNAME - 1) // 99
#define MPTAR_USTAR_MAX_LEN_PREFIX   (MPTAR_USTAR_SIZE_PREFIX - 1)   // 154
#define MPTAR_USTAR_MAX_LEN_UNAME    (MPTAR_USTAR_SIZE_UNAME - 1)    // 31
#define MPTAR_USTAR_MAX_LEN_GNAME    (MPTAR_USTAR_SIZE_GNAME - 1)    // 31

#define MPTAR_USTAR_MAX_OCTAL_12B 8589934591ULL
#define MPTAR_USTAR_MAX_OCTAL_8B  2097151ULL
#define MPTAR_BINARY_MAX_8B       MPTAR_INT64_MAX

#define MPTAR_PAX_HAS_PATH  (1 << 0)
#define MPTAR_PAX_HAS_LINK  (1 << 1)
#define MPTAR_PAX_HAS_SIZE  (1 << 2)
#define MPTAR_PAX_HAS_MTIME (1 << 3)
#define MPTAR_PAX_HAS_UID   (1 << 4)
#define MPTAR_PAX_HAS_GID   (1 << 5)
#define MPTAR_PAX_HAS_ATIME (1 << 6)
#define MPTAR_PAX_HAS_CTIME (1 << 7)
#define MPTAR_PAX_HAS_UNAME (1 << 8)
#define MPTAR_PAX_HAS_GNAME (1 << 9)

typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12]; 
    char checksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
} mptar_header;

typedef struct {
    mptar_int64 sec;   // Standard Unix timestamp (seconds)
    mptar_uint32 nsec; // Nanoseconds (0 to 999,999,999) for subsecond precision
} mptar_timespec;

typedef struct {
    mptar_timespec value;
    bool has_value;
} mptar_opt_time;

typedef struct {
    mptar_opt_time mtime;
#ifdef MPTAR_SUPPORT_EXTRA_TIMES
    mptar_opt_time atime; // Access time
    mptar_opt_time ctime; // Change time
#endif

    mptar_uint64 size;
    mptar_uint64 uid;
    mptar_uint64 gid;

    const char* path;
    const char* link_target;
    const char* uname;
    const char* gname;

    mptar_uint32 mode;
#ifdef MPTAR_SUPPORT_SPECIAL
    mptar_uint32 dev_minor;
    mptar_uint32 dev_major;
#endif

    char typeflag;
} mptar_metadata;

typedef void* (*mptar_alloc_fn)(void* user_data, mptar_size_t size);
typedef void (*mptar_free_fn)(void* user_data, void* ptr);

typedef struct {
    mptar_alloc_fn alloc;
    mptar_free_fn free;
    void* alloc_user_data;
} mptar_memory_cfg;

#ifndef MPTAR_WITHOUT_READ

typedef mptar_size_t (*mptar_read_fn)(void* user_data, void* buffer, mptar_size_t size);

typedef struct {
    mptar_uint64 bytes_left;
    mptar_uint64 offset;
    mptar_memory_cfg memory;
    mptar_read_fn read;
    void* read_user_data;
} mptar_reader;

int mptar_read_header(mptar_reader* reader, mptar_metadata* out_meta);
mptar_size_t mptar_read_data_chunk(mptar_reader* reader, void* buffer, mptar_size_t size, int *out_err);
int mptar_skip_data(mptar_reader* reader);

#endif /* MPTAR_WITHOUT_READ */

#ifndef MPTAR_WITHOUT_WRITE

typedef mptar_size_t (*mptar_write_fn)(void* user_data, const void* buffer, mptar_size_t size);

typedef struct {
    mptar_uint64 bytes_left;
    mptar_memory_cfg memory;
    mptar_write_fn write;
    void* write_user_data;
    bool allow_pax_for_octal;
} mptar_writer;

int mptar_write_header(mptar_writer* ctx, const mptar_metadata* meta);
mptar_size_t mptar_write_data_chunk(mptar_writer* ctx, const void* buffer, mptar_size_t size, int *out_err);
int mptar_write_finalize(mptar_writer* ctx, const mptar_metadata* meta);
int mptar_close_archive(mptar_writer *ctx);

#endif /* MPTAR_WITHOUT_WRITE */