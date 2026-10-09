#include "mptario.h"

#ifdef MPTARIO_CUSTOM_ALLOC

void* mptario_alloc(mptar_size_t size){
    if (size == 0) return MPTAR_NULL;

#if defined (_WIN32)
    return VirtualAlloc(MPTAR_NULL, (mptar_size_t)size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    mptar_size_t total_size = size + sizeof(mptar_size_t);

    void* ptr = mmap(MPTAR_NULL, total_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr == MAP_FAILED) {
        return MPTAR_NULL;
    }

    *(mptar_size_t*)ptr = total_size;
    return (void*)((char*)ptr + sizeof(mptar_size_t));
#endif
}

void mptario_free(void* pointer) {
    if (!pointer) return;
#if defined (_WIN32)
    VirtualFree(pointer, 0, MEM_RELEASE);
#else
    char* raw_ptr = (char*)pointer - sizeof(mptar_size_t);
    mptar_size_t total_size = *(mptar_size_t*)raw_ptr;
    munmap(raw_ptr, total_size);
#endif
}

#endif