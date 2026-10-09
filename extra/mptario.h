#ifndef MPTARIO_H
#define MPTARIO_H

#include "minipaxtar.h"

#ifdef __cplusplus
extern "C" {
#endif

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

#endif

#ifdef __cplusplus
}
#endif

#endif /* MPTARIO_H */