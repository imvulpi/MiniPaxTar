#ifndef MPTARIO_H
#define MPTARIO_H

#include "minipaxtar.h"
#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #define MPTARIO_MAX_NAME 1024 // Accommodates Windows MAX_PATH (260 WCHARs) expanded to UTF-8
    #include <windows.h>
#else
    #define MPTARIO_MAX_NAME 255  // POSIX NAME_MAX limit is strictly 255 bytes
    #include <dirent.h>
    #include <errno.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup mptario_status_codes MPTARIO Status Codes
 * \brief Non-negative success and condition codes for mptario operations.
 * @{
 */
#define MPTARIO_OK                  0   /**< Operation completed successfully. */
/** @} */

/**
 * \defgroup mptario_error_codes MPTARIO Error Codes
 * \brief Negative error codes returned by mptario high-level archiving operations.
 * 
 * \note Mptario-specific error codes start at -100 to provide a safety buffer 
 *       against underlying minipaxtar error codes (-1 to -13).
 * @{
 */
/* Reused directly from minipaxtar for seamless interoperability */
#define MPTARIO_ERR_INVALID_ARG         MPTAR_ERR_INVALID_ARG        /**< Invalid argument or null pointer passed (-1). */
#define MPTARIO_ERR_BUFFER_TOO_SMALL    MPTAR_ERR_BUFFER_TOO_SMALL  /**< Output buffer lacked sufficient capacity (-13). */
#define MPTARIO_ERR_OUT_OF_MEMORY       MPTAR_ERR_ALLOC              /**< Memory allocation failed (-2). */

/* High-level Mptario-specific filesystem and engine errors */
#define MPTARIO_ERR_NOT_FOUND       -100 /**< Requested file or directory does not exist during iteration. */
#define MPTARIO_ERR_ACCESS_DENIED   -101 /**< Permission denied when accessing a file or directory. */
#define MPTARIO_ERR_IO              -102 /**< High-level system I/O or directory traversal failure. */
#define MPTARIO_ERR_CORRUPTED       -103 /**< Archive structure or metadata corruption detected by engine. */
/** @} */

/**
 * \brief Extra character length needed to append the directory search wildcard ("&#47;*").
 */
#define MPTARIO_SEARCH_SUFFIX_LEN 2 

/**
 * \brief Represents an uninitialized, invalid, or unknown file entry type.
 * 
 * Used during file info extraction and validation to ensure malformed 
 * or unrecognized directory entries are safely skipped or rejected.
 */
#define MPTARIO_UNKNOWN_TYPE -1

/**
 * \brief Maximum stack-allocated buffer length for UTF-8 / ANSI paths.
 * 
 * Set to 1024 bytes to accommodate typical deep file paths on both Windows 
 * and POSIX systems without triggering heap allocations. If a path exceeds 
 * this length, mptario gracefully falls back to the configured heap allocator.
 */
#define MPTARIO_STACK_PATH_LEN 1024

/**
 * \brief Maximum stack-allocated buffer length for Windows wide-character paths (`wchar_t`).
 * 
 * Set to 520 characters (equivalent to double MAX_PATH / Windows long path safety margin) 
 * to handle `CreateFileW` path conversions on the stack safely and efficiently.
 */
#define MPTARIO_STACK_WPATH_LEN 520

/**
 * \brief Represents the timestamp of the Unix Epoch (January 1, 1970) in Windows FILETIME units (100-ns ticks since January 1, 1601).  
 */
#define UNIX_EPOCH_INTERVALS   116444736000000000ULL

/**
 * \brief Default chunk/buffer size used for file read and write operations.
 *
 * Used internally when allocating a temporary I/O buffer if none is provided 
 * in the active write context (i.e., when the context buffer is set to `MPTAR_NULL`).
 */
#define MPTARIO_DEFAULT_CHUNK_SIZE 65536

/**
 * \defgroup win_refs_compat Windows ReFS and Compatibility Fallbacks
 * \brief Dynamic loading definitions and SDK fallbacks for 128-bit ReFS file IDs.
 *
 * These definitions are required because older Windows SDKs (prior to Windows 8)
 * do not define `FILE_ID_INFO` or `FileIdInfo`. Furthermore, `GetFileInformationByHandleEx` 
 * must be resolved dynamically via `GetProcAddress` to maintain runtime compatibility 
 * with Windows XP and Windows 7 while still supporting ReFS on newer Windows versions.
 * @{
 */
#if defined(_WIN32) && !defined(MPTARIO_NO_REFS)
/** \brief Function pointer type for dynamically loading GetFileInformationByHandleEx. */
typedef BOOL (WINAPI *PFN_GetFileInformationByHandleEx)(
    HANDLE hFile,
    int    FileInformationClass,
    LPVOID lpFileInformation,
    DWORD  dwBufferSize
);

#ifndef FileIdInfo
/** \brief File information class value for querying 128-bit file IDs (FileIdInfo = 18). */
#define FileIdInfo 18
#endif

#ifndef FILE_ID_INFO
/** \brief Fallback structure for 128-bit file identification when compiling with older Windows SDKs. */
typedef struct {
    ULONGLONG VolumeSerialNumber; /**< Volume serial number containing the file. */
    BYTE      Identifier[16];     /**< 128-bit unique file identifier byte array. */
} MPTAR_FILE_ID_INFO;
#define MPTAR_USING_FALLBACK_ID
#else
/** \brief Standard FILE_ID_INFO type alias. */
typedef FILE_ID_INFO MPTAR_FILE_ID_INFO;
#endif
#endif
/** @} */

/**
 * \brief Cross-platform unique file identifier used for hard link detection.
 */
typedef struct {
#if defined(_WIN32) && !defined(MPTARIO_NO_REFS)
    mptar_uint64 volume_serial; /**< Windows volume serial number. */
    mptar_uint64 low;           /**< Lower 64 bits of the file ID. */
    mptar_uint64 high;          /**< Upper 64 bits (used for 128-bit ReFS IDs, 0 on NTFS). */
#else
    /** 
     * \brief Shared 2-field layout for Windows (without ReFS) and POSIX.
     * 
     * - Windows: primary = volume serial, secondary = file index.
     * - POSIX: primary = device ID (st_dev), secondary = inode number (st_ino).
     */
    mptar_uint64 primary;       /**< Primary scope or device identifier. (volume serial)/(st_dev) */
    mptar_uint64 secondary;     /**< Secondary local file or inode identifier. (file index)/(st_ino) */
#endif
} mptario_file_id;

/**
 * \brief Registry entry tracking hard links for deduplication.
 */
typedef struct {
    mptario_file_id file_id; /**< Unique file identifier for cross-referencing. */
    char    *archive_path;   /**< Path of the file already recorded in the archive. */
} mptario_hardlink_entry;

typedef struct {
    mptario_hardlink_entry *entries;    // Pointer to the dynamic array / hash table buffer
    mptar_size_t            capacity;   // Total allocated slots
    mptar_size_t            count;      // Number of currently tracked files
} mptario_hardlink_registry;

/**
 * Holds metadata attributes for a single file system node.
 */
typedef struct {
    mptar_typeflags type;         /**< Broad classification of the file system entry. */
    mptario_file_id file_id;      /**< Unique file/entry identifier. Values might depend on system - see: \ref mptario_file_id. */
    mptar_uint64 size;            /**< Total size in bytes; zero or unassigned for directories. */
    mptar_int32 hardlink_count;   /**< Number of hard links referencing this file node. */
    mptar_int32 attributes;       /**< Platform-specific bitmask of raw attribute flags. */
    mptar_opt_time mtime;         /**< Modification time. */
#ifndef MPTAR_NO_EXTRA_TIMES
    mptar_opt_time atime;         /**< Access time. */
    mptar_opt_time ctime;         /**< Metadata change time / creation time. */
#endif
    mptar_uint32 mode;            /**< UNIX file permissions / mode. */
} mptario_fileinfo;

/**
 * \brief Archive operation context.
 * 
 * Bundles the underlying `minipaxtar` writer configuration with a reusable 
 * streaming chunk buffer to optimize throughput and eliminate per-file allocations.
 */
typedef struct {
    mptar_writer *writer;   /**< Pointer to the underlying minipaxtar stream writer configuration. */
    char *buf;              /**< Optional pre-allocated or engine-managed I/O chunk buffer. */
    mptar_size_t buf_size;  /**< Size of the chunk buffer in bytes. */
    mptario_hardlink_registry* hardlink_registry; /**< Registry for hardlink tracking. Initialize this to NULL. */
} mptario_write_context;

/**
 * \brief Opaque state structure for directory iteration without heap allocations.
 */
typedef struct {
#if defined(_WIN32)
    HANDLE handle;          /**< Win32 directory search handle. */
    WIN32_FIND_DATAW fdata; /**< Win32 file search data buffer. */
    int is_first;           /**< Flag indicating if the first entry is pending evaluation. */
#else
    DIR *dir_ptr;           /**< POSIX stream pointer for directory entry reading. */
#endif
} mptario_dir;

/**
 * \brief Minimal directory entry representation containing only the child name.
 */
typedef struct {
    char name[MPTARIO_MAX_NAME]; /**< Null-terminated UTF-8 entry name relative to parent directory. */
} mptario_dirent;

#ifdef MPTARIO_CUSTOM_ALLOC

/**
 * \brief Allocates memory directly from the operating system kernel. Uses VirtualAlloc on Windows and mmap on POSIX.
 *
 * \param[in] size Size of memory block to allocate in bytes.
 * \return Pointer to allocated memory block, or NULL on failure.
 */
void* mptario_alloc(mptar_size_t size);

/**
 * \brief Frees OS-allocated memory pages. Uses VirtualFree on Windows and munmap on POSIX.
 * On POSIX, requires allocation size prepended to the allocation block.
 *
 * \param[in] pointer Pointer to memory block previously returned by mptario_alloc.
 */
void mptario_free(void* pointer);

/**
 * \name Default Memory Allocation Wrappers
 * \brief Standard allocation wrappers matching the \ref mptar_alloc_fn and \ref mptar_free_fn signatures.
 * \details These wrappers allow standard allocation functions to plug directly into \ref mptar_memory_cfg,
 *          safely handling the unused user data context.
 * @{
 */

/**
 * \brief Allocation wrapper function matching \ref mptar_alloc_fn.
 * \param user_data Opaque user pointer (ignored by this default wrapper).
 * \param size Number of bytes to allocate.
 * \return Pointer to the allocated memory block, or NULL on failure.
 */
void* mptario_alloc_wrapper(void* user_data, mptar_size_t size);

/**
 * \brief Deallocation wrapper function matching \ref mptar_free_fn.
 * \param user_data Opaque user pointer (ignored by this default wrapper).
 * \param ptr Pointer to the memory block to free.
 */
void mptario_free_wrapper(void* user_data, void* ptr);

/** @} */

#endif

#if defined(_WIN32) && !defined(MPTARIO_NO_REFS)
/**
 * \brief Initializes runtime function pointers for 128-bit ReFS support on Windows.
 *
 * Resolves `GetFileInformationByHandleEx` dynamically via `GetProcAddress` 
 * to enable 128-bit file ID retrieval on ReFS volumes while preserving runtime 
 * compatibility with older Windows versions (XP and 7).
 */
void mptario_init_refs(void);
#else
/**
 * \brief No-op macro for platforms or build configurations without ReFS support.
 *
 * Evaluates to `((void)0)` on POSIX systems or when `MPTARIO_NO_REFS` is defined, 
 * eliminating runtime overhead and dead code.
 */
#define mptario_init_refs() ((void)0)
#endif

/**
 * \brief Opens a directory stream for entry iteration.
 *
 * On Windows, prepares wildcards and invokes \c FindFirstFileW. On POSIX, opens 
 * a stream using \c opendir.
 *
 * \param[in]  path Null-terminated UTF-8 path to the directory.
 * \param[out] dir  Pointer to caller-allocated \ref mptario_dir structure.
 *
 * \return \c MPTARIO_OK on success.
 * \return \c MPTARIO_ERR_INVALID_PARAM if \p path or \p dir is NULL.
 * \return \c MPTARIO_ERR_NOT_FOUND if directory path does not exist.
 * \return \c MPTARIO_ERR_ACCESS_DENIED if lacking read permissions.
 * \return \c MPTAR_ERR_OUT_OF_MEMORY if OS ran out of memory allocating wpath.
 * \return \c MPTARIO_ERR_IO on general OS filesystem errors.
 */
int mptario_dir_open(const char *path, mptario_dir *dir, mptar_memory_cfg *mem);

/**
 * \brief Reads the next entry from an open directory stream.
 *
 * Automatically filters out the '.' and '..' pseudo-directory entries. Decodes
 * entry names to UTF-8 on Windows. This function does not query or populate 
 * detailed entry file types/attributes; use \ref mptario_get_fileinfo on 
 * concatenated paths when metadata is needed.
 *
 * \param[in]  dir     Pointer to an initialized \ref mptario_dir instance.
 * \param[out] out_ent Pointer to caller-allocated \ref mptario_dirent structure receiving entry name.
 *
 * \return \c MPTARIO_OK when an entry is successfully read.
 * \return \c MPTARIO_ERR_NOT_FOUND when all directory entries have been enumerated.
 * \return \c MPTARIO_ERR_INVALID_PARAM if \p dir or \p out_ent is NULL.
 */
int mptario_dir_next(mptario_dir *dir, mptario_dirent *out_ent);

/**
 * \brief Closes an open directory stream and releases OS handle resources.
 *
 * Safely closes \c HANDLE on Windows or releases \c DIR pointer on POSIX systems.
 * Safe to call on NULL or uninitialized directory structures.
 *
 * \param[in,out] dir Pointer to the \ref mptario_dir instance to close.
 */
void mptario_dir_close(mptario_dir *dir);

/**
 * Retrieves metadata and classification information for a specified file system entry.
 *
 * \param path Pointer to a null-terminated UTF-16 encoded path string.
 * \param out_info Pointer to a destination structure that receives the entry's metadata.
 *
 * \return Returns 0 on success, or a non-zero error code on failure.
 */
int mptario_get_fileinfo(const char *path, mptario_fileinfo *out_info, mptar_memory_cfg *mem);

/**
 * \brief Recursively packs a filesystem directory stream using an initialized mptar_writer.
 *
 * If the I/O buffer within the provided write context is set to `MPTAR_NULL`, 
 * this function automatically allocates a temporary buffer of size `MPTARIO_DEFAULT_CHUNK_SIZE` 
 * for streaming data, and ensures it is safely freed before returning. If a pre-allocated 
 * buffer is already provided in the context, it will be used directly without any additional allocation 
 * and will not be freed by this function.
 *
 * \param ctx Active archive writer context configured with write callback and user data.
 * \param path Path to the source directory to read from the host system.
 * \return MPTAR_OK on success, or an error code on failure.
 */
int mptario_pack_stream(mptario_write_context* ctx, const char* path);

#ifdef __cplusplus
}
#endif

#endif /* MPTARIO_H */