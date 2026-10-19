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
    if (!path || !dir || !mem || !mem->alloc) return MPTARIO_ERR_INVALID_ARG;

#if defined(_WIN32)
    int required_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (required_len <= 0) return MPTARIO_ERR_INVALID_ARG;

    wchar_t *wpath = (wchar_t*)mem->alloc(mem->alloc_user_data, ((mptar_size_t)required_len + MPTARIO_SEARCH_SUFFIX_LEN) * sizeof(wchar_t));
    if (!wpath) return MPTARIO_ERR_OUT_OF_MEMORY;

    int path_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, required_len);
    if (path_len <= 0) {
        if(mem->free) mem->free(mem->alloc_user_data, wpath);
        return MPTARIO_ERR_INVALID_ARG;
    }

    int actual_len = path_len - 1; // Index of the original null terminator
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
    if(mem->free) mem->free(mem->alloc_user_data, wpath);
    if (hFind == INVALID_HANDLE_VALUE) {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) return MPTARIO_ERR_NOT_FOUND;
        if (err == ERROR_ACCESS_DENIED) return MPTARIO_ERR_ACCESS_DENIED;
        return MPTARIO_ERR_IO;
    }

    dir->handle = hFind;
    dir->is_first = 1;
    
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

#if defined(_WIN32) && !defined(MPTARIO_NO_REFS)
static PFN_GetFileInformationByHandleEx s_pfnGetFileInformationByHandleEx = NULL;
static int s_checked_getfileinfo_ex = 0;

void mptario_init_refs(void) {
    if (!s_checked_getfileinfo_ex) {
        HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
        if (hKernel32) {
            s_pfnGetFileInformationByHandleEx = (PFN_GetFileInformationByHandleEx)
                GetProcAddress(hKernel32, "GetFileInformationByHandleEx");
        }
        s_checked_getfileinfo_ex = 1;
    }
}
#endif

int mptario_file_id_compare(const mptario_file_id *a, const mptario_file_id *b) {
#if defined(_WIN32) && !defined(MPTARIO_NO_REFS)
    if (a->volume_serial != b->volume_serial) return (a->volume_serial < b->volume_serial) ? -1 : 1;
    if (a->high != b->high) return (a->high < b->high) ? -1 : 1;
    if (a->low != b->low) return (a->low < b->low) ? -1 : 1;
    return 0;
#else
    if (a->primary != b->primary) return (a->primary < b->primary) ? -1 : 1;
    if (a->secondary != b->secondary) return (a->secondary < b->secondary) ? -1 : 1;
    return 0;
#endif
}

int mptario_get_fileinfo(const char *path, mptario_fileinfo *out_info, mptar_memory_cfg *mem) {
    if (!path || !out_info || !mem || !mem->alloc) return MPTARIO_ERR_INVALID_ARG;

#if defined(_WIN32)
    int required_wpath_c = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    if (required_wpath_c <= 0) {
        return MPTARIO_ERR_INVALID_ARG;
    }

    wchar_t *wpath = (wchar_t*)mem->alloc(mem->alloc_user_data, required_wpath_c * sizeof(wchar_t));
    if (!wpath) return MPTARIO_ERR_OUT_OF_MEMORY;

    int path_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, required_wpath_c);
    if (path_len <= 0) {
        if(mem->free) mem->free(mem->alloc_user_data, wpath);
        return MPTARIO_ERR_INVALID_ARG;
    }

    WIN32_FILE_ATTRIBUTE_DATA attr_data;
    if (!GetFileAttributesExW(wpath, GetFileExInfoStandard, &attr_data)) {
        DWORD err = GetLastError();
        if(mem->free) mem->free(mem->alloc_user_data, wpath);

        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            out_info->type = MPTARIO_UNKNOWN_TYPE;
#ifndef MPTARIO_NO_REFS
            out_info->file_id.volume_serial = 0;
            out_info->file_id.high = 0;
            out_info->file_id.low = 0;
#else
            out_info->file_id.primary = 0;
            out_info->file_id.secondary = 0;
#endif
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
#ifndef MPTARIO_NO_REFS
        bool got_id = false;
        // Uses the modern FILE_ID_INFO that handles ReFS
        if (s_pfnGetFileInformationByHandleEx) {
            MPTAR_FILE_ID_INFO file_id_info;            
            if (s_pfnGetFileInformationByHandleEx(hFile, FileIdInfo, &file_id_info, sizeof(file_id_info))) {
                mptar_uint64 low, high;
                mptar_memcpy(&low, &file_id_info.Identifier[0], 8);
                mptar_memcpy(&high, &file_id_info.Identifier[8], 8);                
                out_info->file_id.volume_serial = file_id_info.VolumeSerialNumber;
                out_info->file_id.low = low;
                out_info->file_id.high = high;
                got_id = true;
            }
        }
#endif
        BY_HANDLE_FILE_INFORMATION handle_info;
        if (GetFileInformationByHandle(hFile, &handle_info)) {
            out_info->hardlink_count = (mptar_int32)handle_info.nNumberOfLinks;
#ifndef MPTARIO_NO_REFS
            if(!got_id){
                out_info->file_id.volume_serial = handle_info.dwVolumeSerialNumber;
                out_info->file_id.low = ((mptar_uint64)handle_info.nFileIndexHigh << 32) | handle_info.nFileIndexLow;
            } 
#else
            out_info->file_id.primary = handle_info.dwVolumeSerialNumber;
            out_info->file_id.secondary = ((mptar_uint64)handle_info.nFileIndexHigh << 32) | handle_info.nFileIndexLow;
#endif
        } else {
            out_info->hardlink_count = 1;
        }

        CloseHandle(hFile);
    } else {
        DWORD err = GetLastError();
        if(mem->free) mem->free(mem->alloc_user_data, wpath);

        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
            out_info->type = MPTARIO_UNKNOWN_TYPE;
#ifndef MPTARIO_NO_REFS
            out_info->file_id.volume_serial = 0;
            out_info->file_id.high = 0;
            out_info->file_id.low = 0;
#else
            out_info->file_id.primary = 0;
            out_info->file_id.secondary = 0;
#endif
            out_info->size = 0;
            out_info->hardlink_count = 0;
            out_info->attributes = 0;
            return MPTARIO_OK;
        }

        if (err == ERROR_ACCESS_DENIED) return MPTARIO_ERR_ACCESS_DENIED;
        if (err == ERROR_FILE_CORRUPT || err == ERROR_DISK_CORRUPT) return MPTARIO_ERR_CORRUPTED;

        out_info->hardlink_count = 1;
    }

    if(mem->free) mem->free(mem->alloc_user_data, wpath);

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
            out_info->file_id.primary = 0;
            out_info->file_id.secondary = 0;
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
    out_info->file_id.primary = (mptar_uint64)st.st_dev;
    out_info->file_id.secondary = (mptar_uint64)st.st_ino;

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

#if defined(_WIN32)
static int mptario_get_symlink_target_win(const wchar_t* wpath, mptar_memory_cfg* mem, char** symlink_target, mptar_size_t* symlink_target_len){
    HANDLE hFile = CreateFileW(wpath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, MPTAR_NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, MPTAR_NULL);
    if(!hFile) return (int)GetLastError();

    DWORD required_wchar_len = GetFinalPathNameByHandleW(hFile, MPTAR_NULL, 0, 0);
    if(required_wchar_len <= 0) {
        CloseHandle(hFile);
        return (int)GetLastError();
    } 
    
    wchar_t* symlink_wchar = (wchar_t*)mem->alloc(mem->alloc_user_data, required_wchar_len * sizeof(wchar_t));
    if(!symlink_wchar) {
        CloseHandle(hFile);
        return MPTARIO_ERR_OUT_OF_MEMORY;
    }

    DWORD symlink_wchar_len = GetFinalPathNameByHandleW(hFile, symlink_wchar, required_wchar_len, 0);
    if(symlink_wchar_len <= 0) {
        if(mem->free) mem->free(mem->alloc_user_data, symlink_wchar);
        CloseHandle(hFile);
        return (int)GetLastError();
    }

    BY_HANDLE_FILE_INFORMATION info;
    WINBOOL infoRes = GetFileInformationByHandle(hFile, &info);
    if(!infoRes){
        if(mem->free) mem->free(mem->alloc_user_data, symlink_wchar);
        CloseHandle(hFile);
        return (int)GetLastError();
    }

    CloseHandle(hFile);

    bool is_directory = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    int additional_space = (is_directory && (symlink_wchar[symlink_wchar_len-1] != L'\\' && symlink_wchar[symlink_wchar_len-1] != L'/')) ? 1 : 0;
    int required_utf8_len = WideCharToMultiByte(CP_UTF8, 0, symlink_wchar, -1, MPTAR_NULL, 0, MPTAR_NULL, MPTAR_NULL);
    if(required_utf8_len <= 0) {
        return (int)GetLastError();
    }

    char* symlink_utf8 = mem->alloc(mem->alloc_user_data, (mptar_size_t)required_utf8_len + additional_space);
    if(!symlink_utf8) return MPTARIO_ERR_OUT_OF_MEMORY;

    int symlink_utf8_len = WideCharToMultiByte(
        CP_UTF8, 0,
        symlink_wchar, -1,
        symlink_utf8, required_utf8_len,
        MPTAR_NULL, MPTAR_NULL
    );
    if(symlink_utf8_len <= 0) {
        if(mem->free) mem->free(mem->alloc_user_data, symlink_utf8);
        return GetLastError();
    }

    symlink_utf8_len += additional_space;
    if(additional_space > 0) {
        symlink_utf8[symlink_utf8_len-2] = '/';
        symlink_utf8[symlink_utf8_len-1] = '\0';
    }

    *symlink_target = symlink_utf8;
    *symlink_target_len = symlink_utf8_len;
    
    return MPTARIO_OK;
}

// This assumes relpath is a part of path (so path has relpath thus it being absolute path it also means removing relpath gives abs.)
static int mptario_write_symlink_win(mptar_metadata *meta, const char *path, const char *relpath, const wchar_t* wpath, mptar_memory_cfg* mem) {
    char* symlink_target = MPTAR_NULL;
    mptar_size_t symlink_target_len = 0;
    int result = mptario_get_symlink_target_win(wpath, mem, &symlink_target, &symlink_target_len);
    if(result != MPTARIO_OK) return result;

    // Checks for extended paths, sets an offest to skip the extended path prefix.
    mptar_size_t path_len = mptar_strlen(path);
    mptar_size_t relpath_len = mptar_strlen(relpath);
    mptar_size_t absolute_path_len = path_len - relpath_len;
    int symlink_extended_path = mptar_memcmp(symlink_target, "\\\\?\\", 4); // "\\\\?\\" is prefix for extended paths on Windows (max 2^15 chars).
    int path_extended = mptar_memcmp(path, "\\\\?\\", 4);
    mptar_size_t symlink_offset = (symlink_extended_path == 0 && path_extended != 0) ? 4 : 0;

    for (mptar_size_t i = symlink_offset; i < symlink_target_len; i++) {
        if(symlink_target[i] == '\\') symlink_target[i] = '/';
    }

    // If relative, truncates the path to only keep the relative path to the target.
    if(mptar_memcmp(symlink_target + symlink_offset, path, absolute_path_len) == 0){ // Target is Relative
        mptar_size_t relative_symlink_len = (mptar_size_t)symlink_target_len - absolute_path_len - symlink_offset;
        if(relative_symlink_len <= 0) {
            if(mem->free) mem->free(mem->alloc_user_data, symlink_target);
            return MPTARIO_ERR_INVALID_ARG;
        }

        char* relative_symlink_target = mem->alloc(mem->alloc_user_data, relative_symlink_len+1); // +1 for \0
        if(!relative_symlink_target){ 
            if(mem->free) mem->free(mem->alloc_user_data, symlink_target);
            return MPTARIO_ERR_OUT_OF_MEMORY;
        }
        
        mptar_size_t relative_target_offset = absolute_path_len + symlink_offset;
        mptar_memcpy(relative_symlink_target, symlink_target + relative_target_offset, symlink_target_len - relative_target_offset);
        relative_symlink_target[relative_symlink_len] = '\0';

        if(mem->free) mem->free(mem->alloc_user_data, symlink_target); // We only need the relative one.

        meta->link_target = relative_symlink_target;
    }else{ // Target is Absolute
        // TODO(safety): Allow disabling absolute symlinks (By default should disable them).
        meta->link_target = symlink_target;
    }

    return result;
}
#else 
static int mptario_write_symlink_posix(mptar_metadata *meta, mptario_fileinfo *info, const char *path, const char *relpath, mptar_memory_cfg* mem) {
    size_t buf_size = (info->size > 0) ? info->size + 1 : 1024;
    char* symlink_target = (char*)mem->alloc(mem->alloc_user_data, buf_size);
    if (!symlink_target) return MPTARIO_ERR_OUT_OF_MEMORY;

    ssize_t symlink_target_len = readlink(path, symlink_target, buf_size);
    if (symlink_target_len < 0) {
        if(mem->free) mem->free(mem->alloc_user_data, symlink_target);
        return MPTARIO_ERR_INVALID_ARG;
    }
    
    symlink_target[symlink_target_len] = '\0';

    mptar_size_t path_len = mptar_strlen(path);
    mptar_size_t relpath_len = mptar_strlen(relpath);
    mptar_size_t absolute_path_len = path_len - relpath_len;

    // If relative, truncates the path to only keep the relative path to the target.
    if(mptar_memcmp(symlink_target, path, absolute_path_len) == 0){ // Target is Relative
        mptar_size_t relative_symlink_len = (mptar_size_t)symlink_target_len - absolute_path_len;
        if(relative_symlink_len <= 0) {
            if(mem->free) mem->free(mem->alloc_user_data, symlink_target);
            return MPTARIO_ERR_INVALID_ARG;
        }

        char* relative_symlink_target = mem->alloc(mem->alloc_user_data, relative_symlink_len+1); // +1 for \0
        if(!relative_symlink_target){ 
            if(mem->free) mem->free(mem->alloc_user_data, symlink_target);
            return MPTARIO_ERR_OUT_OF_MEMORY;
        }
        
        mptar_memcpy(relative_symlink_target, symlink_target + absolute_path_len, symlink_target_len - absolute_path_len);
        relative_symlink_target[relative_symlink_len] = '\0';

        if(mem->free) mem->free(mem->alloc_user_data, symlink_target); // We only need the relative one.

        meta->link_target = relative_symlink_target;
    }else{ // Target is Absolute
        // TODO(safety): Allow disabling absolute symlinks (By default should disable them).
        meta->link_target = symlink_target;
    }

    return MPTARIO_OK;
}
#endif

#if defined(_WIN32)
static int mptario_write_file_win(wchar_t* wpath, mptar_uint64 size, mptario_write_context *ctx){
    HANDLE hFile = CreateFileW(
        wpath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, NULL
    );
    if (hFile == INVALID_HANDLE_VALUE) return MPTARIO_ERR_IO;

    DWORD bytes_read = 0;
    mptar_uint64 remaining = size;
    while (remaining > 0) {
        DWORD to_read = (remaining < ctx->buf_size) ? (DWORD)remaining : ctx->buf_size;
        if (!ReadFile(hFile, ctx->buf, to_read, &bytes_read, NULL) || bytes_read == 0) {
            CloseHandle(hFile);
            return MPTARIO_ERR_IO;
        }

        int chunk_err = 0;
        mptar_size_t written = mptar_write_data_chunk(ctx->writer, ctx->buf, (mptar_size_t)bytes_read, &chunk_err);
        if (chunk_err != 0 || written != bytes_read) {
            CloseHandle(hFile);
            return (chunk_err != 0) ? chunk_err : MPTARIO_ERR_IO;
        }

        remaining -= bytes_read;
    }
    
    CloseHandle(hFile);

    return MPTARIO_OK;
}
#else
static int mptario_write_file_posix(const char* path, mptar_uint64 size, mptario_write_context *ctx){
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        return MPTARIO_ERR_IO;
    }

    mptar_uint64 remaining = size;
    while (remaining > 0) {
        mptar_size_t to_read = (remaining < ctx->buf_size) ? (mptar_size_t)remaining : ctx->buf_size;
        ssize_t bytes_read = read(fd, ctx->buf, to_read);
        if (bytes_read <= 0) {
            close(fd);
            return MPTARIO_ERR_IO;
        }

        int chunk_err = 0;
        mptar_size_t written = mptar_write_data_chunk(ctx->writer, ctx->buf, (mptar_size_t)bytes_read, &chunk_err);
        if (chunk_err != 0 || written != (mptar_size_t)bytes_read) {
            close(fd);
            return (chunk_err != 0) ? chunk_err : MPTARIO_ERR_IO;
        }

        remaining -= (mptar_uint64)bytes_read;
    }

    close(fd);

    return MPTARIO_OK;
}
#endif

static int mptario_get_hardlink_entry(mptario_fileinfo *info, mptario_write_context *ctx, mptario_hardlink_entry** out_entry){
    mptar_memory_cfg *mem = &(ctx->writer->memory);
    if (!ctx->hardlink_registry) {
        ctx->hardlink_registry = mem->alloc(mem->alloc_user_data, sizeof(mptario_hardlink_registry));
        if (!ctx->hardlink_registry) return MPTARIO_ERR_OUT_OF_MEMORY;
        
        ctx->hardlink_registry->entries = MPTAR_NULL;
        ctx->hardlink_registry->count = 0;
        ctx->hardlink_registry->capacity = 0;
    }

    // ARRAY IMPLEMENTATION
    *out_entry = MPTAR_NULL;
    bool has_entry = false;
    for(int i = 0; i < ctx->hardlink_registry->count; i++){
        mptario_hardlink_entry* entry = &ctx->hardlink_registry->entries[i];
        int comparison = mptario_file_id_compare(&info->file_id, &entry->file_id);
        if(comparison == 0){
            *out_entry = entry;
            has_entry = true;
        }
    }

    if (!has_entry) {
        if (ctx->hardlink_registry->capacity <= ctx->hardlink_registry->count) {
            mptar_size_t old_capacity = ctx->hardlink_registry->capacity;
            mptar_size_t new_capacity = (old_capacity == 0) ? 16 : old_capacity * 2;
            
            void *new_entries = mem->alloc(mem->alloc_user_data, new_capacity * sizeof(mptario_hardlink_entry));
            if (!new_entries) return MPTARIO_ERR_OUT_OF_MEMORY;
        
            if (old_capacity > 0) {
                mptar_memcpy(new_entries, ctx->hardlink_registry->entries, sizeof(mptario_hardlink_entry) * ctx->hardlink_registry->count);
                if (mem->free) {
                    mem->free(mem->alloc_user_data, ctx->hardlink_registry->entries);
                }
            }
        
            ctx->hardlink_registry->entries = new_entries;
            ctx->hardlink_registry->capacity = new_capacity;
        }
    }

    return MPTARIO_OK;
}

/**
 * \brief Writes a tar archive entry using the minipaxtar library.
 *
 * Populates header metadata from the provided \p info and \p relpath, 
 * If file, it reads data from the given absolute \p path and writes it with \p ctx->writer
 *
 * \param info Pointer to the entry info structure with metadata to write.
 * \param path Absolute filesystem path used for reading the entry data.
 * \param relpath Relative path assigned within the archive structure, relative to path without relpath
 * \param ctx Pointer to the working context holding the buffer and writer instance.
 * \return Zero on success, or a negative error code on failure.
 */
static int mptario_write_entry(mptario_fileinfo *info, const char *path, const char *relpath, mptario_write_context *ctx) {
    if (!info || !relpath || !ctx || info->type == -1 || !ctx->writer || !ctx->writer->memory.alloc) return MPTARIO_ERR_INVALID_ARG;

    bool is_file = info->type == MPTAR_TYPE_FILE || info->type == MPTAR_TYPE_FILE_LEGACY;
    mptar_memory_cfg *mem = &(ctx->writer->memory);
    mptar_metadata meta = {0};
    meta.path = relpath;
    meta.size = is_file ? info->size : 0;
    meta.mode = info->mode;
    meta.mtime = info->mtime;
    meta.typeflag = info->type;
#ifndef MPTAR_NO_EXTRA_TIMES
    meta.atime = info->atime;
    meta.ctime = info->ctime;
#endif

    bool needs_data_written = (is_file && info->size > 0);
    bool needs_wpath = needs_data_written || info->type == MPTAR_TYPE_SYMLINK;
    if (needs_wpath) {
#if defined(_WIN32)
        int required_wpath_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
        if (required_wpath_len <= 0) return GetLastError();

        wchar_t *wpath = (wchar_t *)mem->alloc(mem->alloc_user_data, required_wpath_len * sizeof(wchar_t));
        if (!wpath) return MPTARIO_ERR_OUT_OF_MEMORY;

        int wpath_len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wpath, required_wpath_len);
        if (wpath_len <= 0) {
            if(mem->free) mem->free(mem->alloc_user_data, wpath);
            return GetLastError();
        }
#endif

        if(info->type == MPTAR_TYPE_SYMLINK){
#if defined(_WIN32)
            mptario_write_symlink_win(&meta, path, relpath, wpath, mem);
#else
            mptario_write_symlink_posix(&meta, info, path, relpath, mem);
#endif
            int status = mptar_write_header(ctx->writer, &meta);
            if (status < 0) return status;
            if(mem->free && meta.link_target) mem->free(mem->alloc_user_data, (void*)meta.link_target);
        } else {
            bool has_hardlink_entry = false;
            if(info->hardlink_count > 1){
                mptario_hardlink_entry* entry;
                int res = mptario_get_hardlink_entry(info, ctx, &entry);
                if(res < 0) return res; // alloc error

                if(entry != MPTAR_NULL){
                    has_hardlink_entry = true;
                    meta.link_target = entry->archive_path;
                    meta.typeflag = MPTAR_HARD_LINK;
                    meta.size = 0; // Hardlinks are all initially written as files.
                }
            }

            int status = mptar_write_header(ctx->writer, &meta);
            if (status < 0) return status;

            if(!has_hardlink_entry){
                #if defined(_WIN32)
                    mptario_write_file_win(wpath, info->size, ctx);
                #else
                    mptario_write_file_posix(path, info->size, ctx);
                #endif
                
                if(info->hardlink_count > 1){
                    mptar_size_t relpath_len = mptar_strlen(relpath);
                    char* new_relpath = (char*)mem->alloc(mem->alloc_user_data, relpath_len + 1);
                    if(!new_relpath){
                        return MPTARIO_ERR_OUT_OF_MEMORY;    
                    }

                    mptar_memcpy(new_relpath, relpath, relpath_len);
                    new_relpath[relpath_len] = '\0';

                    mptario_hardlink_entry entry = {
                        .file_id = info->file_id,
                        .archive_path = new_relpath
                    };
                    
                    ctx->hardlink_registry->entries[ctx->hardlink_registry->count] = entry;
                    ctx->hardlink_registry->count += 1;
                }
            }
        }
    }else{
        int status = mptar_write_header(ctx->writer, &meta);
        if (status < 0) return status;
    }

    int finalize_err = mptar_write_finalize(ctx->writer, &meta);
    if (finalize_err < 0) {
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
static int mptario_write_recursive(const char *path, mptar_size_t base_len, mptar_size_t initial_base_len, mptario_write_context *ctx) {
    if(!ctx->writer->memory.alloc) return MPTARIO_ERR_INVALID_ARG;
    mptar_memory_cfg* mem = &(ctx->writer->memory);

    mptario_dir dir;
    int err = mptario_dir_open(path, &dir, mem);
    if (err != 0) return err;

    mptario_dirent ent;
    while ((err = mptario_dir_next(&dir, &ent)) == 0) {
        mptar_size_t name_len = mptar_strlen(ent.name);
        char *full_path = (char*)mem->alloc(mem->alloc_user_data, base_len + name_len + 2); // for: \0 and //
        if (!full_path) {
            mptario_dir_close(&dir);
            return MPTARIO_ERR_OUT_OF_MEMORY;
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
                    if(mem->free) mem->free(mem->alloc_user_data, full_path);
                    mptario_dir_close(&dir);
                    return err;
                }
            }
        }

        if(mem->free) mem->free(mem->alloc_user_data, full_path);
    }

    mptario_dir_close(&dir);
    return (err == MPTARIO_ERR_NOT_FOUND) ? 0 : err; // End of enumeration is a normal completion.
}

int mptario_pack_stream(mptario_write_context *ctx, const char *path)
{
    if (!ctx || !ctx->writer || !ctx->writer->write || !path || !ctx->writer->memory.alloc) return MPTARIO_ERR_INVALID_ARG;

    mptar_memory_cfg* mem = &(ctx->writer->memory);
    mptar_size_t path_len = mptar_strlen(path);
    int needs_slash = (path_len > 0 && path[path_len - 1] != '/' && path[path_len - 1] != '\\');
    mptar_size_t needed = path_len + (needs_slash ? 2 : 1);
    char *base_path = NULL;
    
    mptar_size_t target_size = (ctx->buf_size > 0) ? ctx->buf_size : MPTARIO_DEFAULT_CHUNK_SIZE;
    bool needs_buf_free = false; 
    if (ctx->buf == MPTAR_NULL) {
        ctx->buf = (char*)mem->alloc(mem->alloc_user_data, target_size);
        if (!ctx->buf) return MPTARIO_ERR_OUT_OF_MEMORY;
        ctx->buf_size = target_size;
        needs_buf_free = true;
    }

    base_path = (char*)mem->alloc(mem->alloc_user_data, needed);
    if (!base_path) {
        if (needs_buf_free) {
            if(mem->free) mem->free(mem->alloc_user_data, ctx->buf);
            ctx->buf = MPTAR_NULL;
        }
        return MPTARIO_ERR_OUT_OF_MEMORY;
    }

    for (mptar_size_t i = 0; i < path_len; i++) base_path[i] = (path[i] == '\\') ? '/' : path[i];
    base_path[path_len] = '\0';
    if (needs_slash) {
        base_path[path_len] = '/';
        base_path[path_len + 1] = '\0';
        path_len++;
    }

    int res = mptario_write_recursive(base_path, path_len, path_len, ctx);
    
    if(mem->free) mem->free(mem->alloc_user_data, base_path);
    if (needs_buf_free) {
        if(mem->free) mem->free(mem->alloc_user_data, ctx->buf);
        ctx->buf = MPTAR_NULL;
    }

    if (res < 0) return res;
    return mptar_close_archive(ctx->writer);
}