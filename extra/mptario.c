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

int mptario_dir_open(const char *path, mptario_dir *dir, mptar_memory_cfg *mem) {
    if (!path || !dir) {
        return MPTARIO_ERR_INVALID_ARG;
    }

#if defined(_WIN32)
    int required_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (required_len <= 0) return MPTARIO_ERR_INVALID_ARG;

    size_t alloc_chars = (size_t)required_len + MPTARIO_SEARCH_SUFFIX_LEN;
    wchar_t stack_wpath[MPTARIO_STACK_WPATH_LEN];
    wchar_t *wpath = NULL;
    bool is_heap_allocated = false;

    if (alloc_chars <= MPTARIO_STACK_WPATH_LEN) {
        wpath = stack_wpath;
    } else {
        if (!mem || !mem->alloc) {
            return MPTARIO_ERR_BUFFER_TOO_SMALL; // Path too long for stack and no allocator provided
        }
        wpath = (wchar_t *)mem->alloc(mem->alloc_user_data, alloc_chars * sizeof(wchar_t));
        if (!wpath) return MPTAR_ERR_OUT_OF_MEMORY;
        is_heap_allocated = true;
    }

    int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, required_len);
    if (len <= 0) {
        if (is_heap_allocated) {
            mem->free(mem->alloc_user_data, wpath);
        }
        return MPTARIO_ERR_INVALID_ARG;
    }

    int actual_len = len - 1; // Index of the original null terminator
    bool has_trailing_sep = (actual_len > 0) && (wpath[actual_len - 1] == L'\\' || wpath[actual_len - 1] == L'/');

    if (has_trailing_sep) {
        wpath[actual_len]     = L'*';
        wpath[actual_len + 1] = L'\0';
    } else {
        wpath[actual_len]     = L'/';
        wpath[actual_len + 1] = L'*';
        wpath[actual_len + 2] = L'\0';
    }

    HANDLE hFind = FindFirstFileW(wpath, &dir->fdata);
    if (hFind == INVALID_HANDLE_VALUE) {
        if (is_heap_allocated) {
            mem->free(mem->alloc_user_data, wpath);
        }
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) return MPTARIO_ERR_NOT_FOUND;
        if (err == ERROR_ACCESS_DENIED) return MPTARIO_ERR_ACCESS_DENIED;
        return MPTARIO_ERR_IO;
    }

    dir->handle = hFind;
    dir->is_first = 1;
    
    if (is_heap_allocated) {
        mem->free(mem->alloc_user_data, wpath);
    }
    return MPTARIO_OK;
#else
    (void)mem; // Unused on POSIX
    
    DIR *d = opendir(path);
    if (!d) {
        if (errno == ENOENT || errno == ENOTDIR) return MPTARIO_ERR_NOT_FOUND;
        if (errno == EACCES || errno == EPERM) return MPTARIO_ERR_ACCESS_DENIED;
        return MPTARIO_ERR_IO;
    }

    dir->dir_ptr = d;
    return MPTARIO_OK;
#endif
}

int mptario_dir_next(mptario_dir *dir, mptario_dirent *out_ent) {
    if (!dir || !out_ent) {
        return MPTARIO_ERR_INVALID_ARG;
    }

#if defined(_WIN32)
    while (1) {
        if (dir->is_first) {
            dir->is_first = 0;
        } else {
            if (!FindNextFileW(dir->handle, &dir->fdata)) {
                DWORD err = GetLastError();
                if (err == ERROR_NO_MORE_FILES) return MPTARIO_ERR_NOT_FOUND;
                return MPTARIO_ERR_IO;
            }
        }

        /* Skip '.' and '..' */
        if (dir->fdata.cFileName[0] == L'.' && 
           (dir->fdata.cFileName[1] == L'\0' || (dir->fdata.cFileName[1] == L'.' && dir->fdata.cFileName[2] == L'\0'))) {
            continue;
        }

        int converted = WideCharToMultiByte(
            CP_UTF8, 0, 
            dir->fdata.cFileName, -1, 
            out_ent->name, sizeof(out_ent->name), 
            NULL, NULL
        );

        if (converted <= 0) {
            // If it failed because the buffer was too small
            if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                return MPTAR_ERR_BUFFER_TOO_SMALL;
            }
            continue;
        }

        return MPTARIO_OK;
    }

#else
    struct dirent *entry;

    while ((entry = readdir(dir->dir_ptr)) != NULL) {
        /* Skip '.' and '..' */
        if (entry->d_name[0] == '.' && 
           (entry->d_name[1] == '\0' || (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) {
            continue;
        }

        size_t name_len = mptar_strnlen(entry->d_name, sizeof(out_ent->name));
        
        if (name_len >= sizeof(out_ent->name)) {
            return MPTAR_ERR_BUFFER_TOO_SMALL;
        }

        mptar_memcpy(out_ent->name, entry->d_name, name_len + 1); // Copies name + null terminator

        return MPTARIO_OK;
    }

    return MPTARIO_ERR_NOT_FOUND;
#endif
}

void mptario_dir_close(mptario_dir *dir) {
    if (!dir) return;

#if defined(_WIN32)
    if (dir->handle && dir->handle != INVALID_HANDLE_VALUE) {
        FindClose(dir->handle);
        dir->handle = INVALID_HANDLE_VALUE;
    }
#else
    if (dir->dir_ptr) {
        closedir(dir->dir_ptr);
        dir->dir_ptr = NULL;
    }
#endif
}