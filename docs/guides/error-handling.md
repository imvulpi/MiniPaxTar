# Error Handling & Stream Recovery

This guide covers recommended patterns for handling errors when reading or writing archives with `minipaxtar`, including stream recovery and payload validation.

You can check the codes in the [Status/Error Codes section](status-codes.md)

---

## Core Handling Patterns

### 1. Reader Loop Pattern

When parsing an archive header-by-header, check for `MPTAR_EOF` to gracefully terminate processing, and break on any error code `< 0`:

```c
mptar_metadata meta;
int status;

while ((status = mptar_read_header(&reader, &meta)) == MPTAR_OK) {
    printf("Entry: %s (%llu bytes)\n", meta.path, (unsigned long long)meta.size);
    
    /* Consume or skip payload bytes... */
    mptar_discard_data(&reader);
}

if (status == MPTAR_EOF) {
    /* Standard end of archive - successful read */
} else {
    fprintf(stderr, "Archive read failed with error code: %d\n", status);
}

```

---

### 2. Stream Recovery vs. Unrecoverable Failures

Not all errors require aborting archive processing:

* **Recoverable File Errors (e.g., local disk write failure):** Call `mptar_discard_data(&reader)` to drain the remainder of the active file payload and re-align the reader context to the next 512-byte header boundary.
* **Fatal Archive Errors (e.g., `MPTAR_ERR_CHECKSUM`, `MPTAR_ERR_IO_READ`):** Stream alignment or integrity is broken. Stop parsing immediately.

```c
FILE *out = fopen(meta.path, "wb");
if (!out) {
    fprintf(stderr, "Cannot write %s on disk; skipping entry.\n", meta.path);
    
    /* RECOVERY: Advance reader past unread payload to keep stream aligned */
    mptar_discard_data(&reader);
    continue;
}

```

---

### 3. Write Finalization Safeguards

When writing payload data with `mptar_write_data_chunk()`, `minipaxtar` automatically clamps the write size to `ctx->bytes_left` so you never overflow the declared file size.

Because `mptar_write_data_chunk()` returns an unsigned byte count (`mptar_size_t`), status and error codes are passed back via an error pointer (`int *err`):

```c
/* Writing chunks: */
int err = MPTAR_OK;
mptar_size_t written = mptar_write_data_chunk(&writer, data, len, &err);

if (err != MPTAR_OK) {
    fprintf(stderr, "I/O write error encountered: %d\n", err);
} else if (written < len) {
    printf("Wrote %llu bytes (payload target reached; extra data clamped).\n", 
           (unsigned long long)written);
}

/* Finalizing: */
int final_status = mptar_write_finalize(&writer);
if (final_status == MPTAR_ERR_INCOMPLETE_PAYLOAD) {
    fprintf(stderr, "Error: Finalized archive while a file payload was still incomplete!\n");
} else if (final_status != MPTAR_OK) {
    fprintf(stderr, "Archive finalization failed with error code: %d\n", final_status);
}
```