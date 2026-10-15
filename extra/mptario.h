#ifndef MPTARIO_H
#define MPTARIO_H

#include "minipaxtar.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MPTARIO_OK                    1   /**< Operation completed successfully. */
#define MPTARIO_ERR_INVALID_ARG      -1   /**< Arguments passed were incorrect (NULL pointers, not enough space, etc.) */
#define MPTARIO_ERR_BUFFER_TOO_SMALL -2   /**< Output or destination buffer lacked sufficient capacity to hold some data. */

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

#ifdef __cplusplus
}
#endif

#endif /* MPTARIO_H */