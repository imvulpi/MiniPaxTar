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

void *mptario_alloc_wrapper(void *user_data, mptar_size_t size)
{
    (void)user_data;
    return mptario_alloc(size);
}

void mptario_free_wrapper(void *user_data, void *ptr)
{
    (void)user_data;
    return mptario_free(ptr);
}

#endif

int mptario_path_join(const char *path1, const char *path2, char *out_path, mptar_size_t buf_size){
    if(!path1 || !path2 || !out_path || buf_size <= 1) return MPTARIO_ERR_INVALID_ARG;

    mptar_size_t path1_len = mptar_strlen(path1);
    mptar_size_t path2_len = mptar_strlen(path2);
    if(path1_len >= buf_size || path2_len >= buf_size) return MPTARIO_ERR_BUFFER_TOO_SMALL;

    bool ending_separator = (path1_len > 0) && (path1[path1_len-1] == '\\' || path1[path1_len-1] == '/');
    mptar_size_t total_len = path1_len + (ending_separator ? 0 : 1) + path2_len;

    if(total_len >= buf_size) return MPTARIO_ERR_BUFFER_TOO_SMALL;
    
    mptar_memcpy(out_path, path1, path1_len);
    mptar_size_t offset = path1_len;

    if(!ending_separator && path1_len > 0){
        out_path[offset++] = '/';
    }

    mptar_memcpy(out_path + offset, path2, path2_len);
    offset += path2_len;
    out_path[offset] = '\0';

    for(mptar_size_t i = 0; i < offset; i++){
        if(out_path[i] == '\\') {
            out_path[i] = '/';
        }
    }

    return MPTARIO_OK;
}