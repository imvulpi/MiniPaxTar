#ifdef MPTAR_NO_STD
    typedef unsigned char mptar_uint8;
    
    #if defined(__INT_MAX__) && (__INT_MAX__ == 32767)
        typedef unsigned int mptar_uint16;
    #else
        typedef unsigned short mptar_uint16;
    #endif

    #if defined(__INT_MAX__) && (__INT_MAX__ == 2147483647)
        typedef unsigned int mptar_uint32;
    #else
        typedef unsigned long mptar_uint32;
    #endif

    typedef unsigned long long mptar_uint64;

    #if defined(__SIZEOF_POINTER__) && (__SIZEOF_POINTER__ == 8)
        typedef unsigned long long mptar_size_t;
    #else
        typedef unsigned long mptar_size_t;
    #endif

    #define MPTAR_NULL ((void*)0)
#else
    #include <stdint.h>
    #include <stddef.h>
    
    typedef uint64_t mptar_uint64;
    typedef uint32_t mptar_uint32;
    typedef uint16_t mptar_uint16;
    typedef uint8_t  mptar_uint8;
    typedef size_t   mptar_size_t;
    
    #define MPTAR_NULL NULL
#endif

typedef struct {
    const char* path;     // Unlimited length in memory
    mptar_uint64 size;        // True 64-bit size
    mptar_uint32 mode;        // Octal permissions
    mptar_uint32 uid;         // Numeric Owner ID
    mptar_uint32 gid;         // Numeric Group ID
    mptar_uint64 mtime;       // Modification Timestamp
    char typeflag;        // '0' = file, '5' = directory
} mptar_metadata;

typedef mptar_size_t (*mptar_write_fn)(void* user_data, const void* buffer, mptar_size_t size);
typedef struct {
    mptar_write_fn write_fn;
    void* user_data;
    mptar_uint64 bytes_left;
} mptar_write_ctx;

// STANDARD HEADER:
// Octal strings have to be null/space terminated.
typedef struct {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char modtime[12]; 
    char checksum[8]; // Sum of all of the bytes (unsighed chars) of the header written into 6-digit octal and a \0 with a space.
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
} tar_header;

// PAX:
// Pax supports some keywords: path and size.
// When a file triggers a PAX requirement it follows this:
// Line Length + " " + KEYWORD + "=" + VALUE + "\n"
// Where:
//  Line length is the length of the line including the length of the characters defining the length.

int mptar_write_header(mptar_write_ctx* write_ctx, const mptar_metadata* meta);
int mptar_write_data_chunk(mptar_write_ctx* write_ctx, const void* buffer, mptar_size_t size);
int mptar_write_finalize(mptar_write_ctx* write_ctx, const mptar_metadata* meta);