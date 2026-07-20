#include "minipaxtar.h"

#ifndef MPTAR_NO_STD

#include <string.h>
#include <stdlib.h>

#define mptar_strlen  strlen
#define mptar_strnlen strnlen
#define mptar_strncpy strncpy
#define mptar_memcpy  memcpy
#define mptar_memset  memset
#define mptar_memcmp  memcmp

#else

static mptar_size_t mptar_strlen(const char* str) {
    if (!str) return 0;
    mptar_size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

static mptar_size_t mptar_strnlen(const char* str, mptar_size_t max_limit) {
    if (!str) return 0;
    mptar_size_t count = 0;
    while (count < max_limit && str[count] != '\0') {
        count++;
    }
    return count;
}

static char* mptar_strncpy(char* dst, const char* src, mptar_size_t n) {
    if (dst == MPTAR_NULL || n == 0) return dst;
    
    mptar_size_t i = 0;
    
    if (src != MPTAR_NULL) {
        while (i < n && src[i] != '\0') {
            dst[i] = src[i];
            i++;
        }
    }
    
    while (i < n) {
        dst[i] = '\0';
        i++;
    }
    
    return dst;
}

static void mptar_memcpy(void* dest, const void* src, mptar_size_t n) {
    if (!dest || !src || n == 0) return;
    
    char* d = (char*)dest;
    const char* s = (const char*)src;
    
    for (mptar_size_t i = 0; i < n; i++) {
        d[i] = s[i];
    }
}

static void mptar_memset(void* data, char value, mptar_size_t amount) {
    if (!data || amount == 0) return;
    
    char* bytes = (char*)data;
    for (mptar_size_t i = 0; i < amount; i++) {
        bytes[i] = value;
    }
}

static int mptar_memcmp(const void* s1, const void* s2, mptar_size_t size) {
    if (!s1 || !s2 || size == 0) return 0;
    
    const unsigned char* p1 = (const unsigned char*)s1;
    const unsigned char* p2 = (const unsigned char*)s2;

    for (mptar_size_t i = 0; i < size; i++) {
        if (p1[i] != p2[i]) {
            return (p1[i] < p2[i]) ? -1 : 1;
        }
    }

    return 0;
}

#endif /* MPTAR_NO_STD */

#ifndef MPTAR_WITHOUT_WRITE

#if defined(MPTAR_CUSTOM_IU64TOA)
    #define MPTAR_CUSTOM_U64TOA
    #define MPTAR_CUSTOM_I64TOA
#endif

#ifndef MPTAR_CUSTOM_U64TOA

static char* mptar_u64toa(mptar_uint64 value, char* str, mptar_size_t str_size, int *out_err){
    if (out_err) *out_err = MPTAR_OK;
    if (str == MPTAR_NULL || str_size < 2) {
        if (out_err) *out_err = MPTAR_ERR_INVALID_ARG;
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
        if (out_err) *out_err = MPTAR_ERR_OVERFLOW;
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

#else

extern char* mptar_u64toa(mptar_uint64 value, char* str, mptar_size_t str_size, int *out_err);

#endif

#ifndef MPTAR_CUSTOM_I64TOA

static char* mptar_i64toa(mptar_int64 value, char* buf, mptar_size_t buf_size, int *out_err) {
    if (out_err) *out_err = MPTAR_OK;
    
    if (buf == MPTAR_NULL || buf_size <= 1) {
        if (out_err) *out_err = MPTAR_ERR_INVALID_ARG;
        return MPTAR_NULL;
    }

    if (value < 0) {
        mptar_uint64 abs_val;
        if (value == MPTAR_INT64_MIN) {
            abs_val = (mptar_uint64)MPTAR_INT64_MAX + 1ULL;
        } else {
            abs_val = (mptar_uint64)(-value);
        }
        
        char* result = mptar_u64toa(abs_val, buf + 1, buf_size - 1, out_err);
        if (result == MPTAR_NULL) {
            return MPTAR_NULL;
        }

        buf[0] = '-';
        return buf;
    } else {
        return mptar_u64toa((mptar_uint64)value, buf, buf_size, out_err);
    }
}

#else

extern char* mptar_i64toa(mptar_int64 value, char* buf, mptar_size_t buf_size, int *out_err);

#endif

static int mptar_stream_write_zeroes(mptar_writer* ctx, mptar_size_t padding_needed) {
    if (!ctx) return MPTAR_ERR_INVALID_ARG;
    if (padding_needed == 0) return MPTAR_OK;

    const char zero_chunk[16] = {0}; 
    mptar_size_t total_written = 0;

    while (total_written < padding_needed) {
        mptar_size_t remaining = padding_needed - total_written;
        mptar_size_t chunk_size = (remaining > 16) ? 16 : remaining;

        mptar_size_t res = ctx->write(ctx->write_user_data, zero_chunk, chunk_size);
        if (res == 0 || res > chunk_size) return MPTAR_ERR_IO_WRITE;
        total_written += chunk_size;
    }

    return MPTAR_OK;
}

static mptar_uint32 mptar_pax_calculate_record_len(mptar_uint32 data_len) {
    if (data_len > 4294967285U) return 0; // ERR: Would overflow

    mptar_uint32 digits = 1;
    if (data_len >= 1000000000U) digits = 10;
    else if (data_len >= 100000000U) digits = 9;
    else if (data_len >= 10000000U)  digits = 8;
    else if (data_len >= 1000000U)   digits = 7;
    else if (data_len >= 100000U)    digits = 6;
    else if (data_len >= 10000U)     digits = 5;
    else if (data_len >= 1000U)      digits = 4;
    else if (data_len >= 100U)       digits = 3;
    else if (data_len >= 10U)        digits = 2;

    mptar_uint32 total = data_len + digits;
    
    mptar_uint32 final_digits = 1;
    if (total >= 1000000000U) final_digits = 10;
    else if (total >= 100000000U) final_digits = 9;
    else if (total >= 10000000U)  final_digits = 8;
    else if (total >= 1000000U)   final_digits = 7;
    else if (total >= 100000U)    final_digits = 6;
    else if (total >= 10000U)     final_digits = 5;
    else if (total >= 1000U)      final_digits = 4;
    else if (total >= 100U)       final_digits = 3;
    else if (total >= 10U)        final_digits = 2;

    return data_len + final_digits;
}

static char* mptar_pax_format_time(mptar_int64 sec, mptar_uint32 nsec, char* str, mptar_size_t size, int *out_err){
    if (out_err) *out_err = MPTAR_OK;
    if(str == MPTAR_NULL || size == 0){
        if(out_err) *out_err = MPTAR_ERR_INVALID_ARG;
        return MPTAR_NULL;
    }

    int error = MPTAR_OK;
    mptar_i64toa(sec, str, size, &error);
    if(error != MPTAR_OK) {
        if(out_err) *out_err = error;
        return MPTAR_NULL;
    }

    if (nsec == 0) {
        return str;
    }

    mptar_size_t int_size = mptar_strlen(str);

    char nsec_buf[10] = {0}; // Space for 9 digits + \0
    int nsec_err = MPTAR_OK;
    mptar_u64toa(nsec, nsec_buf, sizeof(nsec_buf), &nsec_err);
    if (nsec_err != MPTAR_OK) {
        if(out_err) *out_err = MPTAR_ERR_NS_CONVERSION_FAILED;
        return str; 
    }

    mptar_size_t nsec_len = mptar_strlen(nsec_buf);
    char formatted_nsec[10];
    mptar_size_t leading_zeros = 9 - nsec_len;

    for (mptar_size_t i = 0; i < leading_zeros; i++) {
        formatted_nsec[i] = '0';
    }
    for (mptar_size_t i = 0; i < nsec_len; i++) {
        formatted_nsec[leading_zeros + i] = nsec_buf[i];
    }
    formatted_nsec[9] = '\0';

    int end = 8;
    while (end > 0 && formatted_nsec[end] == '0') {
        formatted_nsec[end] = '\0';
        end--;
    }
    mptar_size_t final_nsec_len = mptar_strlen(formatted_nsec);

    if (int_size + 1 + final_nsec_len + 1 > size) { // int string + . + ns str + \0
        return str;
    }

    str[int_size] = '.';
    for (mptar_size_t i = 0; i < final_nsec_len; i++) {
        str[int_size + 1 + i] = formatted_nsec[i];
    }
    str[int_size + 1 + final_nsec_len] = '\0';

    return str;
}

static int mptar_pax_stream_record(mptar_writer* ctx, mptar_uint32 record_size, const char* keyword, const char* value, mptar_size_t value_len) {
    char size_str[12];
    mptar_u64toa(record_size, size_str, sizeof(size_str), MPTAR_NULL);
    mptar_size_t size_len = mptar_strlen(size_str);
    mptar_size_t kw_len = mptar_strlen(keyword);

    // [size] [space] [keyword] [=] [value] [\n]
    if (ctx->write(ctx->write_user_data, size_str, size_len) != size_len) return MPTAR_ERR_IO_WRITE;
    if (ctx->write(ctx->write_user_data, " ", 1) != 1) return MPTAR_ERR_IO_WRITE;
    if (ctx->write(ctx->write_user_data, keyword, kw_len) != kw_len) return MPTAR_ERR_IO_WRITE;
    if (ctx->write(ctx->write_user_data, "=", 1) != 1) return MPTAR_ERR_IO_WRITE;
    if (ctx->write(ctx->write_user_data, value, value_len) != value_len) return MPTAR_ERR_IO_WRITE;
    if (ctx->write(ctx->write_user_data, "\n", 1) != 1) return MPTAR_ERR_IO_WRITE;

    return MPTAR_OK;
}

static void mptar_write_octal_field(char* dst, mptar_uint64 value, mptar_size_t size)
{
    if(dst == MPTAR_NULL || size == 0) return;

    mptar_size_t octal_digits = size - 1; 
    mptar_uint64 max_octal_val = 0;
    
    if (octal_digits >= 22) {
        max_octal_val = MPTAR_UINT64_MAX;
    } else {
        max_octal_val = ((mptar_uint64)1 << (octal_digits * 3)) - 1;
    }

    if (value > max_octal_val) {
        mptar_size_t i = size;

        while(i > 0){
            dst[--i] = (char)(value & 0xFF);
            value >>= 8;
        }

        dst[0] |= 0x80;
        return;
    }

    mptar_memset(dst, '0', size);
    dst[size - 1] = '\0';

    mptar_size_t i = octal_digits;
    while (i > 0) {
        dst[--i] = '0' + (value & 7);
        value >>= 3;
    }

    dst[octal_digits] = '\0';
}

static mptar_uint32 mptar_calculate_header_checksum(char* header_block){
    if (header_block == MPTAR_NULL) return 0;

    const unsigned char* block = (const unsigned char*)header_block;
    mptar_uint32 sum = 0;    

    for (int i = 0; i < 148; i++) {
        sum += block[i];
    }

    // Hardcoded constant value for the 8 space characters (8 * 32)
    sum += 256; 

    for (int i = 156; i < 512; i++) {
        sum += block[i];
    }
    
    return sum;
}

static void mptar_write_header_checksum(char* header_block) {
    if (header_block == MPTAR_NULL) return;
    mptar_uint32 sum = mptar_calculate_header_checksum(header_block);
    mptar_write_octal_field(header_block + 148, sum, 7);
    header_block[155] = ' ';
}

static bool mptar_can_path_fit_ustar(const char* path, mptar_size_t len) {
    if (!path) return false;

    if (len <= MPTAR_USTAR_SIZE_NAME) {
        return true;
    }else if (len > (MPTAR_USTAR_SIZE_PREFIX + 1 + MPTAR_USTAR_SIZE_NAME)) {
        return false; 
    }

    for (mptar_size_t i = len - 1; i > 0; i--) {
        mptar_size_t name_len = len - i - 1;
        
        if (name_len > MPTAR_USTAR_SIZE_NAME) {
            break; 
        }

        if (path[i] == '/') {
            mptar_size_t prefix_len = i;
            
            if (prefix_len <= MPTAR_USTAR_SIZE_PREFIX && name_len <= MPTAR_USTAR_SIZE_NAME) {
                return true; 
            }
        }
    }

    return false;
}

static int mptar_write_ustar_path(char* header_name, char* header_prefix, char* full_path)
{
    if(header_name == MPTAR_NULL || full_path == MPTAR_NULL) return MPTAR_ERR_INVALID_ARG; // header_prefix can be null.

    mptar_size_t path_len = mptar_strlen(full_path);
    if (path_len <= MPTAR_USTAR_SIZE_NAME) {
        mptar_memcpy(header_name, full_path, path_len);
        if (path_len < MPTAR_USTAR_SIZE_NAME) {
            header_name[path_len] = '\0';
        }
        if (header_prefix != MPTAR_NULL) {
            header_prefix[0] = '\0';
        }
        return MPTAR_OK;
    }

    if (header_prefix == MPTAR_NULL) {
        mptar_memcpy(header_name, full_path, MPTAR_USTAR_SIZE_NAME);
        return MPTAR_NEEDS_PAX;
    }

    if (path_len > (MPTAR_USTAR_SIZE_PREFIX + 1 + MPTAR_USTAR_SIZE_NAME)) {
        mptar_memcpy(header_name, full_path, MPTAR_USTAR_SIZE_NAME);
        return MPTAR_NEEDS_PAX;
    }

    int split_index = -1;
    for (mptar_size_t i = 0; i < path_len; i++) {
        if (full_path[i] == '/') {
            mptar_size_t prefix_part_len = i;
            mptar_size_t name_part_len = path_len - i - 1;

            if (prefix_part_len <= MPTAR_USTAR_SIZE_PREFIX && name_part_len <= MPTAR_USTAR_SIZE_NAME) {
                if (name_part_len == 0 && split_index != -1) {
                    continue; 
                }
                split_index = (int)i;
            }
        }
    }

    if (split_index == -1) {
        mptar_memcpy(header_name, full_path, MPTAR_USTAR_SIZE_NAME);
        return MPTAR_NEEDS_PAX;
    }

    mptar_size_t final_prefix_len = (mptar_size_t)split_index;
    mptar_size_t final_name_len = path_len - final_prefix_len - 1;

    mptar_memcpy(header_prefix, full_path, final_prefix_len);
    if (final_prefix_len < MPTAR_USTAR_SIZE_PREFIX) {
        header_prefix[final_prefix_len] = '\0';
    }

    mptar_memcpy(header_name, full_path + final_prefix_len + 1, final_name_len);
    if (final_name_len < MPTAR_USTAR_SIZE_NAME) {
        header_name[final_name_len] = '\0';
    }

    return MPTAR_OK;
}

static int mptar_write_ustar_header(mptar_header* header, mptar_metadata* meta){
    if (header == MPTAR_NULL || meta == MPTAR_NULL) {
        return MPTAR_ERR_INVALID_ARG;
    }
    if (meta->path == MPTAR_NULL) {
        return MPTAR_ERR_INVALID_ARG;
    }

    mptar_memset(header, 0, 512);
    
    int result_status = MPTAR_OK;

    if (meta->typeflag == '1' || meta->typeflag == '2') {
        if (meta->link_target != MPTAR_NULL) {
            int link_res = mptar_write_ustar_path(header->linkname, MPTAR_NULL, (char*)meta->link_target);
            if (link_res == MPTAR_NEEDS_PAX) {
                result_status = MPTAR_NEEDS_PAX;
            }
        }
    }

    int path_res = mptar_write_ustar_path(header->name, header->prefix, (char*)meta->path);
    if (path_res == MPTAR_NEEDS_PAX) {
        result_status = MPTAR_NEEDS_PAX;
    }
    
    if (meta->uname != MPTAR_NULL) {
        mptar_strncpy(header->uname, meta->uname, 32);
        header->uname[31] = '\0';
    }

    if (meta->gname != MPTAR_NULL) {
        mptar_strncpy(header->gname, meta->gname, 32);
        header->gname[31] = '\0';
    }

    mptar_write_octal_field(header->gid, meta->gid, 8);
    mptar_write_octal_field(header->uid, meta->uid, 8);
    mptar_write_octal_field(header->mode, (mptar_uint64)meta->mode, 8);

#ifdef MPTAR_SUPPORT_SPECIAL
    if (meta->typeflag == '3' || meta->typeflag == '4') {
        mptar_write_octal_field(header->devmajor, meta->devmajor, 8);
        mptar_write_octal_field(header->devminor, meta->devminor, 8);
    }
#endif
    
    mptar_uint64 size_to_write = meta->size;
    if (size_to_write >= MPTAR_USTAR_MAX_OCTAL_12B) {
        size_to_write = 0;
        result_status = MPTAR_NEEDS_PAX;
    }
    mptar_write_octal_field(header->size, size_to_write, 12);

    if(meta->mtime.has_value){
        mptar_write_octal_field(header->mtime, (mptar_uint64)meta->mtime.value.sec, 12);
    }

    header->typeflag = meta->typeflag;
    mptar_memcpy(header->magic, "ustar\0", 6);
    mptar_memcpy(header->version, "00", 2);
    mptar_write_header_checksum((char*)header);

    return result_status;
}

static int mptar_write_pax_header(mptar_writer* ctx, mptar_uint64 size, mptar_metadata* meta){
    if (ctx == MPTAR_NULL || meta == MPTAR_NULL) {
        return MPTAR_ERR_INVALID_ARG;
    }

    char* header_bytes = ctx->memory.alloc(ctx->memory.alloc_user_data, 512);
    if(header_bytes == MPTAR_NULL){
        return MPTAR_ERR_ALLOC;
    }

    mptar_header* header = (mptar_header*)header_bytes;
    mptar_metadata temp_meta = {0};

    temp_meta.path = "PaxHeader/dump";
    temp_meta.size = size; // Size of pax payload
    temp_meta.mode = meta->mode;
    temp_meta.uid = meta->uid;
    temp_meta.gid = meta->gid;
    temp_meta.mtime = meta->mtime;
    temp_meta.typeflag = 'x';

    mptar_write_ustar_header(header, &temp_meta);
    
    mptar_size_t bytes_written = ctx->write(ctx->write_user_data, header_bytes, 512);
    ctx->memory.free(ctx->memory.alloc_user_data, header_bytes);

    if (bytes_written != 512) {
        return MPTAR_ERR_IO_WRITE;
    }

    return MPTAR_OK;
}

int mptar_write_header(mptar_writer* ctx, const mptar_metadata* meta){
    if (ctx == MPTAR_NULL || meta == MPTAR_NULL || meta->path == MPTAR_NULL || 
        ctx->memory.alloc == MPTAR_NULL || ctx->memory.free == MPTAR_NULL || ctx->write == MPTAR_NULL) {
        return MPTAR_ERR_INVALID_ARG;
    }

#ifndef MPTAR_SUPPORT_SPECIAL
    if (meta->typeflag == '3' || meta->typeflag == '4') {
        return MPTAR_ERR_UNSUPPORTED_TYPE;
    }
#endif
    
    mptar_size_t path_len = mptar_strlen(meta->path);
    mptar_size_t link_target_size = (meta->link_target == MPTAR_NULL) ? 0 : mptar_strlen(meta->link_target);
    mptar_size_t uname_len = (meta->uname == MPTAR_NULL) ? 0 : mptar_strlen(meta->uname);
    mptar_size_t gname_len = (meta->gname == MPTAR_NULL) ? 0 : mptar_strlen(meta->gname);

    bool large_path = !mptar_can_path_fit_ustar(meta->path, path_len);
    bool large_size = ((ctx->flags & MPTAR_CTX_ALLOW_PAX_FOR_OCTAL) != 0) && (meta->size >= MPTAR_USTAR_MAX_OCTAL_12B);
    bool large_link_target = (link_target_size > MPTAR_USTAR_MAX_LEN_LINKNAME);
    bool uid_needs_pax = (meta->uid > MPTAR_USTAR_MAX_OCTAL_8B) && (((ctx->flags & MPTAR_CTX_ALLOW_PAX_FOR_OCTAL) != 0) || meta->uid > MPTAR_BINARY_MAX_8B);
    bool gid_needs_pax = (meta->gid > MPTAR_USTAR_MAX_OCTAL_8B) && (((ctx->flags & MPTAR_CTX_ALLOW_PAX_FOR_OCTAL) != 0) || meta->gid > MPTAR_BINARY_MAX_8B);
    bool uname_needs_pax = (uname_len > MPTAR_USTAR_MAX_LEN_UNAME);
    bool gname_needs_pax = (gname_len > MPTAR_USTAR_MAX_LEN_GNAME);

    bool mtime_needs_pax = meta->mtime.has_value && 
        (meta->mtime.value.sec < 0 || meta->mtime.value.nsec > 0
        || (meta->mtime.value.sec > MPTAR_USTAR_MAX_OCTAL_12B && ((ctx->flags & MPTAR_CTX_ALLOW_PAX_FOR_OCTAL) != 0)));
    
    #ifdef MPTAR_SUPPORT_EXTRA_TIMES
        bool times_need_pax = mtime_needs_pax || meta->atime.has_value || meta->ctime.has_value;
    #else
        bool times_need_pax = mtime_needs_pax;
    #endif

    bool need_pax = large_path || large_size || large_link_target || uid_needs_pax || gid_needs_pax || uname_needs_pax || gname_needs_pax || times_need_pax;
    if(need_pax) {
        mptar_uint64 pax_header_size = 0;
        char size_str_buf[21] = {0};
        mptar_size_t size_str_len = 0;
        
        mptar_uint32 pax_path_size = 0;
        mptar_uint32 pax_size_size = 0;
        mptar_uint32 pax_link_target_size = 0;
        mptar_uint32 pax_mtime_size = 0;
        char mtime_str_buf[32];
        mptar_size_t mtime_str_len = 0;
        char uid_str_buf[24];
        char gid_str_buf[24];
        mptar_size_t uid_str_len = 0;
        mptar_size_t gid_str_len = 0;

        mptar_size_t pax_uid_size = 0;
        mptar_size_t pax_gid_size = 0;
        mptar_size_t pax_uname_size = 0;
        mptar_size_t pax_gname_size = 0;

#ifdef MPTAR_SUPPORT_EXTRA_TIMES
        mptar_uint32 pax_atime_size = 0;
        mptar_uint32 pax_ctime_size = 0;
        char atime_str_buf[32];
        char ctime_str_buf[32];
        mptar_size_t atime_str_len = 0;
        mptar_size_t ctime_str_len = 0;
#endif

        if (large_path) {
            pax_path_size = mptar_pax_calculate_record_len(path_len + MPTAR_PAX_OVERHEAD_PATH);
            pax_header_size += pax_path_size;
        }

        if (large_size) {
            mptar_u64toa(meta->size, size_str_buf, sizeof(size_str_buf), MPTAR_NULL);
            size_str_len = mptar_strlen(size_str_buf);
            pax_size_size = mptar_pax_calculate_record_len(size_str_len + MPTAR_PAX_OVERHEAD_SIZE);
            pax_header_size += pax_size_size;
        }
        
        if (large_link_target) {
            pax_link_target_size = mptar_pax_calculate_record_len(link_target_size + MPTAR_PAX_OVERHEAD_LINK);
            pax_header_size += pax_link_target_size;
        }

        if (mtime_needs_pax) {
            mptar_pax_format_time(meta->mtime.value.sec, meta->mtime.value.nsec, mtime_str_buf, sizeof(mtime_str_buf), MPTAR_NULL);
            mtime_str_len = mptar_strlen(mtime_str_buf);
            pax_mtime_size = mptar_pax_calculate_record_len(mtime_str_len + MPTAR_PAX_OVERHEAD_TIME);
            pax_header_size += pax_mtime_size;
        }

#ifdef MPTAR_SUPPORT_EXTRA_TIMES
        if (meta->atime.has_value) {
            mptar_pax_format_time(meta->atime.value.sec, meta->atime.value.nsec, atime_str_buf, sizeof(atime_str_buf), MPTAR_NULL);
            atime_str_len = mptar_strlen(atime_str_buf);
            pax_atime_size = mptar_pax_calculate_record_len(atime_str_len + MPTAR_PAX_OVERHEAD_TIME);
            pax_header_size += pax_atime_size;
        }

        if (meta->ctime.has_value) {
            mptar_pax_format_time(meta->ctime.value.sec, meta->ctime.value.nsec, ctime_str_buf, sizeof(ctime_str_buf), MPTAR_NULL);
            ctime_str_len = mptar_strlen(ctime_str_buf);
            pax_ctime_size = mptar_pax_calculate_record_len(ctime_str_len + MPTAR_PAX_OVERHEAD_TIME);
            pax_header_size += pax_ctime_size;
        }
#endif

        if (uid_needs_pax) {
            mptar_u64toa(meta->uid, uid_str_buf, sizeof(uid_str_buf), MPTAR_NULL);
            uid_str_len = mptar_strlen(uid_str_buf);
            pax_uid_size = mptar_pax_calculate_record_len(uid_str_len + MPTAR_PAX_OVERHEAD_UID);
            pax_header_size += pax_uid_size;
        }

        if (gid_needs_pax) {
            mptar_u64toa(meta->gid, gid_str_buf, sizeof(gid_str_buf), MPTAR_NULL);
            gid_str_len = mptar_strlen(gid_str_buf);
            pax_gid_size = mptar_pax_calculate_record_len(gid_str_len + MPTAR_PAX_OVERHEAD_GID);
            pax_header_size += pax_gid_size;
        }

        if (uname_needs_pax) {
            pax_uname_size = mptar_pax_calculate_record_len(uname_len + MPTAR_PAX_OVERHEAD_UNAME);
            pax_header_size += pax_uname_size;
        }

        if (gname_needs_pax) {
            pax_gname_size = mptar_pax_calculate_record_len(gname_len + MPTAR_PAX_OVERHEAD_GNAME);
            pax_header_size += pax_gname_size;
        }

        int res = mptar_write_pax_header(ctx, pax_header_size, (mptar_metadata*)meta);
        if (res != MPTAR_OK) return res;

        if (large_path) {
            res = mptar_pax_stream_record(ctx, pax_path_size, "path", meta->path, path_len);
            if (res != MPTAR_OK) return res;
        }

        if (large_size) {
            res = mptar_pax_stream_record(ctx, pax_size_size, "size", size_str_buf, size_str_len);
            if (res != MPTAR_OK) return res;
        }

        if (large_link_target) {
            res = mptar_pax_stream_record(ctx, pax_link_target_size, "linkpath", meta->link_target, link_target_size);
            if (res != MPTAR_OK) return res;
        }

        if (mtime_needs_pax) {
            res = mptar_pax_stream_record(ctx, pax_mtime_size, "mtime", mtime_str_buf, mtime_str_len);
            if (res != MPTAR_OK) return res;
        }
        
#ifdef MPTAR_SUPPORT_EXTRA_TIMES
        if (meta->atime.has_value) {
            res = mptar_pax_stream_record(ctx, pax_atime_size, "atime", atime_str_buf, atime_str_len);
            if (res != MPTAR_OK) return res;
        }

        if (meta->ctime.has_value) {
            res = mptar_pax_stream_record(ctx, pax_ctime_size, "ctime", ctime_str_buf, ctime_str_len);
            if (res != MPTAR_OK) return res;
        }
#endif

        if (uid_needs_pax) {
            res = mptar_pax_stream_record(ctx, pax_uid_size, "uid", uid_str_buf, uid_str_len);
            if (res != MPTAR_OK) return res;
        }

        if (gid_needs_pax) {
            res = mptar_pax_stream_record(ctx, pax_gid_size, "gid", gid_str_buf, gid_str_len);
            if (res != MPTAR_OK) return res;
        }

        if (uname_needs_pax) {
            res = mptar_pax_stream_record(ctx, pax_uname_size, "uname", meta->uname, uname_len);
            if (res != MPTAR_OK) return res;
        }

        if (gname_needs_pax) {
            res = mptar_pax_stream_record(ctx, pax_gname_size, "gname", meta->gname, gname_len);
            if (res != MPTAR_OK) return res;
        }

        mptar_size_t remainder = (mptar_size_t)(pax_header_size % 512);
        mptar_size_t padding_needed = (remainder > 0) ? (512 - remainder) : 0;
        if (padding_needed > 0) {
            res = mptar_stream_write_zeroes(ctx, padding_needed);
            if (res != MPTAR_OK) return res;
        }
    }

    char* header_bytes = ctx->memory.alloc(ctx->memory.alloc_user_data, 512);
    if (header_bytes == MPTAR_NULL) {
        return MPTAR_ERR_ALLOC;
    }

    mptar_header* header = (mptar_header*)header_bytes;
    mptar_write_ustar_header(header, (mptar_metadata*)meta);

    mptar_size_t written = ctx->write(ctx->write_user_data, header_bytes, 512);
    ctx->memory.free(ctx->memory.alloc_user_data, header_bytes);

    if (written != 512) {
        return MPTAR_ERR_IO_WRITE;
    }

    ctx->bytes_left = meta->size;

    return MPTAR_OK;
}

mptar_size_t mptar_write_data_chunk(mptar_writer *ctx, const void *buffer, mptar_size_t size, int *out_err)
{
    if(out_err) *out_err = MPTAR_OK;
    if (size == 0 || ctx->bytes_left == 0) return 0;
    if (!ctx || !buffer) {
        if (out_err) *out_err = MPTAR_ERR_INVALID_ARG;
        return 0;
    }
    
    mptar_size_t bytes_to_write = size;
    if (bytes_to_write > ctx->bytes_left) {
        bytes_to_write = ctx->bytes_left;
    }

    mptar_size_t written = ctx->write(ctx->write_user_data, buffer, bytes_to_write);
    
    if (written == 0 || written > bytes_to_write) {
        if (out_err) *out_err = MPTAR_ERR_IO_WRITE;
        return 0; 
    }

    ctx->bytes_left -= written;
    return written;
}

int mptar_write_finalize(mptar_writer *ctx, const mptar_metadata *meta)
{
    if (!ctx || !meta) return MPTAR_ERR_INVALID_ARG;
    if (ctx->bytes_left > 0) {
        return MPTAR_ERR_INCOMPLETE_PAYLOAD;
    }

    mptar_size_t remainder = (mptar_size_t)(meta->size % 512);
    mptar_size_t padding_needed = (remainder > 0) ? (512 - remainder) : 0;

    if (padding_needed > 0) {
        int err = mptar_stream_write_zeroes(ctx, padding_needed);
        if (err != MPTAR_OK) {
            return err;
        }
    }

    return MPTAR_OK;
}

int mptar_close_archive(mptar_writer *ctx)
{
    if (!ctx) return MPTAR_ERR_INVALID_ARG;
    
    int err = mptar_stream_write_zeroes(ctx, 1024);
    if (err != MPTAR_OK) {
        return err;
    }

    return MPTAR_OK;
}

#endif /* MPTAR_WITHOUT_WRITE */

#ifndef MPTAR_WITHOUT_READ

#if defined(MPTAR_CUSTOM_ATOIU64)
    #define MPTAR_CUSTOM_ATOU64
    #define MPTAR_CUSTOM_ATOI64
#endif

#ifndef MPTAR_CUSTOM_ATOU64

static mptar_uint64 mptar_atou64(const char* str, mptar_size_t len, int* err) {
    if(err) *err = MPTAR_OK;
    if(str == MPTAR_NULL || len == 0){
        if(err) *err = MPTAR_ERR_INVALID_ARG;
        return 0;
    }

    const unsigned char* bytes = (const unsigned char*)str;
    mptar_uint64 val = 0;
    mptar_size_t digits_parsed = 0;

    for (mptar_size_t i = 0; i < len; i++) {
        unsigned char c = bytes[i];
        if (c < '0' || c > '9') break;

        if (val > (MPTAR_UINT64_MAX / 10)) {
            if(err) *err = MPTAR_ERR_OVERFLOW;
            return MPTAR_UINT64_MAX;
        }

        mptar_uint64 next_val = val * 10;
        unsigned char digit = c - '0';

        if (next_val > (MPTAR_UINT64_MAX - digit)) {
            if(err) *err = MPTAR_ERR_OVERFLOW;
            return MPTAR_UINT64_MAX;
        }

        val = next_val + digit;
        digits_parsed++;
    }

    if(digits_parsed == 0){
        if(err) *err = MPTAR_ERR_MALFORMED;
        return 0;
    }

    return val;
}

#else

extern mptar_uint64 mptar_atou64(const char* str, mptar_size_t len, int* err);

#endif

#ifndef MPTAR_CUSTOM_ATOI64

static mptar_int64 mptar_atoi64(const char* str, mptar_size_t len, int* err) {
    if (err) *err = MPTAR_OK;
    if (str == MPTAR_NULL || len == 0) {
        if(err) *err = MPTAR_ERR_INVALID_ARG;
        return 0;
    }

    bool is_negative = false;
    mptar_size_t start_idx = 0;

    if (str[0] == '-') {
        is_negative = true;
        start_idx = 1;
    } else if (str[0] == '+') {
        start_idx = 1;
    }

    if (start_idx >= len) {
        if (err) *err = MPTAR_ERR_MALFORMED;
        return 0;
    }

    int atou_error = 0;
    mptar_uint64 u_val = mptar_atou64(str + start_idx, len - start_idx, &atou_error);
    if (err) *err = atou_error;
    if (atou_error != MPTAR_OK && atou_error != MPTAR_ERR_OVERFLOW) return 0;

    if (is_negative) {
        mptar_uint64 max_negative_abs = (mptar_uint64)MPTAR_INT64_MAX + 1ULL;

        if (atou_error == MPTAR_ERR_OVERFLOW || u_val >= max_negative_abs) {
            return MPTAR_INT64_MIN;
        }

        return -(mptar_int64)u_val;
    } else {
        if (atou_error == MPTAR_ERR_OVERFLOW || u_val > (mptar_uint64)MPTAR_INT64_MAX) {
            return MPTAR_INT64_MAX;
        }

        return (mptar_int64)u_val;
    }
}

#else

extern mptar_int64 mptar_atoi64(const char* str, mptar_size_t len, int* err);

#endif

static mptar_size_t mptar_consume_stream(mptar_reader* reader, mptar_size_t count) {
    if (count == 0) return 0;

    char discard_buf[128];
    mptar_size_t total_consumed = 0;

    while (total_consumed < count) {
        mptar_size_t remaining = count - total_consumed;
        mptar_size_t chunk = (remaining > sizeof(discard_buf)) ? sizeof(discard_buf) : remaining;

        mptar_size_t bytes_read = reader->read(reader->read_user_data, discard_buf, chunk);
        if (bytes_read == 0) {
            break;
        }

        total_consumed += bytes_read;
        reader->offset += bytes_read;
    }

    return total_consumed;
}

static int mptar_align_stream(mptar_reader* reader) {
    if (reader->bytes_left != 0) return MPTAR_OK;

    mptar_size_t remainder = reader->offset % 512;
    if (remainder > 0) {
        mptar_size_t padding_needed = 512 - remainder;
        mptar_size_t skipped = mptar_consume_stream(reader, padding_needed);
        
        if (skipped < padding_needed) {
            return MPTAR_ERR_IO_READ;
        }
    }
    return MPTAR_OK;
}

static mptar_uint64 mptar_parse_octal_field(const char* str, mptar_size_t str_size, int* err) {    
    if (str == MPTAR_NULL || str_size == 0) {
        if(err) *err = MPTAR_ERR_INVALID_ARG;
        return 0;
    }

    const unsigned char* bytes = (const unsigned char*)str;

    if((bytes[0] & 0x80) != 0){
        mptar_uint64 result = 0;
        result = bytes[0] & 0x7F;

        for (mptar_size_t i = 1; i < str_size; i++)
        {
            if (result > (MPTAR_UINT64_MAX >> 8)) {
                if (err) *err = MPTAR_ERR_OVERFLOW;
                return MPTAR_UINT64_MAX;
            }
            
            result = (result << 8) | bytes[i];
        }

        if (err) *err = MPTAR_OK;
        return result;
    }

    mptar_uint64 result = 0;
    mptar_size_t i = 0;

    while (i < str_size && (bytes[i] == ' ' || bytes[i] == '0')) {
        i++;
    }

    while (i < str_size) {
        unsigned char c = bytes[i];

        if (c == '\0' || c == ' ') {
            break;
        }

        if (c < '0' || c > '7') {
            if(err) *err = MPTAR_ERR_MALFORMED;
            return result; 
        }

        if (result > (MPTAR_UINT64_MAX >> 3)) {
            if(err) *err = MPTAR_ERR_OVERFLOW;
            return MPTAR_UINT64_MAX; 
        }

        result = (result << 3) + (c - '0');
        i++;
    }

    while (i < str_size) {
        unsigned char c = bytes[i];
        if (c != '\0' && c != ' ') {
            if(err) *err = MPTAR_ERR_MALFORMED;
            return result; 
        }
        i++;
    }

    if(err) *err = MPTAR_OK;
    return result;
}

static int mptar_verify_header_checksum(const mptar_header* header) {
    if(header == MPTAR_NULL) return MPTAR_ERR_INVALID_ARG;

    const unsigned char* bytes = (const unsigned char*)header;

    mptar_uint32 stored_checksum = (mptar_uint32)mptar_parse_octal_field(header->checksum, 8, MPTAR_NULL);
    if (header->typeflag == '\0' && stored_checksum == 0 && header->name[0] == '\0') {
        static const char zero_magic_version[8] = {0};
        if (mptar_memcmp(header->magic, zero_magic_version, 8) == 0) {
            return MPTAR_EOF;
        }
    }

    mptar_uint32 unsigned_sum = 0;
    mptar_int32 signed_sum = 0;

    for (int i = 0; i < 148; i++) {
        unsigned_sum += bytes[i];
        signed_sum += (signed char)bytes[i];
    }

    // Hardcoded constant value for the 8 space characters (8 * 32)
    unsigned_sum += 256; 
    signed_sum += 256;

    for (int i = 156; i < 512; i++) {
        unsigned_sum += bytes[i];
        signed_sum += (signed char)bytes[i];
    }

    if (unsigned_sum == stored_checksum || (mptar_uint32)signed_sum == stored_checksum) {
        return MPTAR_OK;
    }

    return MPTAR_ERR_CHECKSUM;
}

static int mptar_parse_pax_time(const char* val_str, mptar_size_t val_len, mptar_opt_time* out_time) {
    if (val_str == MPTAR_NULL || out_time == MPTAR_NULL || val_len == 0) {
        return MPTAR_ERR_INVALID_ARG;
    }

    const unsigned char* bytes = (const unsigned char*)val_str;
    mptar_size_t dot_i = 0;
    while (dot_i < val_len && bytes[dot_i] != '.') {
        dot_i++;
    }

    int status = MPTAR_OK;
    out_time->value.sec = mptar_atoi64(val_str, dot_i, &status);
    out_time->value.nsec = 0;
    out_time->has_value = true;

    if (dot_i < val_len && bytes[dot_i] == '.') {
        mptar_size_t frac_start = dot_i + 1;
        mptar_uint32 multiplier = 100000000;
        mptar_uint32 nsec = 0;
        mptar_size_t i = frac_start;

        while (i < val_len && (i - frac_start) < 9) {
            unsigned char c = bytes[i];
            if (c < '0' || c > '9') {
                status = (status == MPTAR_OK) ? MPTAR_ERR_MALFORMED : status;
                out_time->value.nsec = nsec;
                return status;
            }

            nsec += (mptar_uint32)(c - '0') * multiplier;
            multiplier /= 10;
            i++;
        }

        while (i < val_len) {
            unsigned char c = bytes[i];
            if (c < '0' || c > '9') {
                status = (status == MPTAR_OK) ? MPTAR_ERR_MALFORMED : status;
                out_time->value.nsec = nsec;
                return status;
            }
            i++;
        }

        out_time->value.nsec = nsec;
    }

    return status;
}

static int mptar_reconstruct_ustar_path(mptar_reader* reader, const mptar_header* header, mptar_metadata* out_meta) {
    if(reader == MPTAR_NULL || header == MPTAR_NULL || out_meta == MPTAR_NULL) return MPTAR_ERR_INVALID_ARG;
    out_meta->path = MPTAR_NULL;
    
    mptar_size_t name_len = mptar_strnlen(header->name, 100);
    mptar_size_t prefix_len = mptar_strnlen(header->prefix, 155);

    if (prefix_len > 0) {
        mptar_size_t full_len = prefix_len + 1 + name_len;
        char* allocated_path = (char*)reader->memory.alloc(reader->memory.alloc_user_data, full_len + 1);
        if (!allocated_path) return MPTAR_ERR_ALLOC;

        mptar_memcpy(allocated_path, header->prefix, prefix_len);
        allocated_path[prefix_len] = '/';
        mptar_memcpy(allocated_path + prefix_len + 1, header->name, name_len);
        allocated_path[full_len] = '\0';
        out_meta->path = allocated_path;
    } else if (name_len > 0) {
        char* allocated_path = (char*)reader->memory.alloc(reader->memory.alloc_user_data, name_len + 1);
        if (!allocated_path) return MPTAR_ERR_ALLOC;

        mptar_memcpy(allocated_path, header->name, name_len);
        allocated_path[name_len] = '\0';
        out_meta->path = allocated_path;
    }else{
        return MPTAR_ERR_MALFORMED;
    }
    
    return MPTAR_OK;
}

static const char* mptar_alloc_pax_string(mptar_memory_cfg* mem, const char* src, mptar_size_t len) {
    if (mem == MPTAR_NULL || src == MPTAR_NULL || len >= (mptar_size_t)-1) {
        return MPTAR_NULL;
    }

    char* dst = (char*)mem->alloc(mem->alloc_user_data, len + 1);
    if (!dst) return MPTAR_NULL;

    mptar_memcpy(dst, src, len);
    dst[len] = '\0';

    return dst;
}

static void mptar_apply_pax_string(mptar_memory_cfg* mem, const char* val, mptar_size_t val_len,
                                   const char** out_str, mptar_uint32* pax_flags, mptar_uint32 flag) {
#ifdef MPTAR_PAX_LAST_WINS
    if (*out_str != MPTAR_NULL && mem->free) {
        mem->free(mem->alloc_user_data, (void*)*out_str);
        *out_str = MPTAR_NULL;
    }
#else
    if (*pax_flags & flag) return;
#endif

    const char* allocated = mptar_alloc_pax_string(mem, val, val_len);
    if (allocated != MPTAR_NULL) {
        *out_str = allocated;
        *pax_flags |= flag;
    } else {
        *pax_flags &= ~flag; 
    }
}

static void mptar_apply_pax_u64(const char* val, mptar_size_t val_len, mptar_uint64* out_val,
                                mptar_uint32* pax_flags, mptar_uint32 flag) {
#ifndef MPTAR_PAX_LAST_WINS
    if (*pax_flags & flag) return;
#endif

    int error = MPTAR_OK;
    mptar_uint64 parsed = mptar_atou64(val, val_len, &error);
    if (error == MPTAR_OK || error == MPTAR_ERR_OVERFLOW) {
        *out_val = parsed;
        *pax_flags |= flag;
    }
}

static void mptar_apply_pax_time(const char* val, mptar_size_t val_len, mptar_opt_time* out_time,
                                 mptar_uint32* pax_flags, mptar_uint32 flag) {
#ifndef MPTAR_PAX_LAST_WINS
    if (*pax_flags & flag) return;
#endif

    int error = mptar_parse_pax_time(val, val_len, out_time);
    if (error == MPTAR_OK || error == MPTAR_ERR_MALFORMED) {
        *pax_flags |= flag;
    }
}

static void mptar_apply_pax_kv(mptar_reader* reader, const char* key, mptar_size_t key_len, 
    const char* val, mptar_size_t val_len, mptar_metadata* meta, mptar_uint32* pax_flags) {
    if (!reader || !key || !val || !meta || !pax_flags) {
        return;
    }
    
    if (key_len == 4 && mptar_memcmp(key, "path", 4) == 0) {
        mptar_apply_pax_string(&reader->memory, val, val_len, &meta->path, pax_flags, MPTAR_PAX_HAS_PATH);
    } 
    else if (key_len == 8 && mptar_memcmp(key, "linkpath", 8) == 0) {
        mptar_apply_pax_string(&reader->memory, val, val_len, &meta->link_target, pax_flags, MPTAR_PAX_HAS_LINK);
    }
    else if (key_len == 5 && mptar_memcmp(key, "uname", 5) == 0) {
        mptar_apply_pax_string(&reader->memory, val, val_len, &meta->uname, pax_flags, MPTAR_PAX_HAS_UNAME);
    }
    else if (key_len == 5 && mptar_memcmp(key, "gname", 5) == 0) {
        mptar_apply_pax_string(&reader->memory, val, val_len, &meta->gname, pax_flags, MPTAR_PAX_HAS_GNAME);
    }
    else if (key_len == 4 && mptar_memcmp(key, "size", 4) == 0) {
        mptar_apply_pax_u64(val, val_len, &meta->size, pax_flags, MPTAR_PAX_HAS_SIZE);
    } 
    else if (key_len == 3 && mptar_memcmp(key, "uid", 3) == 0) {
        mptar_apply_pax_u64(val, val_len, &meta->uid, pax_flags, MPTAR_PAX_HAS_UID);
    } 
    else if (key_len == 3 && mptar_memcmp(key, "gid", 3) == 0) {
        mptar_apply_pax_u64(val, val_len, &meta->gid, pax_flags, MPTAR_PAX_HAS_GID);
    }
    else if (key_len == 5 && mptar_memcmp(key, "mtime", 5) == 0) {
        mptar_apply_pax_time(val, val_len, &meta->mtime, pax_flags, MPTAR_PAX_HAS_MTIME);
    }
#ifdef MPTAR_SUPPORT_EXTRA_TIMES
    else if (key_len == 5 && mptar_memcmp(key, "atime", 5) == 0) {
        mptar_apply_pax_time(val, val_len, &meta->atime, pax_flags, MPTAR_PAX_HAS_ATIME);
    } 
    else if (key_len == 5 && mptar_memcmp(key, "ctime", 5) == 0) {
        mptar_apply_pax_time(val, val_len, &meta->ctime, pax_flags, MPTAR_PAX_HAS_CTIME);
    }
#endif
}

static int mptar_parse_pax_block(mptar_reader* reader, mptar_uint32 total_pax_size, mptar_metadata* meta, mptar_uint32* pax_flags) {
    if (!reader || !meta || !pax_flags) return MPTAR_ERR_INVALID_ARG;
    if (total_pax_size == 0) return MPTAR_OK;

    if (total_pax_size > (mptar_uint32)((mptar_size_t)-1)) {
        return MPTAR_ERR_OVERFLOW;
    }

    mptar_size_t size_t_pax = (mptar_size_t)total_pax_size;

    char* buffer = (char*)reader->memory.alloc(reader->memory.alloc_user_data, size_t_pax);
    if (!buffer) return MPTAR_ERR_ALLOC;

    if (reader->read(reader->read_user_data, buffer, size_t_pax) != size_t_pax) {
        reader->memory.free(reader->memory.alloc_user_data, buffer);
        return MPTAR_ERR_IO_READ;
    }
    reader->offset += size_t_pax;

    // Reads remaining block padding
    mptar_uint32 remainder = total_pax_size % 512;
    if (remainder > 0) {
        char dummy[512];
        mptar_size_t pad = 512 - (mptar_size_t)remainder;
        mptar_size_t read = reader->read(reader->read_user_data, dummy, pad);
        if (read != pad) {
            reader->memory.free(reader->memory.alloc_user_data, buffer);
            return MPTAR_ERR_IO_READ;
        }
        reader->offset += pad;
    }

    mptar_size_t i = 0;
    while (i < size_t_pax) {
        mptar_size_t len_start = i;
        while (i < size_t_pax && buffer[i] != ' ') {
            i++;
        }

        if (i >= size_t_pax) {
            reader->memory.free(reader->memory.alloc_user_data, buffer);
            return MPTAR_ERR_MALFORMED;
        }
        
        int error = MPTAR_OK;
        mptar_uint64 record_len = mptar_atou64(&buffer[len_start], i - len_start, &error);
        if (error != MPTAR_OK) {
            reader->memory.free(reader->memory.alloc_user_data, buffer);
            return error;
        } else if (record_len > (mptar_uint64)size_t_pax || record_len == 0) {
            reader->memory.free(reader->memory.alloc_user_data, buffer);
            return MPTAR_ERR_MALFORMED;
        }

        i++; // Advances past ' '

        mptar_size_t key_start = i;
        while (i < size_t_pax && buffer[i] != '=') {
            i++;
        }

        if (i >= size_t_pax) {
            reader->memory.free(reader->memory.alloc_user_data, buffer);
            return MPTAR_ERR_MALFORMED;
        }
        
        mptar_size_t key_len = i - key_start;
        i++; // Advances past '='

        mptar_size_t val_start = i;
        mptar_size_t target_next_record = len_start + (mptar_size_t)record_len;

        if (target_next_record > size_t_pax || target_next_record <= val_start) {
            reader->memory.free(reader->memory.alloc_user_data, buffer);
            return MPTAR_ERR_MALFORMED;
        } else if (buffer[target_next_record - 1] != '\n') {
            reader->memory.free(reader->memory.alloc_user_data, buffer);
            return MPTAR_ERR_MALFORMED;
        }

        mptar_size_t val_len = target_next_record - val_start - 1;
        mptar_apply_pax_kv(reader, &buffer[key_start], key_len, &buffer[val_start], val_len, meta, pax_flags);
        
        i = target_next_record;
    }

    reader->memory.free(reader->memory.alloc_user_data, buffer);
    return MPTAR_OK;
}

static int mptar_parse_ustar_header(const mptar_header* header, mptar_metadata* out_meta) {
    if (!header || !out_meta) return MPTAR_ERR_INVALID_ARG;

    int error = MPTAR_OK;

    out_meta->size = mptar_parse_octal_field(header->size, 12, &error);
    if (error != MPTAR_OK) return error;

    out_meta->typeflag = header->typeflag;

    error = MPTAR_OK;
    out_meta->mode = (mptar_uint32)mptar_parse_octal_field(header->mode, 8, &error);
    if (error != MPTAR_OK) {
        out_meta->mode = (header->typeflag == '5') ? 0755 : 0644;
    }

    error = MPTAR_OK;
    out_meta->uid = mptar_parse_octal_field(header->uid, 8, &error);
    if (error != MPTAR_OK) {
        out_meta->uid = 0;
    }

    error = MPTAR_OK;
    out_meta->gid = mptar_parse_octal_field(header->gid, 8, &error);
    if (error != MPTAR_OK) {
        out_meta->gid = 0;
    }
    
    error = MPTAR_OK;
    out_meta->mtime.value.sec = (mptar_int64)mptar_parse_octal_field(header->mtime, 12, &error);
    out_meta->mtime.value.nsec = 0;
    if (error != MPTAR_OK) {
        out_meta->mtime.value.sec = 0;
    }
    out_meta->mtime.has_value = true;

#ifdef MPTAR_SUPPORT_EXTRA_TIMES
    out_meta->atime.has_value = false;
    out_meta->ctime.has_value = false;
#endif

#ifdef MPTAR_SUPPORT_SPECIAL
    error = MPTAR_OK;
    out_meta->dev_major = (mptar_uint32)mptar_parse_octal_field(header->devmajor, 8, &error);
    if (error != MPTAR_OK) {
        out_meta->dev_major = 0;
    }

    error = MPTAR_OK;
    out_meta->dev_minor = (mptar_uint32)mptar_parse_octal_field(header->devminor, 8, &error);
    if (error != MPTAR_OK) {
        out_meta->dev_minor = 0;
    }
#endif
    
    return MPTAR_OK;
}

int mptar_read_header(mptar_reader *reader, mptar_metadata *out_meta) {
    if (!reader || !out_meta) return MPTAR_ERR_INVALID_ARG;

    out_meta->path = MPTAR_NULL;
    out_meta->link_target = MPTAR_NULL;
    out_meta->uname = MPTAR_NULL;
    out_meta->gname = MPTAR_NULL;

    mptar_uint32 pax_flags = 0;
    char header_bytes[512];
    int res = MPTAR_OK;
    
    while (1) {
        if (reader->read(reader->read_user_data, header_bytes, 512) != 512) {
            res = MPTAR_ERR_IO_READ;
            goto error_cleanup;
        }

        reader->offset += 512;

        mptar_header* header = (mptar_header*)header_bytes;
        res = mptar_verify_header_checksum(header);
        if (res != MPTAR_OK) {
            goto error_cleanup;
        }

        if (header->typeflag == 'x') {
            res = MPTAR_OK;
            mptar_uint64 pax_size = mptar_parse_octal_field(header->size, 12, &res);
            if (res != MPTAR_OK) {
                goto error_cleanup;
            }

            res = mptar_parse_pax_block(reader, pax_size, out_meta, &pax_flags);
            if (res != MPTAR_OK) {
                goto error_cleanup;
            }

            continue;
        }

        if (header->typeflag != '\0' && (header->typeflag < '0' || header->typeflag > '7')) {
            res = MPTAR_OK;
            mptar_uint64 ext_size = mptar_parse_octal_field(header->size, 12, &res);
            
            if (res != MPTAR_OK) {
                // We assume the extension we are trying to skip has no payload (size = 0)
                // We can skip it because we already loaded the 512B of the extension
                // The specification states if size is unsed the extension has no payload.
                ext_size = 0;
                continue;
            }

            mptar_size_t remainder = (mptar_size_t)(ext_size % 512);
            mptar_size_t total_skip = (mptar_size_t)ext_size + ((remainder > 0) ? (512 - remainder) : 0);                        
            if (total_skip > 0) {
                mptar_size_t skipped = mptar_consume_stream(reader, total_skip);        
                if (skipped != total_skip) {
                    res = MPTAR_ERR_IO_READ;
                    goto error_cleanup;
                }
            }

            continue;
        }

        mptar_metadata pax_staged = *out_meta;
        res = mptar_parse_ustar_header(header, out_meta);
        if (res != MPTAR_OK) {
            goto error_cleanup;
        }

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
            res = mptar_reconstruct_ustar_path(reader, header, out_meta);
            if (res != MPTAR_OK) {
                goto error_cleanup;
            }
        }

        if (pax_flags & MPTAR_PAX_HAS_LINK) {
            out_meta->link_target = pax_staged.link_target;
        } else {
            mptar_size_t link_len = mptar_strnlen(header->linkname, 100);
            if (link_len > 0) {
                char* allocated_link = (char*)reader->memory.alloc(reader->memory.alloc_user_data, link_len + 1);
                if (!allocated_link) {
                    res = MPTAR_ERR_ALLOC;
                    goto error_cleanup;
                }
                mptar_memcpy(allocated_link, header->linkname, link_len);
                allocated_link[link_len] = '\0';
                out_meta->link_target = allocated_link;
            }else{
                out_meta->uname = MPTAR_NULL;
            }            
        }

        if (pax_flags & MPTAR_PAX_HAS_UNAME) {
            out_meta->uname = pax_staged.uname;
        } else {
            mptar_size_t uname_len = mptar_strnlen(header->uname, 32);
            if (uname_len > 0) {
                char* allocated_uname = (char*)reader->memory.alloc(reader->memory.alloc_user_data, uname_len + 1);
                if (!allocated_uname) {
                    res = MPTAR_ERR_ALLOC;
                    goto error_cleanup;
                }
                mptar_memcpy(allocated_uname, header->uname, uname_len);
                allocated_uname[uname_len] = '\0';
                out_meta->uname = allocated_uname;
            } else {
                out_meta->uname = MPTAR_NULL;
            }
        }

        if (pax_flags & MPTAR_PAX_HAS_GNAME) {
            out_meta->gname = pax_staged.gname;
        } else {
            mptar_size_t gname_len = mptar_strnlen(header->gname, 32);
            if (gname_len > 0) {
                char* allocated_gname = (char*)reader->memory.alloc(reader->memory.alloc_user_data, gname_len + 1);
                if (!allocated_gname) {
                    res = MPTAR_ERR_ALLOC;
                    goto error_cleanup;
                }
                mptar_memcpy(allocated_gname, header->gname, gname_len);
                allocated_gname[gname_len] = '\0';
                out_meta->gname = allocated_gname;
            } else {
                out_meta->gname = MPTAR_NULL;
            }
        }

        reader->bytes_left = out_meta->size;
        break;
    }

    return MPTAR_OK;

error_cleanup:
    if (out_meta->path) {
        reader->memory.free(reader->memory.alloc_user_data, (void*)out_meta->path);
        out_meta->path = MPTAR_NULL;
    }

    if (out_meta->link_target) {
        reader->memory.free(reader->memory.alloc_user_data, (void*)out_meta->link_target);
        out_meta->link_target = MPTAR_NULL;
    }
    
    if (out_meta->uname) {
        reader->memory.free(reader->memory.alloc_user_data, (void*)out_meta->uname);
        out_meta->uname = MPTAR_NULL;
    }
    
    if (out_meta->gname) {
        reader->memory.free(reader->memory.alloc_user_data, (void*)out_meta->gname);
        out_meta->gname = MPTAR_NULL;
    }

    return res;
}

mptar_size_t mptar_read_data_chunk(mptar_reader* reader, void* buffer, mptar_size_t size, int *out_err) {
    if (out_err) *out_err = MPTAR_OK;

    if (!reader || !buffer) {
        if (out_err) *out_err = MPTAR_ERR_INVALID_ARG;
        return 0;
    }

    int align_err = mptar_align_stream(reader);
    if (align_err != MPTAR_OK) {
        if (out_err) *out_err = align_err;
        return 0;
    }

    if (size == 0 || reader->bytes_left == 0) return 0;

    mptar_size_t bytes_to_read = size;
    if (bytes_to_read > reader->bytes_left) {
        bytes_to_read = reader->bytes_left;
    }

    mptar_size_t bytes_read = reader->read(reader->read_user_data, buffer, bytes_to_read);
    if (bytes_read == 0 || bytes_read > bytes_to_read) {
        if (out_err) *out_err = MPTAR_ERR_IO_READ;
        return 0;
    }

    reader->bytes_left -= bytes_read;
    reader->offset += bytes_read;

    align_err = mptar_align_stream(reader);
    if (align_err != MPTAR_OK && out_err) {
        *out_err = align_err;
    }

    return bytes_read;
}

int mptar_skip_data(mptar_reader* reader) {
    if (!reader) {
        return MPTAR_ERR_INVALID_ARG;
    }

    if (reader->bytes_left > 0) {
        mptar_size_t skipped = mptar_consume_stream(reader, reader->bytes_left);        
        reader->bytes_left -= skipped;
        
        if (skipped < reader->bytes_left + skipped) {
            return MPTAR_ERR_IO_READ; 
        }
    }

    int align_err = mptar_align_stream(reader);
    if (align_err != MPTAR_OK) {
        return align_err;
    }

    return MPTAR_OK;
}

#endif /* MPTAR_WITHOUT_READ */