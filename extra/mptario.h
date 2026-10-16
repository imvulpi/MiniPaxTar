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

#define MPTARIO_OK                    0   /**< Operation completed successfully. */
#define MPTARIO_ERR_INVALID_ARG      -1   /**< Arguments passed were incorrect (NULL pointers, not enough space, etc.) */
#define MPTARIO_ERR_BUFFER_TOO_SMALL -2   /**< Output or destination buffer lacked sufficient capacity to hold some data. */
/**
 * \name Directory Iteration Error Codes
 * \brief Standard error codes returned by MPTARIO operations.
 * @{
 */
#define MPTARIO_ERR_NOT_FOUND       -3 /**< Requested file or directory does not exist. */
#define MPTARIO_ERR_ACCESS_DENIED   -4 /**< Permission denied for the file or directory. */
#define MPTARIO_ERR_IO              -5 /**< Low-level I/O or system operation failure. */
#define MPTAR_ERR_OUT_OF_MEMORY     -6 /**< Memory allocation failed. */
/** @} */
#define MPTARIO_ERR_CORRUPTED       -7

/**
 * \brief Extra character length needed to append the directory search wildcard ("&#47;*").
 */
#define MPTARIO_SEARCH_SUFFIX_LEN 2 

#define MPTARIO_STACK_WPATH_LEN 520 /**<  Generous stack buffer (covers standard Windows MAX_PATH + wildcard) */
/**
 * \brief Represents an uninitialized, invalid, or unknown file entry type.
 * 
 * Used during file info extraction and validation to ensure malformed 
 * or unrecognized directory entries are safely skipped or rejected.
 */
#define MPTARIO_UNKNOWN_TYPE -1


/**
 * \brief Represents the timestamp of the Unix Epoch (January 1, 1970) in Windows FILETIME units (100-ns ticks since January 1, 1601).  
 */
#define UNIX_EPOCH_INTERVALS   116444736000000000ULL

/**
 * Holds metadata attributes for a single file system node.
 */
typedef struct {
    mptar_typeflags type;         /**< Broad classification of the file system entry. */
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

/**
 * \brief Safely joins two paths into a single UTF-8 path string.
 *
 * Appends a path separator ('/') between \p path1 and \p path2 if \p path1 does
 * not already end with a forward slash or backslash.
 *
 * \param[in]  path1     Null-terminated UTF-8 string representing parent directory path.
 * \param[in]  path2     Null-terminated UTF-8 string representing child entry name.
 * \param[out] out_buf   Destination stack buffer receiving concatenated path.
 * \param[in]  buf_size  Total byte capacity of \p out_buf (including null terminator).
 *
 * \return \c MPTARIO_OK on success.
 * \return \c MPTARIO_ERR_INVALID_PARAM if inputs are NULL or \p buf_size is insufficient.
 * \return \c MPTARIO_ERR_BUFFER_TOO_SMALL if \p out_buf is too small to hold the joined path. 
 */
int mptario_path_join(const char *path1, const char *path2, char *out_buf, mptar_size_t buf_size);

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
 * \param writer Active archive writer context configured with write callback and user data.
 * \param path Path to the source directory to read from the host system.
 * \return MPTAR_OK on success, or an error code on failure.
 */
int mptario_pack_stream(mptar_writer* writer, const char* path);

#ifdef __cplusplus
}
#endif

#endif /* MPTARIO_H */