# Configuration & Compile-Time Flags

`minipaxtar` is designed to be lean and adaptable for embedded environments, bare-metal kernels, and resource-constrained platforms. You can tailor its feature set, binary footprint, and dependencies using preprocessor definitions.

---

## High-Level Feature Switches

These macros control major API subsets and platform capability dependencies.

| Preprocessor Flag | Default | Description |
| :--- | :--- | :--- |
| **`MPTAR_NO_READ`** | *Undefined* | Disables the reading and parsing API (`mptar_reader`, `mptar_read_header`, `mptar_read_data_chunk`, etc.). Reduces binary footprint when only writing archives. |
| **`MPTAR_NO_WRITE`** | *Undefined* | Disables the archive creation API (`mptar_writer`, `mptar_write_header`, `mptar_write_data_chunk`, etc.). Reduces binary footprint when only extracting archives. |
| **`MPTAR_SUPPORT_SPECIAL`** | *Undefined* / ON in CMake | Include support for special node metadata (`devmajor` and `devminor` device numbers). |
| **`MPTAR_SUPPORT_EXTRA_TIMES`** | *Undefined* / ON in CMake | Include support for extended PAX timestamp attributes (`atime` access time and `ctime` change time). |

---

## Detailed Switch Usage

### 1. Stripping Read or Write API

If your application only needs to unpack archives or only needs to create them, you can strip the unused logic entirely:

```c
#define MPTAR_NO_READ
#include "minipaxtar.h"
```

```bash
gcc -DMPTAR_NO_WRITE -c minipaxtar.c -o minipaxtar.o
```

---

### 2. Extended Metadata Features

By default, `minipaxtar` doesn't define any compilation options, however it enables support on cmake for special device nodes and extended PAX timestamps. If your system does not require these metadata fields, you can omit them to simplify `mptar_metadata` structs and handling logic.

* **`MPTAR_SUPPORT_SPECIAL`**: Includes device node fields (`meta.devmajor` and `meta.devminor`).
* **`MPTAR_SUPPORT_EXTRA_TIMES`**: Includes extended access time (`meta.atime`) and change time (`meta.ctime`).

```c
#ifdef MPTAR_SUPPORT_SPECIAL
    printf("Device major: %u, minor: %u\n", meta.devmajor, meta.devminor);
#endif

#ifdef MPTAR_SUPPORT_EXTRA_TIMES
    printf("Access time: %llu, Change time: %llu\n", meta.atime, meta.ctime);
#endif
```

---

## Custom Number Conversion Hooks

To prevent dependencies on Standard C library functions like `snprintf` or `strtoull`, `minipaxtar` ships with internal fallback functions for integer-to-string and string-to-integer conversions.

If your environment provides optimized, custom, or assembly-accelerated equivalents, define these macros to **omit built-in implementations** and delegate to external routines of the same name:

| Flag | Omitted <br> Built-in | Required `extern` Function Signature |
| --- | --- | --- |
| **`MPTAR_CUSTOM_U64TOA`** | `mptar_u64toa` | `char* mptar_u64toa(mptar_uint64 value, char* str, mptar_size_t str_size, int *out_err);` |
| **`MPTAR_CUSTOM_I64TOA`** | `mptar_i64toa` | `char* mptar_i64toa(mptar_int64 value, char* buf, mptar_size_t buf_size, int *out_err);` |
| **`MPTAR_CUSTOM_ATOU64`** | `mptar_atou64` | `mptar_uint64 mptar_atou64(const char* str, mptar_size_t len, int* err);` |
| **`MPTAR_CUSTOM_ATOI64`** | `mptar_atoi64` | `mptar_int64 mptar_atoi64(const char* str, mptar_size_t len, int* err);` |

---

### Example: Supplying Custom Conversion Hooks

When defining any of the custom conversion flags, ensure your custom implementations are declared or linked in your binary:

```c
/* Enable custom unsigned 64-bit conversion hook */
#define MPTAR_CUSTOM_ATOU64
#include "minipaxtar.h"

/* Supply the external function matching the exact name and signature */
uint64_t mptar_atou64(const char* str, mptar_size_t len, int* err) {
    /* Custom optimized conversion logic */
    uint64_t result = 0;
    /* ... */
    if (err) *err = MPTAR_OK; /* or 0 */
    return result;
}

```
---

## CMake Integration Example

If building via CMake, you can toggle these definitions cleanly using `target_compile_definitions()` or CMake `option()`s:

```cmake
# Disable write support and use custom 64-bit parser
target_compile_definitions(my_app PRIVATE
    MPTAR_NO_WRITE
    MPTAR_CUSTOM_ATOU64
)
```
