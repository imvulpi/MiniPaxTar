# Extracting Archives

This guide demonstrates how to unpack files and directories from a TAR archive onto the local filesystem using `minipaxtar`.

When extracting an archive, inspect `meta.typeflag` to distinguish between directories (`'5'`) and regular files (`'0'` or `'\0'`).
`mptar_read_data_chunk()` automatically handles internal byte-clamping and stream alignment - so you can pass your full buffer size directly to the function!

## Example

This example below is fully copyable! Just copy it and test it - make sure you actually have the `archive.tar` file.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "minipaxtar.h"

static mptar_size_t stdlib_tar_read(void *u, void *b, mptar_size_t s)
                    { return (mptar_size_t)fread(b, 1, s, (FILE*)u); }
static void* stdlib_tar_alloc(void *u, mptar_size_t s) { (void)u; return malloc(s); }
static void stdlib_tar_free(void *u, void *p) { (void)u; free(p); }

/* Platform-specific directory creation helper */
static void create_dir_if_not_exists(const char *path) {
#ifdef _WIN32
    _mkdir(path);
#else
    mkdir(path, 0755);
#endif
}

void extract_archive_example(const char *tar_filepath) {
    FILE *file_in = fopen(tar_filepath, "rb");
    if (!file_in) return;

    mptar_reader reader = {
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

    mptar_metadata meta;
    int status;

    while ((status = mptar_read_header(&reader, &meta)) == MPTAR_OK) {

        /* 1. Directory Entry */
        if (meta.typeflag == '5') {
            printf("Creating directory: %s\n", meta.path);
            create_dir_if_not_exists(meta.path);
            continue;
        }

        /* 2. Regular File Entry */
        if (meta.typeflag == '0' || meta.typeflag == '\0') {
            printf("Extracting file: %s (%llu bytes)\n", meta.path, (unsigned long long)meta.size);

            FILE *out_file = fopen(meta.path, "wb");
            
            /* If opening destination file fails, discard that entry's payload & align the stream */
            if (!out_file) {
                fprintf(stderr, "Failed to open output file: %s\n", meta.path);
                if(mptar_discard_data(&reader) != MPTAR_OK) break;
                continue;
            }

            /* Read chunk from tar reader and immediately stream to out_file */
            char buffer[4096];
            int err = MPTAR_OK;
            mptar_size_t bytes_read;

            /* No clamping needed! mptar_read_data_chunk internally limits reads 
               to reader.bytes_left and handles block alignment */
            while ((bytes_read = mptar_read_data_chunk(&reader, buffer, sizeof(buffer), &err)) > 0) {
                if (err != MPTAR_OK) {
                    fprintf(stderr, "Error reading chunk for %s (code %d)\n", meta.path, err);
                    break;
                }
                fwrite(buffer, 1, bytes_read, out_file);
            }

            fclose(out_file);
        }
    }

    fclose(file_in);
}

int main(int argc, char const *argv[])
{
    extract_archive_example("archive.tar");
    return 0;
}
```

---

## Clean Extraction Tips

### 1. Direct Buffer Slicing

You never need to manually clamp your read buffer size against `reader.bytes_left`:

```c
/* Clean - just pass sizeof(buffer) directly */
mptar_read_data_chunk(&reader, buffer, sizeof(buffer), &err);
```

### 2. Stream Alignment & Skipped Files

If an error prevents you from opening or writing to a destination file on disk (such as missing parent directories or permission errors), you **must still consume or advance past the payload bytes** before reading the next header.

Calling `mptar_discard_data(&reader)` handles this automatically: it streams and discards any remaining unread bytes for the current entry and aligns the reader context to the next 512-byte block.

```c
if (!out_file) {
    fprintf(stderr, "Failed to open output file: %s\n", meta.path);
    
    /* Drains remaining entry bytes and aligns stream to next 512-byte block */
    if(mptar_discard_data(&reader) != MPTAR_OK) break;
    continue;
}
```
/\ It's important that you handle the `mptar_discard_data()` errors too, because of the fact that it uses reads to advance the stream it might fail.

## Stream Navigation:  `discard` vs `skip`

`minipaxtar` provides two distinct ways to handle unread payloads depending on your I/O backend:

| Function | Method | Use Case |
| --- | --- | --- |
| **`mptar_discard_data()`** | **Reads stream forward** | Best for standard sequential streams (`FILE*`, sockets). Reads and discards remaining bytes through your `.read` callback and aligns the stream to the next 512-byte block boundary. |
| **`mptar_skip_data()`** | **Adjusts offsets without I/O** | Best for memory-mapped buffers or seekable streams. Resets `bytes_left` to `0` and aligns internal offsets instantly without calling `.read`. |

> **Note:** Because `mptar_skip_data()` modifies `offset` without issuing actual read calls, do not use it on non-seekable streams unless you manually moved the stream forward.

Using mptar_skip_data() is useful for [Bypassing chunk reading](reading.md#advanced-bypassing-chunk-reading)