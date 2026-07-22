# Writing Archives

This guide explains how to construct TAR archives, configure file metadata, write data payloads, and finalize standard-compliant archives using `minipaxtar`.

## Creating an Archive

Creating a TAR archive involves a structured four-step lifecycle for every file entry:

1. **Initialize Writer**: Pass your write callback, stream handle, and memory allocators to `mptar_writer`.
2. **Write Header**: Populate `mptar_metadata` with your data and use `mptar_write_header()` to write it to the stream. You don't need to worry about the path sizes, `minipaxtar` will automatically use extensions if the path doesn't fit.
3. **Stream Payload**: Write data blocks using `mptar_write_data_chunk()`.
4. **Finalize & Close**: Call `mptar_write_finalize()` to pad the current file entry to a 512-byte boundary, then call `mptar_close_archive()` to append the trailing EOF blocks.
> And finally use `mptar_close_archive(&writer)` after all entries are written to close the archive.

## Example

This example below is fully copyable! Just copy it and test it.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minipaxtar.h"

static mptar_size_t stdlib_tar_write(void *u, const void *b, mptar_size_t s) 
                    { return (mptar_size_t)fwrite(b, 1, s, (FILE*)u); }
static void* stdlib_tar_alloc(void *u, mptar_size_t s) { (void)u; return malloc(s); }
static void stdlib_tar_free(void *u, void *p) { (void)u; free(p); }

void create_archive_example(const char *output_path) {
    FILE *file_out = fopen(output_path, "wb");
    if (!file_out) return;

    mptar_writer writer = {
        .write = stdlib_tar_write,
        .write_user_data = file_out,
        .memory = {
            .alloc = stdlib_tar_alloc,
            .free = stdlib_tar_free,
            .alloc_user_data = NULL
        }
    };

    /* 1. Define file payload and metadata */
    const char *payload = "Hello, minipaxtar!";
    mptar_size_t payload_len = (mptar_size_t)strlen(payload);

    mptar_metadata meta = {0};
    meta.path = "hello.txt";
    meta.size = payload_len;
    meta.mode = 0644;
    meta.typeflag = '0'; /* Regular file */

    /* Optional timestamp setup */
    meta.mtime.has_value = true;
    meta.mtime.value.sec = 1700000000;
    meta.mtime.value.nsec = 0;

    /* 2. Write Header */
    int status = mptar_write_header(&writer, &meta);
    if (status != MPTAR_OK) {
        fprintf(stderr, "Failed to write header: %d\n", status);
        fclose(file_out);
        return;
    }

    /* 3. Write Data Chunk */
    int err = MPTAR_OK;
    mptar_write_data_chunk(&writer, payload, payload_len, &err);

    /* 4. Finalize Stream Entry (Pads entry payload to 512-byte alignment) */
    mptar_write_finalize(&writer, &meta);

    /* 5 (final). Close Archive (Writes two 512-byte blocks of null bytes at EOF) */
    mptar_close_archive(&writer);

    fclose(file_out);
}

int main(int argc, char const *argv[])
{
    create_archive_example("new_archive.tar");
    return 0;
}
```

## Writing Mechanics & Lifecycle

| Stage | Function | Description |
| --- | --- | --- |
| **Header Write** | `mptar_write_header()` | Converts metadata fields into a standard USTAR block (or generates a PAX extended header block if paths/timestamps exceed standard formats). |
| **Data Chunks** | `mptar_write_data_chunk()` | Streams raw file content to your write callback. Can be called repeatedly for large files. |
| **Entry Alignment** | `mptar_write_finalize()` | Calculates remaining unaligned bytes for the current file and writes null padding to round the entry out to a multiple of 512 bytes. |
| **Archive Termination** | `mptar_close_archive()` | Appends two 512-byte blocks of zero bytes (`0x00`) to signal end-of-archive (EOA) per POSIX TAR specifications. |

> **Important:** Always call `mptar_close_archive(&writer)` before closing your underlying file handle to ensure that the generated archive conforms to standard TAR tools like `tar` or `7z`.

## Write chunks in loops

Often payloads need to be written in chunks, especially if you're constained in memory. Because `mptar_write_data_chunk()` can be used to write smaller buffers of data, what you can do is implement a loop which: 

1. Reads data from your source into a buffer,
2. Pass the buffer to the `mptar_write_data_chunk()` to write it,
3. Check for errors and continue until `mptar_write_data_chunk` returns 0 with no errors.

> You should not ignore errors from `mptar_write_data_chunk()`. It will return 0 in case of an error because the returning type of it is unsigned (`mptar_size_t`).

Check how you can [read archives](reading.md) too