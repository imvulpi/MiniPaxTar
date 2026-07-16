#include "minipaxtar.h"

#ifdef MPTAR_NO_STD

mptar_size_t strlen(const char* str){
    mptar_size_t len;
    for (len = 0 ;; len++)
    {
        if(str[len] == '\0') return len;   
    }
}

void memcpy(void* dest, const void* src, mptar_size_t n) {
    char* d = (char*)dest;
    const char* s = (const char*)src;
    
    for (mptar_size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

mptar_size_t strcpy(char* dest, const char* src) {
    mptar_size_t i = 0;
    while (src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    return i;
}

void memset(void* data, char value, mptar_size_t amount){
    char* bytes = (char*)data;
    for (mptar_size_t i = 0; i < amount; i++)
    {
        bytes[i] = value;
    }
}

typedef _Bool bool;
#define true 1
#define false 0

#else

#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#endif

#ifndef MPTAR_CUSTOM_U64TOA

char* u64toa(mptar_uint64 value, char* str, mptar_size_t str_size){
    if (str == MPTAR_NULL || str_size < 2) {
        return MPTAR_NULL;
    }

    mptar_size_t i = 0;
    if (value == 0) {
        str[0] = '0';
        str[1] = '\0';
        return str;
    }

    while (value > 0 && i < (str_size - 1)) {
        str[i++] = (char)((value % 10) + '0');
        value /= 10;
    }

    if (value > 0) {
        return MPTAR_NULL;
    }

    str[i] = '\0';

    mptar_size_t start = 0;
    mptar_size_t end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }

    return str;
}

#endif

#ifndef MPTAR_CUSTOM_U64TOO

char* u64too(mptar_uint64 value, char* str, mptar_size_t size) {
    if (str == MPTAR_NULL || size < 2) return MPTAR_NULL;

    mptar_size_t i = 0;

    if (value == 0) {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    while (value > 0 && i < (size - 1)) {
        str[i++] = (char)((value & 7) + '0');         
        value >>= 3; 
    }

    if (value > 0) return MPTAR_NULL;

    str[i] = '\0';

    mptar_size_t start = 0;
    mptar_size_t end = i - 1;
    while (start < end) {
        char temp = str[start];
        str[start] = str[end];
        str[end] = temp;
        start++;
        end--;
    }

    return str;
}

#endif

mptar_size_t safe_bounded_strlen(const char* str, mptar_size_t max_limit) {
    mptar_size_t count = 0;
    while (count < max_limit && str[count] != '\0') {
        count++;
    }
    return count;
}

void stream_zeroes(mptar_writer* ctx, mptar_size_t padding_needed) {
    const char zero_chunk[16] = {0}; 
    mptar_size_t written = 0;

    while (written < padding_needed) {
        mptar_size_t remaining = padding_needed - written;
        mptar_size_t chunk_size = (remaining > 16) ? 16 : remaining;

        ctx->cfg.write(ctx->cfg.write_user_data, zero_chunk, chunk_size);
        written += chunk_size;
    }
}

/// @brief Calculates the total length (data_len + digits of total length).
/// @param data_len The initial length of your data payload.
/// @return The final total length including its own digit count.
mptar_uint32 calculate_framed_len(mptar_uint32 data_len) {
    mptar_uint32 total_len = data_len;
    int previous_digits = 0;

    while (1) {

#ifdef MPTAR_UNIVERSAL_FRAME_COUNTING
        int current_digits = 0;
        mptar_uint32 temp = total_len;
        
        if (temp == 0) {
            current_digits = 1;
        } else {
            while (temp > 0) {
                current_digits++;
                temp /= 10;
            }
        }
#else // Should be faster and handles most cases
        int current_digits = 1;
        
        if (total_len >= 100000)      current_digits = 6;
        else if (total_len >= 10000)  current_digits = 5;
        else if (total_len >= 1000)   current_digits = 4;
        else if (total_len >= 100)    current_digits = 3;
        else if (total_len >= 10)     current_digits = 2;
#endif

        if (current_digits == previous_digits) {
            break; 
        }

        previous_digits = current_digits;
        total_len = data_len + current_digits;
    }

    return total_len;
}

char* u64_to_tar_octal(mptar_uint64 value, char* str, mptar_size_t str_size)
{
    char temp[str_size];
    memcpy(&temp, str, str_size); 

    char* octal_str = u64too(value, temp, str_size);
    mptar_size_t octal_str_len = strlen(octal_str);
    
    memset(str, '0', str_size);
    memcpy(&str[str_size - octal_str_len - 1], octal_str, octal_str_len + 1);
    return str;
}

int write_tar_octal(char* dst, mptar_uint64 value, mptar_size_t size){
    char str_buf[size];
    char* str = u64_to_tar_octal(value, str_buf, size);
    strcpy(dst, str);
}

int write_basic_path(char* header_name, char* header_prefix, char* full_path)
{
    mptar_size_t path_len = strlen(full_path);
    if(path_len <= TAR_NAME_LENGTH){
        strcpy(header_name, full_path);
        return 0;
    }

    char* current = full_path;
    const char* end_boundary = full_path + TAR_PREFIX_LENGTH; 
    const char* last_safe_slash = MPTAR_NULL;

    if (header_prefix != MPTAR_NULL){
        while (*current != '\0' && current < end_boundary) {
            if (*current == '/') {
                last_safe_slash = current;
            }
            current++;
        }

        if(last_safe_slash != MPTAR_NULL && last_safe_slash[1] == '\0'){ // Directory path
            const char* previous = last_safe_slash - 1;
            while(previous > full_path && *previous != '/'){
                previous--;
            }

            if(*previous == '/'){
                last_safe_slash = previous;
            }else{
                last_safe_slash = MPTAR_NULL;
            }
        }
    }

    if(last_safe_slash == MPTAR_NULL || safe_bounded_strlen(last_safe_slash + 1, 100) > 99){
        memcpy(header_name, full_path, TAR_NAME_SIZE);
        header_name[TAR_NAME_LENGTH] = '\0';
        current = header_name + TAR_NAME_LENGTH - 1;
        end_boundary = header_name;
        while (current >= end_boundary)
        {
            if(*current == '/'){
                *current = '\0';
            }else{
                break;
            }
            current--;
        }
        return 1;
    }else{
        mptar_size_t prefix_len = last_safe_slash - full_path;
        memcpy(header_prefix, full_path, prefix_len);
        header_prefix[prefix_len] = '\0';
        strcpy(header_name, last_safe_slash+1);
    }
    return 0;
}

void write_tar_checksum(char* header_block) {
    unsigned int sum = 0;    
    for (int i = 0; i < 512; i++) {
        // 8 checksum field bytes (148 to 155) have to be treated as spaces (ASCII for 32)
        if (i == 148) {
            sum += 256; // 32 * 8
            i = 155;
        } else {
            sum += (unsigned char)header_block[i];
        }
    }

    write_tar_octal(header_block + 148, (mptar_uint64)sum, 7);
    header_block[155] = ' ';
}

int write_base_header(tar_header* header, mptar_metadata* meta){
#ifndef MPTAR_CLEAN_ALLOC
    memset(header, 0, 512);
#endif
    if(meta->typeflag == '1' || meta->typeflag == '2'){
        write_basic_path(header->linkname, MPTAR_NULL, meta->link_target); 
        write_basic_path(header->name, header->prefix, meta->path);
    }else{
        write_basic_path(header->name, header->prefix, meta->path);
    }
    write_tar_octal(header->gid, (mptar_uint64)meta->gid, 8);
    write_tar_octal(header->uid, (mptar_uint64)meta->uid, 8);
    write_tar_octal(header->mode, (mptar_uint64)meta->mode, 8);

    mptar_uint64 size_to_write = meta->size;
    if (size_to_write >= TAR_MAX_SIZE) {
        size_to_write = 0;
    }
    write_tar_octal(header->size, size_to_write, 12);
    write_tar_octal(header->modtime, (mptar_uint64)meta->modtime, 12);
    header->typeflag = meta->typeflag;
    memcpy(header->magic, "ustar\0", 6);
    memcpy(header->version, "00", 2);
    write_tar_checksum(header);
    return 0;
}

int write_pax_header(mptar_writer* ctx, mptar_uint64 size, mptar_metadata* meta){
    char* header_bytes = ctx->cfg.alloc(ctx->cfg.alloc_user_data, 512);
    tar_header* header = (tar_header*)header_bytes;

    mptar_metadata temp_meta = {0};
    temp_meta.path = "PaxHeader/dump";
    temp_meta.size = size; // size of pax payload
    temp_meta.mode = meta->mode;
    temp_meta.uid = meta->uid;
    temp_meta.gid = meta->gid;
    temp_meta.modtime = meta->modtime;
    temp_meta.typeflag = 'x';
    write_base_header(header, &temp_meta);
    ctx->cfg.write(ctx->cfg.write_user_data, header_bytes, 512);
    ctx->cfg.free(ctx->cfg.alloc_user_data, header_bytes);
    return 0;
}

int mptar_write_header(mptar_writer* ctx, const mptar_metadata* meta){
    mptar_size_t path_size = strlen(meta->path);
    mptar_size_t link_target_size = meta->link_target == MPTAR_NULL ? 0 : strlen(meta->link_target);
    bool large_path = (path_size > TAR_NAME_LENGTH);
    bool large_size = (meta->size >= TAR_MAX_SIZE);
    bool large_link_target = (link_target_size > TAR_LINKNAME_LENGTH);
    bool need_pax = large_path || large_size || large_link_target;

    if(need_pax)
    {
        mptar_uint64 pax_header_size = 0;
        char size_str_buf[21] = {0};
        mptar_size_t size_str_len = 0;
        mptar_uint32 pax_path_size = 0;
        mptar_uint32 pax_link_target_size = 0;
        mptar_uint32 pax_size_size = 0;

        if (large_path) {
            pax_path_size = calculate_framed_len(path_size + PAX_PATH_KEYWORD_SIZE);
            pax_header_size += pax_path_size;
        }

        if (large_size) {
            u64toa(meta->size, size_str_buf, 21);
            size_str_len = strlen(size_str_buf);
            pax_size_size = calculate_framed_len(size_str_len + PAX_SIZE_KEYWORD_SIZE);
            pax_header_size += pax_size_size;
        }

        if(large_link_target){
            pax_link_target_size = calculate_framed_len(link_target_size + PAX_LINK_PATH_KEYWORD_SIZE);
            pax_header_size += pax_link_target_size;
        }

        write_pax_header(ctx, pax_header_size, meta);

        if (large_path) {
            char path_pax_str[11];
            u64toa(pax_path_size, path_pax_str, sizeof(path_pax_str));
            ctx->cfg.write(ctx->cfg.write_user_data, path_pax_str, strlen(path_pax_str));
            ctx->cfg.write(ctx->cfg.write_user_data, " path=", 6);
            ctx->cfg.write(ctx->cfg.write_user_data, meta->path, path_size);
            ctx->cfg.write(ctx->cfg.write_user_data, "\n", 1);
        }

        if (large_size) {
            char size_pax_str[11];
            u64toa(pax_size_size, size_pax_str, sizeof(size_pax_str));
            ctx->cfg.write(ctx->cfg.write_user_data, size_pax_str, strlen(size_pax_str));
            ctx->cfg.write(ctx->cfg.write_user_data, " size=", 6);
            ctx->cfg.write(ctx->cfg.write_user_data, size_str_buf, size_str_len);
            ctx->cfg.write(ctx->cfg.write_user_data, "\n", 1);
        }

        if(large_link_target){
            char linkpath_pax_str[11];
            u64toa(pax_link_target_size, linkpath_pax_str, sizeof(linkpath_pax_str));
            ctx->cfg.write(ctx->cfg.write_user_data, linkpath_pax_str, strlen(linkpath_pax_str));
            ctx->cfg.write(ctx->cfg.write_user_data, " linkpath=", 10);
            ctx->cfg.write(ctx->cfg.write_user_data, meta->link_target, link_target_size);
            ctx->cfg.write(ctx->cfg.write_user_data, "\n", 1);
        }

        mptar_size_t rounded_size = (pax_header_size + 511) & ~511;
        mptar_size_t padding_needed = rounded_size - pax_header_size;
        stream_zeroes(ctx, padding_needed);
    }

    char* header_bytes = ctx->cfg.alloc(ctx->cfg.alloc_user_data, 512);
    tar_header* header = (tar_header*)header_bytes;
    write_base_header(header, meta);
    ctx->cfg.write(ctx->cfg.write_user_data, header_bytes, 512);
    ctx->cfg.free(ctx->cfg.alloc_user_data, header_bytes);
    ctx->bytes_left = meta->size;

    return 0;
}

int mptar_write_data_chunk(mptar_writer *ctx, const void *buffer, mptar_size_t size)
{
    if(size > ctx->bytes_left){
        return -1; // Too much was requested to be written.
    }

    mptar_size_t written = ctx->cfg.write(ctx->cfg.write_user_data, buffer, size);
    if(written != size) return -2; // Error from write fn

    ctx->bytes_left -= size;

    return 0;
}

int mptar_write_finalize(mptar_writer *ctx, const mptar_metadata *meta)
{
    if (ctx->bytes_left > 0) {
        return -1; // Not all bytes were written
    }

    mptar_size_t rounded_size = (meta->size + 511) & ~511;
    mptar_size_t padding_needed = rounded_size - meta->size;

    if (padding_needed > 0) {
        stream_zeroes(ctx, padding_needed);
    }

    return 0;
}

int mptar_close_archive(mptar_writer *ctx)
{
    stream_zeroes(ctx, 1024);
    return 0;
}
