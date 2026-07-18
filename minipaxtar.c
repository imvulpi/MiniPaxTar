#include "minipaxtar.h"

#ifdef MPTAR_NO_STD

mptar_size_t strlen(const char* str){
    mptar_size_t len;
    for (len = 0 ;; len++)
    {
        if(str[len] == '\0') return len;   
    }
}

mptar_size_t strnlen(const char* str, mptar_size_t max_limit) {
    mptar_size_t count = 0;
    while (count < max_limit && str[count] != '\0') {
        count++;
    }
    return count;
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

int memcmp(const void* s1, const void* s2, mptar_size_t size) {
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;

    for (mptar_size_t i = 0; i < size; i++) {
        if (p1[i] != p2[i]) {
            return (p1[i] < p2[i]) ? -1 : 1;
        }
    }

    return 0;
}

#else

#include <string.h>
#include <stdlib.h>

#endif

#ifndef MPTAR_CUSTOM_IU64TOA

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

void i64toa(mptar_int64 value, char* buf, int buf_size) {
    if (value < 0) {
        if (buf_size > 1) {
            buf[0] = '-';
            mptar_uint64 abs_val = ~(mptar_uint64)value + 1; 
            u64toa(abs_val, buf + 1, buf_size - 1);
        }
    } else {
        u64toa((mptar_uint64)value, buf, buf_size);
    }
}

#else

extern char* u64toa(mptar_uint64 value, char* str, mptar_size_t str_size);
extern void i64toa(mptar_int64 value, char* buf, int buf_size);

#endif

void stream_zeroes(mptar_writer* ctx, mptar_size_t padding_needed) {
    const char zero_chunk[16] = {0}; 
    mptar_size_t written = 0;

    while (written < padding_needed) {
        mptar_size_t remaining = padding_needed - written;
        mptar_size_t chunk_size = (remaining > 16) ? 16 : remaining;

        ctx->write(ctx->write_user_data, zero_chunk, chunk_size);
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
    if (str_size == 0) return str;
    memset(str, '0', str_size);
    str[str_size - 1] = '\0';
    
    mptar_size_t i = str_size - 2;
    
    if (value == 0) {
        str[i] = '0';
        return str;
    }

    while (value > 0 && i < str_size) {
        str[i] = '0' + (char)(value & 7);
        value >>= 3;
        
        if (value > 0) {
            if (i == 0) {
                break;
            }
            i--;
        }
    }

    return str;
}

char* format_pax_timestamp(mptar_int64 sec, mptar_uint32 nsec, char* str, mptar_size_t size){
    i64toa(sec, str, size);
    mptar_size_t int_size = strlen(str);

    if (nsec == 0 || int_size + 2 >= size) { // needs space for '.' and a digit
        return str;
    }

    str[int_size] = '.';
    if(size - int_size < 11){
        return str;
    }

    char* nsec_start = str + int_size + 1;
    u64toa(nsec, nsec_start, 10);
    mptar_size_t nsec_len = strlen(nsec_start);
    if (nsec_len < 9) {
        mptar_size_t shift = 9 - nsec_len;
        
        for (mptar_size_t i = nsec_len; i > 0; i--) {
            nsec_start[i - 1 + shift] = nsec_start[i - 1];
        }
        
        for (mptar_size_t i = 0; i < shift; i++) {
            nsec_start[i] = '0';
        }
    }

    mptar_size_t end = 8;
    while (end > 0 && nsec_start[end] == '0') {
        nsec_start[end] = '\0';
        end--;
    }

    nsec_start[9] = '\0';

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

    if(last_safe_slash == MPTAR_NULL || strnlen(last_safe_slash + 1, 100) > 99){
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

mptar_uint32 calculate_checksum(char* header_block){
    mptar_uint32 sum = 0;    
    for (int i = 0; i < 512; i++) {
        // 8 checksum field bytes (148 to 155) have to be treated as spaces (ASCII for 32)
        if (i == 148) {
            sum += 256; // 32 * 8
            i = 155;
        } else {
            sum += (unsigned char)header_block[i];
        }
    }
    return sum;
}

void write_tar_checksum(char* header_block) {
    mptar_int32 sum = calculate_checksum(header_block);
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

#ifdef MPTAR_SUPPORT_SPECIAL
    if (meta->typeflag == '3' || meta->typeflag == '4') {
        write_tar_octal(header->devmajor, meta->devmajor, 8);
        write_tar_octal(header->devminor, meta->devminor, 8);
    }
#endif
    
    mptar_uint64 size_to_write = meta->size;
    if (size_to_write >= TAR_MAX_SIZE) {
        size_to_write = 0;
    }
    write_tar_octal(header->size, size_to_write, 12);

    if(meta->mtime.has_value){
        write_tar_octal(header->mtime, (mptar_uint64)meta->mtime.value.sec, 12); // Does not support negative numbers
    }
    header->typeflag = meta->typeflag;
    memcpy(header->magic, "ustar\0", 6);
    memcpy(header->version, "00", 2);
    write_tar_checksum(header);
    return 0;
}

int write_pax_header(mptar_writer* ctx, mptar_uint64 size, mptar_metadata* meta){
    char* header_bytes = ctx->memory.alloc(ctx->memory.alloc_user_data, 512);
    tar_header* header = (tar_header*)header_bytes;

    mptar_metadata temp_meta = {0};
    temp_meta.path = "PaxHeader/dump";
    temp_meta.size = size; // size of pax payload
    temp_meta.mode = meta->mode;
    temp_meta.uid = meta->uid;
    temp_meta.gid = meta->gid;
    temp_meta.mtime = meta->mtime;
    temp_meta.typeflag = 'x';
    write_base_header(header, &temp_meta);
    ctx->write(ctx->write_user_data, header_bytes, 512);
    ctx->memory.free(ctx->memory.alloc_user_data, header_bytes);
    return 0;
}

int mptar_write_header(mptar_writer* ctx, const mptar_metadata* meta){
#ifndef MPTAR_SUPPORT_SPECIAL
    if (meta->typeflag == '3' || meta->typeflag == '4') {
        return -3; // Special file are not supported in this build because dev minor/major is disabled.
    }
#endif
    
    mptar_size_t path_size = strlen(meta->path);
    mptar_size_t link_target_size = meta->link_target == MPTAR_NULL ? 0 : strlen(meta->link_target);
    bool large_path = (path_size > TAR_NAME_LENGTH);
    bool large_size = (meta->size >= TAR_MAX_SIZE);
    bool large_link_target = (link_target_size > TAR_LINKNAME_LENGTH);
#ifdef MPTAR_SUPPORT_EXTRA_TIMES
    bool times_need_pax = (meta->mtime.has_value && (meta->mtime.value.sec < 0 || meta->mtime.value.nsec > 0)) || meta->atime.has_value || meta->ctime.has_value;
#else
    bool times_need_pax = meta->mtime.has_value && (meta->mtime.value.sec < 0 || meta->mtime.value.nsec > 0);
#endif
    bool need_pax = large_path || large_size || large_link_target || times_need_pax;

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

        mptar_uint32 pax_mtime_size = 0;
        char mtime_str_buf[32];
        mptar_size_t mtime_str_len;

        if(meta->mtime.has_value && meta->mtime.value.sec < 0 || meta->mtime.value.nsec > 0){
            format_pax_timestamp(meta->mtime.value.sec, meta->mtime.value.nsec, mtime_str_buf, 32);
            mtime_str_len = strlen(mtime_str_buf);
            pax_mtime_size = calculate_framed_len(mtime_str_len + PAX_TIMES_KEYWORD_SIZE);
            pax_header_size += pax_mtime_size;
        }

#ifdef MPTAR_SUPPORT_EXTRA_TIMES
        mptar_uint32 pax_atime_size = 0;
        mptar_uint32 pax_ctime_size = 0;
        char atime_str_buf[32];
        char ctime_str_buf[32];
        mptar_size_t atime_str_len;
        mptar_size_t ctime_str_len;

        if(meta->atime.has_value){
            format_pax_timestamp(meta->atime.value.sec, meta->atime.value.nsec, atime_str_buf, 32);
            atime_str_len = strlen(atime_str_buf);
            pax_atime_size = calculate_framed_len(atime_str_len + PAX_TIMES_KEYWORD_SIZE);
            pax_header_size += pax_atime_size;
        }

        if(meta->ctime.has_value){
            format_pax_timestamp(meta->ctime.value.sec, meta->ctime.value.nsec, ctime_str_buf, 32);
            ctime_str_len = strlen(ctime_str_buf);
            pax_ctime_size = calculate_framed_len(ctime_str_len + PAX_TIMES_KEYWORD_SIZE);
            pax_header_size += pax_ctime_size;
        }
#endif

        write_pax_header(ctx, pax_header_size, meta);

        if (large_path) {
            char path_pax_str[11];
            u64toa(pax_path_size, path_pax_str, sizeof(path_pax_str));
            ctx->write(ctx->write_user_data, path_pax_str, strlen(path_pax_str));
            ctx->write(ctx->write_user_data, " path=", 6);
            ctx->write(ctx->write_user_data, meta->path, path_size);
            ctx->write(ctx->write_user_data, "\n", 1);
        }

        if (large_size) {
            char size_pax_str[11];
            u64toa(pax_size_size, size_pax_str, sizeof(size_pax_str));
            ctx->write(ctx->write_user_data, size_pax_str, strlen(size_pax_str));
            ctx->write(ctx->write_user_data, " size=", 6);
            ctx->write(ctx->write_user_data, size_str_buf, size_str_len);
            ctx->write(ctx->write_user_data, "\n", 1);
        }

        if(large_link_target){
            char linkpath_pax_str[11];
            u64toa(pax_link_target_size, linkpath_pax_str, sizeof(linkpath_pax_str));
            ctx->write(ctx->write_user_data, linkpath_pax_str, strlen(linkpath_pax_str));
            ctx->write(ctx->write_user_data, " linkpath=", 10);
            ctx->write(ctx->write_user_data, meta->link_target, link_target_size);
            ctx->write(ctx->write_user_data, "\n", 1);
        }

        if(meta->mtime.has_value && meta->mtime.value.sec < 0 || meta->mtime.value.nsec > 0){
            char mtime_pax_str[11];
            u64toa(pax_mtime_size, mtime_pax_str, sizeof(mtime_pax_str));
            ctx->write(ctx->write_user_data, mtime_pax_str, strlen(mtime_pax_str));
            ctx->write(ctx->write_user_data, " mtime=", 7);
            ctx->write(ctx->write_user_data, mtime_str_buf, mtime_str_len);
            ctx->write(ctx->write_user_data, "\n", 1);
        }

#ifdef MPTAR_SUPPORT_EXTRA_TIMES
        if(meta->atime.has_value){
            char atime_pax_str[11];
            u64toa(pax_atime_size, atime_pax_str, sizeof(atime_pax_str));
            ctx->write(ctx->write_user_data, atime_pax_str, strlen(atime_pax_str));
            ctx->write(ctx->write_user_data, " atime=", 7);
            ctx->write(ctx->write_user_data, atime_str_buf, atime_str_len);
            ctx->write(ctx->write_user_data, "\n", 1);
        }

        if(meta->ctime.has_value){
            char ctime_pax_str[11];
            u64toa(pax_ctime_size, ctime_pax_str, sizeof(ctime_pax_str));
            ctx->write(ctx->write_user_data, ctime_pax_str, strlen(ctime_pax_str));
            ctx->write(ctx->write_user_data, " ctime=", 7);
            ctx->write(ctx->write_user_data, ctime_str_buf, ctime_str_len);
            ctx->write(ctx->write_user_data, "\n", 1);
        }
#endif

        mptar_size_t rounded_size = (pax_header_size + 511) & ~511;
        mptar_size_t padding_needed = rounded_size - pax_header_size;
        stream_zeroes(ctx, padding_needed);
    }

    char* header_bytes = ctx->memory.alloc(ctx->memory.alloc_user_data, 512);
    tar_header* header = (tar_header*)header_bytes;
    write_base_header(header, meta);
    ctx->write(ctx->write_user_data, header_bytes, 512);
    ctx->memory.free(ctx->memory.alloc_user_data, header_bytes);
    ctx->bytes_left = meta->size;

    return 0;
}

int mptar_write_data_chunk(mptar_writer *ctx, const void *buffer, mptar_size_t size)
{
    if(size > ctx->bytes_left){
        return -1; // Too much was requested to be written.
    }

    mptar_size_t written = ctx->write(ctx->write_user_data, buffer, size);
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

mptar_uint64 tar_octal_to_u64(const char* str, mptar_size_t str_size) {
    mptar_uint64 result = 0;
    mptar_size_t i = 0;

    while (i < str_size && (str[i] == ' ' || str[i] == '0')) {
        i++;
    }

    while (i < str_size) {
        char c = str[i];

        if (c == '\0' || c == ' ') {
            break;
        }

        if (c < '0' || c > '7') {
            break;
        }

        result = (result << 3) + (c - '0');
        i++;
    }

    return result;
}

int verify_header_checksum(const tar_header* header) {
    const unsigned char* bytes = (const unsigned char*)header;
    mptar_uint32 unsigned_sum = 0;
    mptar_int32 signed_sum = 0;

    if (header->typeflag == '\0' && header->name[0] == '\0') {
        mptar_size_t zero_chars = 0;
        for (mptar_size_t i = 0; i < 16; i++) {
            if (bytes[i] == 0) zero_chars++;
        }
        if (zero_chars == 16) {
            return -2; // EOF
        }
    }

    for (mptar_size_t i = 0; i < 512; i++) {
        if (i == 148) { // 8 checksum field bytes (148 to 155) have to be treated as spaces (ASCII for 32)
            unsigned_sum += 256; // 32 * 8
            signed_sum += 256;
            i = 155;
        } else {
            unsigned_sum += bytes[i];
            signed_sum += (signed char)bytes[i];
        }
    }

    mptar_uint32 stored_checksum = (mptar_uint32)tar_octal_to_u64(header->checksum, 8);

    if (unsigned_sum == stored_checksum || (mptar_uint32)signed_sum == stored_checksum) {
        return 0;
    }

    return -1; // CHECKSUM FAIL
}

static mptar_uint64 atou64(const char* str, mptar_size_t len) {
    mptar_uint64 val = 0;
    for (mptar_size_t i = 0; i < len; i++) {
        if (str[i] < '0' || str[i] > '9') break;
        val = (val * 10) + (str[i] - '0');
    }
    return val;
}

static void parse_pax_time(const char* val_str, mptar_size_t val_len, mptar_opt_time* out_time) {
    mptar_size_t dot_i = 0;
    while (dot_i < val_len && val_str[dot_i] != '.') {
        dot_i++;
    }

    out_time->value.sec = (mptar_int64)atou64(val_str, dot_i);
    out_time->value.nsec = 0;

    if (dot_i < val_len && val_str[dot_i] == '.') {
        mptar_size_t frac_start = dot_i + 1;
        mptar_size_t frac_len = val_len - frac_start;
        
        // up to 9 digits of ns
        mptar_uint32 nsec = 0;
        mptar_uint32 multiplier = 100000000;
        
        for (mptar_size_t i = 0; i < frac_len && i < 9; i++) {
            char c = val_str[frac_start + i];
            if (c >= '0' && c <= '9') {
                nsec += (c - '0') * multiplier;
            }
            multiplier /= 10;
        }
        out_time->value.nsec = nsec;
    }
    out_time->has_value = true;
}

static int reconstruct_ustar_path(mptar_reader* reader, const tar_header* header, mptar_metadata* out_meta) {
    mptar_size_t name_len = strnlen(header->name, 100);
    mptar_size_t prefix_len = strnlen(header->prefix, 155);

    if (prefix_len > 0) {
        mptar_size_t full_len = prefix_len + 1 + name_len;
        char* allocated_path = (char*)reader->memory.alloc(reader->memory.alloc_user_data, full_len + 1);
        if (!allocated_path) return -4; // Out of memory

        memcpy(allocated_path, header->prefix, prefix_len);
        allocated_path[prefix_len] = '/';
        memcpy(allocated_path + prefix_len + 1, header->name, name_len);
        allocated_path[full_len] = '\0';
        out_meta->path = allocated_path;
    } else if (name_len > 0) {
        char* allocated_path = (char*)reader->memory.alloc(reader->memory.alloc_user_data, name_len + 1);
        if (!allocated_path) return -4;

        memcpy(allocated_path, header->name, name_len);
        allocated_path[name_len] = '\0';
        out_meta->path = allocated_path;
    }
    
    return 0;
}

static const char* alloc_pax_string(mptar_memory_cfg* mem, const char* src, mptar_size_t len) {
    char* dst = (char*)mem->alloc(mem->alloc_user_data, len + 1);
    if (!dst) return MPTAR_NULL;
    memcpy(dst, src, len);
    dst[len] = '\0';
    return dst;
}

static void mptar_apply_pax_kv(mptar_reader* reader, const char* key, mptar_size_t key_len, 
    const char* val, mptar_size_t val_len, mptar_metadata* meta, mptar_uint32* pax_flags) {
    
    if (key_len == 4 && memcmp(key, "path", 4) == 0) {
        meta->path = alloc_pax_string(&reader->memory, val, val_len);
        *pax_flags |= MPTAR_PAX_HAS_PATH;
    } 
    else if (key_len == 8 && memcmp(key, "linkpath", 8) == 0) {
        meta->link_target = alloc_pax_string(&reader->memory, val, val_len);
        *pax_flags |= MPTAR_PAX_HAS_LINK;
    }
    else if (key_len == 4 && memcmp(key, "size", 4) == 0) {
        meta->size = atou64(val, val_len);
        *pax_flags |= MPTAR_PAX_HAS_SIZE;
    } 
    else if (key_len == 3 && memcmp(key, "uid", 3) == 0) {
        meta->uid = (mptar_uint32)atou64(val, val_len);
        *pax_flags |= MPTAR_PAX_HAS_UID;
    } 
    else if (key_len == 3 && memcmp(key, "gid", 3) == 0) {
        meta->gid = (mptar_uint32)atou64(val, val_len);
        *pax_flags |= MPTAR_PAX_HAS_GID;
    }
    else if (key_len == 5 && memcmp(key, "mtime", 5) == 0) {
        parse_pax_time(val, val_len, &meta->mtime);
        *pax_flags |= MPTAR_PAX_HAS_MTIME;
    }
#ifdef MPTAR_SUPPORT_EXTRA_TIMES
    else if (key_len == 5 && memcmp(key, "atime", 5) == 0) {
        parse_pax_time(val, val_len, &meta->atime);
        *pax_flags |= MPTAR_PAX_HAS_ATIME;
    } 
    else if (key_len == 5 && memcmp(key, "ctime", 5) == 0) {
        parse_pax_time(val, val_len, &meta->ctime);
        *pax_flags |= MPTAR_PAX_HAS_CTIME;
    }
#endif
}

int mptar_parse_pax_block(mptar_reader* reader, mptar_uint64 total_pax_size, mptar_metadata* meta, mptar_uint32* pax_flags) {
    if (total_pax_size == 0) return 0;

    char* buffer = (char*)reader->memory.alloc(reader->memory.alloc_user_data, (mptar_size_t)total_pax_size);
    if (!buffer) return -1; // Out of memory

    if (reader->read(reader->read_user_data, buffer, (mptar_size_t)total_pax_size) != total_pax_size) {
        reader->memory.free(reader->memory.alloc_user_data, buffer);
        return -3; // Read error
    }
    reader->offset += total_pax_size;

    // reads the remaining padding
    mptar_uint64 remainder = total_pax_size % 512;
    if (remainder > 0) {
        char dummy[512];
        mptar_size_t pad = 512 - (mptar_size_t)remainder;
        reader->read(reader->read_user_data, dummy, pad);
        reader->offset += pad;
    }

    mptar_size_t i = 0;
    while (i < total_pax_size) {
        mptar_size_t len_start = i;
        while (i < total_pax_size && buffer[i] != ' ') {
            i++;
        }
        if (i >= total_pax_size) break;
        
        mptar_uint64 record_len = atou64(&buffer[len_start], i - len_start);
        i++; // Advances past space

        mptar_size_t key_start = i;
        while (i < total_pax_size && buffer[i] != '=') {
            i++;
        }
        if (i >= total_pax_size) break;
        
        mptar_size_t key_len = i - key_start;
        i++; // Advances past '='

        mptar_size_t val_start = i;
        mptar_size_t target_next_record = len_start + (mptar_size_t)record_len;
        mptar_size_t val_len = target_next_record - val_start - 1; // Subtract 1 for '\n'

        mptar_apply_pax_kv(reader, &buffer[key_start], key_len, &buffer[val_start], val_len, meta, pax_flags);

        i = target_next_record;
    }

    reader->memory.free(reader->memory.alloc_user_data, buffer);
    return 0;
}

static void mptar_parse_base_ustar(const tar_header* header, mptar_metadata* out_meta) {
    out_meta->size = tar_octal_to_u64(header->size, 12);
    out_meta->mode = (mptar_uint32)tar_octal_to_u64(header->mode, 8);
    out_meta->uid  = (mptar_uint32)tar_octal_to_u64(header->uid, 8);
    out_meta->gid  = (mptar_uint32)tar_octal_to_u64(header->gid, 8);
    out_meta->typeflag = header->typeflag;
    
    out_meta->mtime.value.sec = (mptar_int64)tar_octal_to_u64(header->mtime, 12);
    out_meta->mtime.value.nsec = 0;
    out_meta->mtime.has_value = true;

#ifdef MPTAR_SUPPORT_EXTRA_TIMES
    out_meta->atime.has_value = false;
    out_meta->ctime.has_value = false;
#endif

#ifdef MPTAR_SUPPORT_SPECIAL
    out_meta->dev_major = (mptar_uint32)tar_octal_to_u64(header->devmajor, 8);
    out_meta->dev_minor = (mptar_uint32)tar_octal_to_u64(header->devminor, 8);
#endif
}

static mptar_size_t mptar_consume_stream_bytes(mptar_reader* reader, mptar_size_t count) {
    if (count == 0) return 0;

    char discard_buf[128];
    mptar_size_t total_consumed = 0;

    while (total_consumed < count) {
        mptar_size_t chunk = count - total_consumed;
        if (chunk > sizeof(discard_buf)) {
            chunk = sizeof(discard_buf);
        }

        mptar_size_t bytes_read = reader->read(reader->read_user_data, discard_buf, chunk);
        if (bytes_read == 0) {
            break; // Unexpected stream termination
        }

        total_consumed += bytes_read;
        reader->offset += bytes_read;
    }

    return total_consumed;
}

int mptar_read_header(mptar_reader *reader, mptar_metadata *out_meta) {
    out_meta->path = 0;
    out_meta->link_target = 0;

    mptar_uint32 pax_flags = 0;
    char header_bytes[512];
    
    while (1) {
        if (reader->read(reader->read_user_data, header_bytes, 512) != 512) {
            return -3;
        }
        reader->offset += 512;

        tar_header* header = (tar_header*)header_bytes;
        int res = verify_header_checksum(header);
        if (res != 0) return res;

        if (header->typeflag == 'x') {
            mptar_uint64 pax_size = tar_octal_to_u64(header->size, 12);            
            int pax_res = mptar_parse_pax_block(reader, pax_size, out_meta, &pax_flags);
            if (pax_res != 0) return pax_res;

            continue;
        }

        mptar_metadata pax_staged = *out_meta;
        mptar_parse_base_ustar(header, out_meta);

        if (pax_flags & MPTAR_PAX_HAS_SIZE)  { out_meta->size = pax_staged.size; }
        if (pax_flags & MPTAR_PAX_HAS_UID)   { out_meta->uid  = pax_staged.uid;  }
        if (pax_flags & MPTAR_PAX_HAS_GID)   { out_meta->gid  = pax_staged.gid;  }
        if (pax_flags & MPTAR_PAX_HAS_MTIME) { 
            out_meta->mtime = pax_staged.mtime; 
        }

#ifdef MPTAR_SUPPORT_EXTRA_TIMES
        if (pax_flags & MPTAR_PAX_HAS_ATIME) { out_meta->atime = pax_staged.atime; }
        if (pax_flags & MPTAR_PAX_HAS_CTIME) { out_meta->ctime = pax_staged.ctime; }
#endif

        if (pax_flags & MPTAR_PAX_HAS_PATH) {
            out_meta->path = pax_staged.path;
        } else {
            int path_res = reconstruct_ustar_path(reader, header, out_meta);
            if (path_res != 0) return path_res;
        }

        if (pax_flags & MPTAR_PAX_HAS_LINK) {
            out_meta->link_target = pax_staged.link_target;
        } else {
            mptar_size_t link_len = strnlen(header->linkname, 100);
            if (link_len > 0) {
                char* allocated_link = (char*)reader->memory.alloc(reader->memory.alloc_user_data, link_len + 1);
                if (!allocated_link) return -4; // Out Of Memory
                memcpy(allocated_link, header->linkname, link_len);
                allocated_link[link_len] = '\0';
                out_meta->link_target = allocated_link;
            }
        }

        reader->bytes_left = out_meta->size;
        break;
    }

    return 0;
}

mptar_size_t mptar_read_data_chunk(mptar_reader* reader, void* buffer, mptar_size_t size) {
    if (!reader || !buffer || size == 0 || reader->bytes_left == 0) {
        return 0;
    }

    mptar_size_t bytes_to_read = size;
    if (bytes_to_read > reader->bytes_left) {
        bytes_to_read = reader->bytes_left;
    }

    mptar_size_t bytes_read = reader->read(reader->read_user_data, buffer, bytes_to_read);
    
    reader->bytes_left -= bytes_read;
    reader->offset += bytes_read;

    // discards padding
    if (reader->bytes_left == 0) {
        mptar_size_t remainder = reader->offset % 512;
        if (remainder > 0) {
            mptar_size_t padding_needed = 512 - remainder;
            mptar_consume_stream_bytes(reader, padding_needed);
        }
    }

    return bytes_read;
}

int mptar_skip_data(mptar_reader* reader) {
    if (!reader) {
        return -1; // invalid arg
    }

    mptar_size_t payload_left = reader->bytes_left;
    if (payload_left > 0) {
        mptar_size_t skipped = mptar_consume_stream_bytes(reader, payload_left);
        if (skipped < payload_left) {
            return -2; // eof
        }
        reader->bytes_left = 0;
    }

    // padding to be cleared
    mptar_size_t remainder = reader->offset % 512;
    if (remainder > 0) {
        mptar_size_t padding_needed = 512 - remainder;
        mptar_size_t skipped_pad = mptar_consume_stream_bytes(reader, padding_needed);
        if (skipped_pad < padding_needed) {
            return -2; // eof
        }
    }

    return 0;
}