# Reading Archives

This guide explains how to iterate through TAR headers, inspect entry metadata, and stream payload data in chunks using `minipaxtar`.
In this document I will use standard library's file api, but this could be modified to be used with any other streams.

To read an archive:

1. Initialize the `mptar_reader` context with your read callback, stream handle (`file_in`), and memory allocators.
2. Call `mptar_read_header(&reader, &meta)` in a loop until it returns an status other than `MPTAR_OK` (e.g., EOF or error).
3. Read the entry's payload in chunks using `mptar_read_data_chunk()`. `minipaxtar` updates `reader.bytes_left` automatically and aligns the stream block structure of the reader struct.


## Example

This example below is fully copyable! Just copy it and test it - make sure you actually have the `archive.tar` file.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "minipaxtar.h"

static mptar_size_t stdlib_tar_read(void *u, void *b, mptar_size_t s) 
                    { return (mptar_size_t)fread(b, 1, s, (FILE*)u); }
static void* stdlib_tar_alloc(void *u, mptar_size_t s) { (void)u; return malloc(s); }
static void stdlib_tar_free(void *u, void *p) { (void)u; free(p); }

void read_archive_example(const char *filepath) {
    FILE *file_in = fopen(filepath, "rb");
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

    /* Loop until EOF or error */
    while ((status = mptar_read_header(&reader, &meta)) == MPTAR_OK) {
        printf("File: %s | Size: %llu bytes | Mode: 0%o\n", 
               meta.path, 
               (unsigned long long)meta.size, 
               meta.mode);

        char buffer[1024];

        int err = MPTAR_OK;
        while (mptar_read_data_chunk(&reader, buffer, sizeof(buffer), &err) != 0) {
            if (err != MPTAR_OK) {
                fprintf(stderr, "Error reading payload chunk: %d\n", err);
                break;
            }

            /* Process chunk data here... */
        }
    }

    fclose(file_in);
}


int main(int argc, char const *argv[])
{
    read_archive_example("archive.tar");
    return 0;
}
```

## Overview

The example will print some basic metadata from entries found in the `archive.tar`. It won't extract it, but you could modify this code to process the data in it in different ways. For example to extract it :). You can see the [extraction example here](extracting.md)

> Of course other metadata is present as well, you can try modifying the code to print those as well~!

## No need for clamping

In some other libraries, you'd need to clamp the value of bytes you want to read to the bytes left in the reader. 
Worry not! This is actually done in the function internally, so just pass the `sizeof(buffer)` or the number of bytes you want to read.

```c
/* YOU DONT NEED CLAMPS! THIS IS UNNECESARY */
mptar_size_t to_read = (reader.bytes_left < sizeof(buffer)) 
                        ? reader.bytes_left 
                        : sizeof(buffer);
```
/\ Example of unnecesary clamping

## No need for bytes left checks

While you certainly can check how many bytes are left to be read, you actually don't need to do it in `minipaxtar`

See how some people would do this:
```c
while (reader.bytes_left > 0) {
    int err = MPTAR_OK;
    mptar_size_t read_bytes = mptar_read_data_chunk(&reader, buffer, sizeof(buffer), &err);
    if (err != MPTAR_OK) {
        fprintf(stderr, "Error reading payload chunk: %d\n", err);
        break;
    }
}
```

It's a valid way to read the data, however you could replace it with this:
```c
    int err = MPTAR_OK;
    while (mptar_read_data_chunk(&reader, buffer, sizeof(buffer), &err) != 0) {
        if (err != MPTAR_OK) {
            fprintf(stderr, "Error reading payload chunk: %d\n", err);
            break;
        }
    }
```
> Use whichever version you prefer.

Sometimes using `reader->bytes_left` is neccesary, for example when you want to do a custom read like with mapping the memory from the system. You would read it like that and then skip data by using `mptar_skip_data`. But it's important to note that `mptar_skip_data` modifies the bytes_left and offset, so if you try to skip the data and then read manually based on the `reader->bytes_left` you will fail since the bytes left after the skip is 0.

## Advanced: Bypassing chunk reading

You can use `mptar_skip_data()` to bypass `minipaxtar`'s chunk reading entirely if you plan to read the payload directly via zero-copy memory mapping or custom buffer offsets.
> **Important:** `mptar_skip_data()` resets `reader->bytes_left` to `0` immediately upon execution. If you map or slice payload memory manually, either **cache `reader->bytes_left` beforehand** or perform your mapping operation **before** calling `mptar_skip_data()`.

```c
/* Option A: Cache payload size before skipping */
mptar_size_t payload_len = reader.bytes_left;
mptar_skip_data(&reader);
/* Read/Map the stream manually here and use the cached values. */

/* Option B: Process first, skip after */
/* Read/Map the stream manually here and use reader->bytes_left directly */
mptar_skip_data(&reader);
```