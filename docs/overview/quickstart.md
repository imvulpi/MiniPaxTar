# Quickstart Guide

This guide introduces the core concepts, memory architecture, and stream-handling model of `minipaxtar`.

---

## The Mental Model

`minipaxtar` is completely decoupled from standard library I/O and memory management. Rather than reading from files directly, it delegates all I/O and allocation to callbacks that you provide. It also allows you to perform custom high performance reading of data if you skip it with `mptar_skip_data` - it simply adjusts offsets and internal values so that after you read the data, you can instantly go back to reading headers. 


### 1. I/O & Memory Callbacks
All operations rely on basic function signatures for I/O and allocation:

```c
typedef void* (*mptar_alloc_fn)(void* user_data, mptar_size_t size);
typedef void  (*mptar_free_fn)(void* user_data, void* ptr);
typedef mptar_size_t (*mptar_read_fn)(void* user_data, void* buffer, mptar_size_t size);
typedef mptar_size_t (*mptar_write_fn)(void* user_data, const void* buffer, mptar_size_t size);
```

> These can be easily wrapped around files, memory and network stream reading/writing apis. 

### 2. Contexts: Reader & Writer

State is maintained inside explicit context structures:

* **`mptar_reader`**: Holds the `.read` callback, user pointer, memory allocators, and stream position tracking (`bytes_left`).
* **`mptar_writer`**: Holds the `.write` callback, user pointer, and memory allocators.

> Both of the writer and reader share a `mptar_memory_cfg` which means you can easily pass the same structure to both of them.

### 3. Metadata Header (`mptar_metadata`)

Describes an archive entry (path, file size, permissions, timestamps, typeflag and more). 
It's the minipaxtar's universal struct for you to process or pass to the functions. 
Use it to process entries in different ways or set values inside it to write entries.

> Strings are null-terminated and don't have any limits. <br>
> Timestamps support nanoseconds and negative numbers. <br>
> Number values get 64-bit of space support.

## Standard C File Setup

To use standard C `FILE*` streams, define these lightweight wrapper callbacks once in your project:

```c
static mptar_size_t stdlib_tar_read(void *user_data, void *buffer, mptar_size_t size) {
    return (mptar_size_t)fread(buffer, 1, size, (FILE*)user_data);
}

static mptar_size_t stdlib_tar_write(void *user_data, const void *buffer, mptar_size_t size) {
    return (mptar_size_t)fwrite(buffer, 1, size, (FILE*)user_data);
}

static void* stdlib_tar_alloc(void *user_data, mptar_size_t size) {
    (void)user_data; return malloc(size);
}

static void stdlib_tar_free(void *user_data, void *ptr) {
    (void)user_data; free(ptr);
}
```

## Next Steps

Now that you understand the context structs and callbacks, jump directly into the code guide you need:

* **[Reading Archives](reading.md)**: Iterate headers, inspect metadata, and stream payload chunks.
* **[Writing Archives](writing.md)**: Build new TAR archives from scratch and write PAX headers.
* **[Extracting Files](extracting.md)**: Unpack archives to disk while maintaining directory structures.
