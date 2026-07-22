# PAX Extended Format vs. Legacy USTAR

`minipaxtar` provides full support for both standard **USTAR (IEEE Std 1003.1-1988)** and **PAX Extended Header (POSIX.1-2001 / IEEE Std 1003.1-2001)** formats.

This guide explains how `minipaxtar` transparently negotiates these standards, handles field physical limits, and performs automatic PAX promotion.

---

## Comparison at a Glance

Traditional USTAR uses fixed-width null-terminated string fields in a single 512-byte header block. PAX removes these physical constraints by prepending key-value attribute blocks formatted in UTF-8 before the file's primary header.

| Feature | Legacy USTAR | PAX Extended (`minipaxtar`) |
| :--- | :--- | :--- |
| **Path Length** | Max 100 chars (or 256 via `prefix`) | Unlimited (UTF-8 string) |
| **Link Target Length** | Max 100 chars | Unlimited (UTF-8 string) |
| **Maximum File Size** | 8 GiB (`8,589,934,591` bytes) | Unlimited (up to $2^{64}-1$ in 64-bit systems) |
| **Sub-second Timestamps** | Not supported (1-second resolution) | Nanosecond precision (`atime`, `mtime`, `ctime`) |
| **UID / GID Limits** | 7 octal digits ($2,097,151$) | Unlimited numeric values |
| **Vendor Extensions** | Vendor-specific raw fields | Custom key-value pairs (`KEY=VALUE\n`) |

---

## How `minipaxtar` Operates

### 1. Automatic PAX Promotion (Writing)

When writing an entry via `mptar_write_header()`, `minipaxtar` inspects the populated `mptar_metadata` structure to determine whether standard USTAR can accommodate the record:

1. **Fits in USTAR:** The writer formats a standard 512-byte USTAR header block (type flag `'0'`, `'5'`, etc.).
2. **Exceeds USTAR Limits:** The writer automatically creates a **PAX Extended Header block** (type flag `'x'`) containing the extended key-value attributes immediately preceding the main USTAR header.

> **Note:** When PAX promotion occurs, `minipaxtar` automatically writes fallback/truncated values into the USTAR header for backward compatibility with legacy TAR extractors that do not support PAX.

#### Conditions Triggering PAX Extended Headers

* `path` exceeds 100 characters and cannot be split across `prefix`/`name`.
* `linkpath` exceeds 100 characters.
* `size` exceeds 8,589,934,591 bytes (8 GiB) [1].
* `uid` or `gid` exceeds 2,097,151 [1].
* `uname` or `gname` string length exceeds 32 bytes.
* `mtime` timestamp exceeds 8,589,934,591 seconds [1].
* Negative timestamps are present (dates prior to January 1, 1970).
* Sub-second timestamp resolution or extended timestamps (`atime`, `ctime`) are present (when `MPTAR_SUPPORT_EXTRA_TIMES` is enabled).

> **[1] Binary Octal Encoding Exception:**
> Standard USTAR octal fields can also store numbers using binary encoding, expanding their capacity to full signed 64-bit integers. By default, if a value fits inside a binary-encoded field, `minipaxtar` skips generating a PAX header. However, if the writer flag `MPTAR_CTX_ALLOW_PAX_FOR_OCTAL` is set, `minipaxtar` will emit both the PAX extended header and the binary-encoded USTAR field.
---

### 2. Transparent Header Parsing (Reading)

When reading archives with `mptar_read_header()`, `minipaxtar` handles format differences transparently:

```

[PAX Header 'x']  ---> Updates internal reader state with extended attributes
│
▼
[USTAR Header '0'] ---> Overlays baseline metadata; returns unified mptar_metadata
```

1. If a PAX header (`typeflag == 'x'`) is encountered, `minipaxtar` parses its key-value pairs (`path`, `size`, `mtime`, `atime`, etc.).
2. When the following file header arrives, the baseline USTAR fields are read, and any present PAX attributes override the standard USTAR values.
3. The resulting populated `mptar_metadata` struct presented to your application contains the complete, high-precision metadata regardless of which format variant created it.

---

## PAX Key-Value Pair Format

PAX extended headers store records in UTF-8 text lines structured as:

> [size][' ' (space)][keyword][=][value][\n]

For example, a file with a 250-character path (that couldn't be split between `prefix` and `name`) generates a PAX block structured like this:

```text
260 path=very/......./big/path
22 mtime=1700000000.5
```

`minipaxtar` handles the length calculation, formatting, and boundary parsing internally without requiring manual string manipulation or memory allocation from the caller.

---

## Preprocessor Options & Size Impact

If your platform has strict memory or binary size constraints and does not require extended metadata attributes, you can strip specific PAX features:

* **`MPTAR_SUPPORT_EXTRA_TIMES`**: Disables `atime` and `ctime` PAX parsing/formatting.
* **`MPTAR_SUPPORT_SPECIAL`**: Disables PAX device number (`devmajor`/`devminor`) extended handling.

*(See [Configuration & Compile-Time Flags](config-defines.md) for full definition details.)*

