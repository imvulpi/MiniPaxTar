#include "mptario.h"
#if !defined(_WIN32)
#include <fcntl.h>
#include <unistd.h>
#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* Required for MAP_ANONYMOUS */
#endif
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON /* For older POSIX where MAP_ANONYMOUS is unavailable */
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#endif

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

    mptar_size_t alloc_chars = (mptar_size_t)required_len + MPTARIO_SEARCH_SUFFIX_LEN;
    wchar_t *wpath = NULL;
    bool is_heap_allocated = false;

#ifndef MPTARIO_NO_STACK
    wchar_t stack_wpath[MPTARIO_STACK_WPATH_LEN];
    if (alloc_chars <= MPTARIO_STACK_WPATH_LEN) {
        wpath = stack_wpath;
    } else 
#endif
    {
        if (!mem || !mem->alloc) {
            return MPTARIO_ERR_BUFFER_TOO_SMALL;
        }
        wpath = (wchar_t *)mem->alloc(mem->alloc_user_data, alloc_chars * sizeof(wchar_t));
        if (!wpath) return MPTARIO_ERR_OUT_OF_MEMORY;
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
                return MPTARIO_ERR_BUFFER_TOO_SMALL;
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

        mptar_size_t name_len = mptar_strnlen(entry->d_name, sizeof(out_ent->name));
        
        if (name_len >= sizeof(out_ent->name)) {
            return MPTARIO_ERR_BUFFER_TOO_SMALL;
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

#if defined(_WIN32)
/**
 * \brief Converts a Windows FILETIME to a mptar_timespec.
 * 
 * Transforms a Windows file time (100-nanosecond ticks since January 1, 1601) 
 * into Unix time represented in seconds and nanoseconds since January 1, 1970. 
 * Timestamps predating the Unix epoch are clamped to zero.
 * 
 * \param ft Pointer to the source Windows FILETIME structure.
 * \param ts Pointer to the destination mptar_timespec structure.
 */
static void mptar_filetime_to_timespec(const FILETIME *ft, mptar_timespec *ts) {
    unsigned long long intervals = ((unsigned long long)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
    if (intervals < UNIX_EPOCH_INTERVALS) {
        ts->sec = 0;
        ts->nsec = 0;
        return;
    }

    unsigned long long unix_intervals = intervals - UNIX_EPOCH_INTERVALS;
    
    ts->sec = (mptar_int64)(unix_intervals / 10000000ULL);
    ts->nsec = (mptar_uint32)((unix_intervals % 10000000ULL) * 100);
}
#endif

int mptario_get_fileinfo(const char *path, mptario_fileinfo *out_info, mptar_memory_cfg *mem) {
    if (!path || !out_info) {
        return MPTARIO_ERR_INVALID_ARG;
    }

#if defined(_WIN32)
    int required_chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (required_chars <= 0) {
        return MPTARIO_ERR_INVALID_ARG;
    }

    wchar_t *wpath = NULL;
    bool is_heap_allocated = false;

#ifndef MPTARIO_NO_STACK
    wchar_t stack_wpath[MPTARIO_STACK_WPATH_LEN];
    if (required_chars <= MPTARIO_STACK_WPATH_LEN) {
        wpath = stack_wpath;
    } else 
#endif
    if (mem && mem->alloc) {
        wpath = (wchar_t *)mem->alloc(mem->alloc_user_data, required_chars * sizeof(wchar_t));
        if (!wpath) return MPTARIO_ERR_OUT_OF_MEMORY;
        is_heap_allocated = true;
    } else {
        return MPTARIO_ERR_BUFFER_TOO_SMALL;
    }

    int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, required_chars);
    if (len <= 0) {
        if (is_heap_allocated) {
            mem->free(mem->alloc_user_data, wpath);
        }
        return MPTARIO_ERR_INVALID_ARG;
    }

    WIN32_FILE_ATTRIBUTE_DATA attr_data;
    if (!GetFileAttributesExW(wpath, GetFileExInfoStandard, &attr_data)) {
        DWORD err = GetLastError();

        if (is_heap_allocated) {
            mem->free(mem->alloc_user_data, wpath);
        }

        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            out_info->type = MPTARIO_UNKNOWN_TYPE;
            out_info->size = 0;
            out_info->hardlink_count = 0;
            out_info->attributes = 0;
            return MPTARIO_OK;
        }

        if (err == ERROR_ACCESS_DENIED) return MPTARIO_ERR_ACCESS_DENIED;
        if (err == ERROR_FILE_CORRUPT || err == ERROR_DISK_CORRUPT) return MPTARIO_ERR_CORRUPTED;

        return MPTARIO_ERR_IO;
    }

    out_info->attributes = (mptar_int32)attr_data.dwFileAttributes;
    out_info->size = ((mptar_uint64)attr_data.nFileSizeHigh << 32) | attr_data.nFileSizeLow;

    if (attr_data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
        out_info->type = MPTAR_TYPE_SYMLINK;
    } else if (attr_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        out_info->type = MPTAR_TYPE_DIRECTORY;
    } else {
        out_info->type = MPTAR_TYPE_FILE;
    }

    HANDLE hFile = CreateFileW(
        wpath,
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
        NULL
    );

    if (hFile != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION handle_info;
        if (GetFileInformationByHandle(hFile, &handle_info)) {
            out_info->hardlink_count = (mptar_int32)handle_info.nNumberOfLinks;
        } else {
            out_info->hardlink_count = 1;
        }
        CloseHandle(hFile);
    } else {
        DWORD err = GetLastError();

        if (is_heap_allocated) {
            mem->free(mem->alloc_user_data, wpath);
        }

        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            out_info->type = MPTARIO_UNKNOWN_TYPE;
            out_info->size = 0;
            out_info->hardlink_count = 0;
            return MPTARIO_OK;
        }

        if (err == ERROR_ACCESS_DENIED) return MPTARIO_ERR_ACCESS_DENIED;
        if (err == ERROR_FILE_CORRUPT || err == ERROR_DISK_CORRUPT) return MPTARIO_ERR_CORRUPTED;

        out_info->hardlink_count = 1;
    }

    if (is_heap_allocated) {
        mem->free(mem->alloc_user_data, wpath);
    }

    mptar_filetime_to_timespec(&attr_data.ftLastWriteTime, &out_info->mtime.value);
    out_info->mtime.has_value = true;

    #ifndef MPTAR_NO_EXTRA_TIMES
    mptar_filetime_to_timespec(&attr_data.ftLastAccessTime, &out_info->atime.value);
    out_info->atime.has_value = true;

    mptar_filetime_to_timespec(&attr_data.ftCreationTime, &out_info->ctime.value);
    out_info->ctime.has_value = true;
    #endif

    return MPTARIO_OK;

#else
    (void)mem;
    struct stat st;
    if (lstat(path, &st) != 0) {
        if (errno == ENOENT || errno == ENOTDIR) {
            out_info->type = MPTARIO_UNKNOWN_TYPE;
            out_info->size = 0;
            out_info->hardlink_count = 0;
            out_info->attributes = 0;
            return MPTARIO_OK;
        }

        if (errno == EACCES || errno == EPERM) return MPTARIO_ERR_ACCESS_DENIED;
        if (errno == EIO) return MPTARIO_ERR_CORRUPTED;

        return MPTARIO_ERR_IO;
    }

    if (S_ISREG(st.st_mode)) {
        out_info->type = MPTAR_TYPE_FILE;
    } else if (S_ISDIR(st.st_mode)) {
        out_info->type = MPTAR_TYPE_DIRECTORY;
    } else if (S_ISLNK(st.st_mode)) {
        out_info->type = MPTAR_TYPE_SYMLINK;
    } else if (S_ISFIFO(st.st_mode)) {
        out_info->type = MPTAR_TYPE_PIPE;
    }else if (S_ISCHR(st.st_mode)) {
        out_info->type = MPTAR_TYPE_SPECIAL_CHARACTER_FILE;
    } else if (S_ISBLK(st.st_mode)) {
        out_info->type = MPTAR_TYPE_SPECIAL_BLOCK_FILE;
    } else {
        out_info->type = MPTARIO_UNKNOWN_TYPE;
    } // Sockets not handled.

    out_info->size = (mptar_uint64)st.st_size;
    out_info->hardlink_count = (mptar_int32)st.st_nlink;
    out_info->attributes = (mptar_int32)st.st_mode;

    #if defined(__APPLE__)
        out_info->mtime.value.sec = (mptar_int64)st.st_mtimespec.tv_sec;
        out_info->mtime.value.nsec = (mptar_uint32)st.st_mtimespec.tv_nsec;
        #ifndef MPTAR_NO_EXTRA_TIMES
        out_info->atime.value.sec = (mptar_int64)st.st_atimespec.tv_sec;
        out_info->atime.value.nsec = (mptar_uint32)st.st_atimespec.tv_nsec;
        out_info->ctime.value.sec = (mptar_int64)st.st_ctimespec.tv_sec;
        out_info->ctime.value.nsec = (mptar_uint32)st.st_ctimespec.tv_nsec;
        #endif
    #else
        out_info->mtime.value.sec = (mptar_int64)st.st_mtim.tv_sec;
        out_info->mtime.value.nsec = (mptar_uint32)st.st_mtim.tv_nsec;
        #ifndef MPTAR_NO_EXTRA_TIMES
        out_info->atime.value.sec = (mptar_int64)st.st_atim.tv_sec;
        out_info->atime.value.nsec = (mptar_uint32)st.st_atim.tv_nsec;
        out_info->ctime.value.sec = (mptar_int64)st.st_ctim.tv_sec;
        out_info->ctime.value.nsec = (mptar_uint32)st.st_ctim.tv_nsec;
        #endif
    #endif

    out_info->mtime.has_value = true;
    #ifndef MPTAR_NO_EXTRA_TIMES
        out_info->atime.has_value = true;
        out_info->ctime.has_value = true;
    #endif

    return MPTARIO_OK;
#endif
}

static int mptario_write_entry(mptario_fileinfo *info, const char *path, const char *relpath, mptario_context *ctx) {
    if (!info || !relpath || !ctx || info->type == -1) {
        return MPTARIO_ERR_INVALID_ARG;
    }

    mptar_memory_cfg *mem = &(ctx->writer->memory);
    mptar_metadata meta = {0};
    meta.path = relpath;
    if (info->type == MPTAR_TYPE_FILE) {
        meta.size = info->size;
    } else {
        meta.size = 0; 
    }
    meta.mode = info->mode;
    meta.mtime = info->mtime;
    meta.typeflag = info->type;
#ifndef MPTAR_NO_EXTRA_TIMES
    meta.atime = info->atime;
    meta.ctime = info->ctime;
#endif

    int status = mptar_write_header(ctx->writer, &meta);
    if (status < 0) {
        return status;
    }

    if (info->type == MPTAR_TYPE_FILE && info->size > 0) {
#if defined(_WIN32)
        int required_chars = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
        if (required_chars <= 0) {
            return MPTARIO_ERR_INVALID_ARG;
        }

        wchar_t *wpath = NULL;
        int is_heap = 0;

#ifndef MPTARIO_NO_STACK
        wchar_t stack_wpath[MPTARIO_STACK_WPATH_LEN];
        if (required_chars <= MPTARIO_STACK_WPATH_LEN) {
            wpath = stack_wpath;
        } else 
#endif
        {
            if (!mem || !mem->alloc) return MPTARIO_ERR_BUFFER_TOO_SMALL;
            wpath = (wchar_t *)mem->alloc(mem->alloc_user_data, required_chars * sizeof(wchar_t));
            if (!wpath) return MPTARIO_ERR_OUT_OF_MEMORY;
            is_heap = 1;
        }

        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, required_chars);

        HANDLE hFile = CreateFileW(
            wpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, NULL
        );

        if (is_heap) mem->free(mem->alloc_user_data, wpath);
        if (hFile == INVALID_HANDLE_VALUE) {
            return MPTARIO_ERR_IO;
        }

        DWORD bytes_read = 0;
        mptar_uint64 remaining = info->size;
        while (remaining > 0) {
            DWORD to_read = (remaining < ctx->buf_size) ? (DWORD)remaining : ctx->buf_size;
            if (!ReadFile(hFile, ctx->chunk_buf, to_read, &bytes_read, NULL) || bytes_read == 0) {
                CloseHandle(hFile);
                return MPTARIO_ERR_IO;
            }

            int chunk_err = 0;
            mptar_size_t written = mptar_write_data_chunk(ctx->writer, ctx->chunk_buf, (mptar_size_t)bytes_read, &chunk_err);
            if (chunk_err != 0 || written != bytes_read) {
                CloseHandle(hFile);
                return (chunk_err != 0) ? chunk_err : MPTARIO_ERR_IO;
            }

            remaining -= bytes_read;
        }

        CloseHandle(hFile);

#else
        (void)mem;
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            return MPTARIO_ERR_IO;
        }

        mptar_uint64 remaining = info->size;
        while (remaining > 0) {
            mptar_size_t to_read = (remaining < ctx->buf_size) ? (mptar_size_t)remaining : ctx->buf_size;
            ssize_t bytes_read = read(fd, ctx->chunk_buf, to_read);
            if (bytes_read <= 0) {
                close(fd);
                return MPTARIO_ERR_IO;
            }

            int chunk_err = 0;
            mptar_size_t written = mptar_write_data_chunk(ctx->writer, ctx->chunk_buf, (mptar_size_t)bytes_read, &chunk_err);
            if (chunk_err != 0 || written != (mptar_size_t)bytes_read) {
                close(fd);
                return (chunk_err != 0) ? chunk_err : MPTARIO_ERR_IO;
            }

            remaining -= (mptar_uint64)bytes_read;
        }

        close(fd);
#endif
    }

    int finalize_err = mptar_write_finalize(ctx->writer, &meta);
    if (finalize_err != 0) {
        return finalize_err;
    }

    return MPTARIO_OK;
}

/**
 * \brief Recursively traverses a directory path and writes its entries into the archive.
 * 
 * This internal helper function walks through directories and files starting from the 
 * given path, maintaining base length offsets to ensure that paths stored in the archive 
 * are relative rather than absolute.
 * 
 * \note **Important Path Requirement:** The input string passed to `path` must be properly 
 * formatted and terminated (expecting trailing slash so that initial offset calculations 
 * remain reliable across recursion levels.
 * 
 * \param path             The current absolute or relative directory path being traversed.
 * \param base_len         The string length of the current base prefix used for relative path stripping.
 * \param initial_base_len The original, unchanging base length captured at the root of the archive operation.
 *                         Should be the length counting '/', but not counting \0.
 * \param writer           A pointer to the active \ref mptar_writer stream configuration and callback structure.
 * 
 * \return Returns \ref MPTARIO_OK on successful traversal and writing, or a negative error code 
 *         if an I/O, traversal, or header generation failure occurs.
 */
static int mptario_write_recursive(const char *path, mptar_size_t base_len, mptar_size_t initial_base_len, mptario_context *ctx) {
    mptar_memory_cfg* mem = &(ctx->writer->memory);
    mptario_dir dir;
    int err = mptario_dir_open(path, &dir, mem);
    if (err != 0) return err;

    mptario_dirent ent;
    while ((err = mptario_dir_next(&dir, &ent)) == 0) {
        mptar_size_t name_len = mptar_strlen(ent.name);
        mptar_size_t path_needed = base_len + name_len + 2; // for: \0 and //
        char *full_path = NULL;
        bool path_is_heap = 0;

#ifndef MPTARIO_NO_STACK
        char stack_path[MPTARIO_STACK_PATH_LEN];
        if (path_needed <= MPTARIO_STACK_PATH_LEN) {
            full_path = stack_path;
        } else 
#endif 
        {
            if (!mem || !mem->alloc) {
                mptario_dir_close(&dir);
                return MPTARIO_ERR_BUFFER_TOO_SMALL;
            }

            full_path = (char*)mem->alloc(mem->alloc_user_data, path_needed);
            if (!full_path) {
                mptario_dir_close(&dir);
                return MPTARIO_ERR_OUT_OF_MEMORY;
            }

            path_is_heap = true;
        }

        mptar_memcpy(full_path, path, base_len);
        mptar_memcpy(full_path + base_len, ent.name, name_len + 1);
        const char *relative_path = full_path + initial_base_len;

        mptario_fileinfo info;
        int info_err = mptario_get_fileinfo(full_path, &info, mem);

        if (info_err == 0) {
            mptario_write_entry(&info, full_path, relative_path, ctx);
            if (info.type == MPTAR_TYPE_DIRECTORY) {
                mptar_size_t sub_len = base_len + name_len;
                full_path[sub_len] = '/';
                full_path[sub_len + 1] = '\0';

                err = mptario_write_recursive(full_path, sub_len + 1, initial_base_len, ctx);
                if (err != 0 && err != MPTARIO_ERR_NOT_FOUND) {
                    if (path_is_heap) mem->free(mem->alloc_user_data, full_path);
                    mptario_dir_close(&dir);
                    return err;
                }
            }
        }

        if (path_is_heap) {
            mem->free(mem->alloc_user_data, full_path);
        }
    }

    mptario_dir_close(&dir);
    return (err == MPTARIO_ERR_NOT_FOUND) ? 0 : err; // End of enumeration is a normal completion.
}

int mptario_pack_stream(mptario_context *ctx, const char *path)
{
    // Alloc/Free can be unset - it fallbacks to stack if enabled.
    if (ctx == MPTAR_NULL || ctx->writer == MPTAR_NULL || ctx->writer->write == MPTAR_NULL || path == MPTAR_NULL) 
    {
        return MPTARIO_ERR_INVALID_ARG;
    }

    mptar_memory_cfg* mem = &(ctx->writer->memory);
    mptar_size_t len = mptar_strlen(path);
    int needs_slash = (len > 0 && path[len - 1] != '/' && path[len - 1] != '\\');
    mptar_size_t needed = len + (needs_slash ? 2 : 1);
    char *base_path = NULL;
    bool base_is_heap = false;
    bool needs_buf_free = false; 
    mptar_size_t target_size = (ctx->buf_size > 0) ? ctx->buf_size : MPTARIO_DEFAULT_CHUNK_SIZE;

#ifndef MPTARIO_DISABLE_CHUNK_STACK_FALLBACK
    char fallback_chunk_buf[MPTARIO_DEFAULT_CHUNK_SIZE]; 
#endif

    if (ctx->chunk_buf == MPTAR_NULL) {
        if (mem && mem->alloc) {
            ctx->chunk_buf = (char*)mem->alloc(mem->alloc_user_data, target_size);
            if (!ctx->chunk_buf) return MPTARIO_ERR_OUT_OF_MEMORY;
            ctx->buf_size = target_size;
            needs_buf_free = true;
        } else {
#ifndef MPTARIO_DISABLE_CHUNK_STACK_FALLBACK
            if (target_size > sizeof(fallback_chunk_buf)) {
                return MPTARIO_ERR_BUFFER_TOO_SMALL;
            }
            ctx->chunk_buf = fallback_chunk_buf;
            ctx->buf_size = target_size;
            needs_buf_free = false;
#else
            return MPTARIO_ERR_INVALID_ARG;
#endif
        }
    }

    bool use_stack = false;
#ifndef MPTARIO_NO_STACK
    char stack_base[MPTARIO_STACK_PATH_LEN];
    if (needed <= MPTARIO_STACK_PATH_LEN) use_stack = true;
#endif

    if (use_stack) {
#ifndef MPTARIO_NO_STACK
        base_path = stack_base;
#endif
    } else {
        if (!mem || !mem->alloc) {
            if (needs_buf_free) {
                mem->free(mem->alloc_user_data, ctx->chunk_buf);
                ctx->chunk_buf = MPTAR_NULL;
            }
            return MPTARIO_ERR_BUFFER_TOO_SMALL;
        }
        base_path = (char*)mem->alloc(mem->alloc_user_data, needed);
        if (!base_path) {
            if (needs_buf_free) {
                mem->free(mem->alloc_user_data, ctx->chunk_buf);
                ctx->chunk_buf = MPTAR_NULL;
            }
            return MPTARIO_ERR_OUT_OF_MEMORY;
        }
        base_is_heap = true;
    }

    for (mptar_size_t i = 0; i < len; i++) base_path[i] = (path[i] == '\\') ? '/' : path[i];
    base_path[len] = '\0';
    if (needs_slash) {
        base_path[len] = '/';
        base_path[len + 1] = '\0';
        len++;
    }

    int res = mptario_write_recursive(base_path, len, len, ctx);
    
    if (base_is_heap && mem && mem->free) mem->free(mem->alloc_user_data, base_path);
    if (needs_buf_free && mem && mem->free) {
        mem->free(mem->alloc_user_data, ctx->chunk_buf);
        ctx->chunk_buf = MPTAR_NULL;
    }

    if (res < 0) return res;
    return mptar_close_archive(ctx->writer);
}