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
| **`MPTAR_NO_EXTRA_TIMES`** | *Undefined* / ON in CMake | Removes support for extended PAX timestamp attributes (`atime` access time and `ctime` change time). Reduces binary footprint where such support is unneded. |

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

By default, `minipaxtar` doesn't define any compilation options, however it enables support on cmake for special device nodes. If your system does not require these metadata fields, you can omit them to simplify `mptar_metadata` structs and handling logic. By default extra extended access time (`meta.atime`) and change time (`meta.ctime`) are included to remove it use `MPTAR_NO_EXTRA_TIMES`. It is included by default because the size difference in negligible for most users and yet it provides useful features. Special files support is not included by default because most users don't use it, you may enable the support by defining `MPTAR_SUPPORT_SPECIAL`

* **`MPTAR_SUPPORT_SPECIAL`**: Includes device node fields (`meta.devmajor` and `meta.devminor`).
* **`MPTAR_NO_EXTRA_TIMES`**: Removes extended access time (`meta.atime`) and change time (`meta.ctime`).

```c
#ifdef MPTAR_SUPPORT_SPECIAL
    printf("Device major: %u, minor: %u\n", meta.devmajor, meta.devminor);
#endif

#ifndef MPTAR_NO_EXTRA_TIMES // Or assume it is turned on, like it is by default.
    printf("Access time: %llu, Change time: %llu\n", meta.atime, meta.ctime);
#endif
```

---

## Stable Function Overrides

`minipaxtar` allows selected internal and standard-library-compatible functions to be overridden at compile time. This includes string operations such as `strlen` and `strnlen`, memory operations such as `memcpy`, and other supported functions. See all functions that can be safely overriden in [stable config options](/MiniPaxTar/api/Modules/group__mptar__stable__config/)

To use a custom implementation, define the corresponding function-name macro to your implementation's name. This allows you to provide optimized, platform-specific, or assembly-accelerated implementations while keeping the rest of `minipaxtar` unchanged.

For example, to override `strlen`, define `mptar_strlen` as `custom_strlen`. The built-in implementation will be omitted, and `minipaxtar` will use your custom function instead.

### Example: Overriding `strlen`

```c
#include <stdio.h>
#include <string.h>
#include "minipaxtar.h"

/* Custom implementation matching the expected signature */
size_t custom_strlen(const char* str) {
    printf("Custom strlen called\n");

    const char* p = str;
    while (*p) {
        ++p;
    }

    return (size_t)(p - str);
}
```

Compile with the override definition:

```sh
clang minipaxtar.c ... -Dmptar_strlen=custom_strlen
```

When `minipaxtar` calls `mptar_strlen`, the call resolves to `custom_strlen`, which prints a message before calculating the string length.

Your custom implementation must be available at link time and match the expected function signature. It must not be `static` if it is defined in a separate translation unit and referenced externally by `minipaxtar`.

### Legacy Overrides

The legacy `MPTAR_CUSTOM_*` macros are still supported for backward compatibility but are deprecated. Unlike the newer function-name overrides, they do not allow you to specify a custom function name. Depending on the override, this can retain built-in symbols or increase the resulting binary size.

### CMake Integration Example

Use `target_compile_definitions()` to configure overrides for a specific target:

```cmake
target_compile_definitions(my_app PRIVATE
    mptar_strlen=custom_strlen
)
```

Ensure that `custom_strlen` is included in the target and linked into the final executable.

## Experimental and Internal Overrides

In addition to the stable overrides documented here, `minipaxtar` provides experimental and internal overrides for low-level customization, including internal functions.

These overrides carry **no stability guarantees** and may change or be removed without notice. Use them at your own risk.

See the [Experimental & Internal Overrides](/MiniPaxTar/api/Modules/group__mptar__experimental__config/) documentation for the complete list.

Only overrides explicitly documented under [Stable Compile-Time Options](/MiniPaxTar/api/Modules/group__mptar__stable__config/) are covered by the project's stability guarantees.