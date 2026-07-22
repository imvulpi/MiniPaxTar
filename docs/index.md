# minipaxtar

`minipaxtar` is a zero-dependency, single-header C library designed for parsing and creating **TAR archives** with support for standard **USTAR** and **PAX extended headers** (high-precision timestamps and long path names).

---

## Key Features

* **Single-Header Integration**: Just drop `minipaxtar.h` into your C/C++ project.
* **Stream & Memory Friendly**: Read or write directly using customizable I/O callbacks without allocating huge in-memory buffers.
* **PAX Support**: Handles path lengths exceeding standard USTAR limits (100–256 chars) and sub-second nanosecond timestamps.
* **Zero Dependencies**: Pure standard C99 with no external third-party library dependencies.
* **Embedded & Footprint Friendly**: Disable reading (`MPTAR_NO_READER`) or writing (`MPTAR_NO_WRITER`) via preprocessor flags to strip unused code from your binary.

---

## Quick Example

Here is a brief demonstration showing how to initialize the reader context, read headers, and stream entry payload data in chunks:

```c
#define MINIPAXTAR_IMPLEMENTATION
#include "minipaxtar.h"
#include <stdio.h>
#include <stdlib.h>

mptar_size_t stdlib_tar_read(void* context, void* buffer, mptar_size_t size) {
    FILE* f = (FILE*)context;
    return (mptar_size_t)fread(buffer, 1, size, f);
}

void* stdlib_tar_alloc(void* context, mptar_size_t size) {
    return malloc(size);
}

void stdlib_tar_free(void* context, void* ptr) {
    free(ptr);
}
```

```c
FILE *file_in = fopen("archive.tar", "rb");
if (!file_in) return 1;

mptar_reader reader_ctx = {
    .read = stdlib_tar_read,
    .read_user_data = file_in,
    .memory = {
        .alloc = stdlib_tar_alloc,
        .free = stdlib_tar_free,
        .alloc_user_data = NULL
    },
    .offset = 0,
    .bytes_left = 0
};

mptar_metadata read_meta;
while (mptar_read_header(&reader_ctx, &read_meta) == 0) {
    char chunk[512];
    while (reader_ctx.bytes_left > 0) {
        int error = MPTAR_OK;
        mptar_size_t read_bytes = mptar_read_data_chunk(
            &reader_ctx, chunk, reader_ctx.bytes_left, 
            &error);

        if (error != MPTAR_OK) {
            break;
        }

        /* Process payload chunk here... */
        /* E.g. Extract the data to files. */
    }
}

fclose(file_in);
return 0;
```