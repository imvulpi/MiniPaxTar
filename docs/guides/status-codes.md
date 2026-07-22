# Status & Error Codes Reference

`minipaxtar` uses integer return codes to signal operation success, archive boundaries, or specific structural and I/O failures. 

Functions return `0` or positive status codes for expected operational states, and negative values for error conditions.

---

## Status & Error Reference

### Success & Informational Codes (`>= 0`)

| Code | Macro | Description |
| :---: | :--- | :--- |
| `0` | **`MPTAR_OK`** | Operation completed successfully. |
| `1` | **`MPTAR_EOF`** | End of archive reached (encountered the standard sequence of two consecutive zero-filled 512-byte blocks). |
| `2` | **`MPTAR_NEEDS_PAX`** | Item metadata cannot fit within standard legacy USTAR constraints and requires a PAX extended header. |

---

### Failure & Error Codes (`< 0`)

| Code | Macro | Description |
| :---: | :--- | :--- |
| `-1` | **`MPTAR_ERR_INVALID_ARG`** | `NULL` context pointers, invalid parameters, or zero-length arguments passed to an API function. |
| `-2` | **`MPTAR_ERR_ALLOC`** | Memory allocation callback returned `NULL` or an invalid pointer when attempting to allocate dynamic buffers. |
| `-3` | **`MPTAR_ERR_IO_READ`** | User `.read` callback failed or returned fewer bytes than requested (unexpected end of stream). |
| `-4` | **`MPTAR_ERR_IO_WRITE`** | User `.write` callback failed to write the requested chunk to underlying storage or wrote too little. |
| `-5` | **`MPTAR_ERR_CHECKSUM`** | Header block checksum verification failed; the TAR record is corrupted or misaligned. |
| `-6` | **`MPTAR_ERR_UNSUPPORTED_TYPE`** | Encountered a special file node type (e.g., FIFO, Block, Char) which handling of was disabled. |
| `-7` | **`MPTAR_ERR_RESERVED_LEGACY_1`** | Reserved / Legacy. Formerly used for write overflow prior to automatic payload clamping. |
| `-8` | **`MPTAR_ERR_INCOMPLETE_PAYLOAD`** | Called `mptar_write_finalize()` or attempted to start a new header while `bytes_left > 0` on the current file payload. |
| `-9` | **`MPTAR_ERR_MALFORMED`** | Archive structural corruption or malformed header formatting detected (e.g., invalid octal fields, mangled PAX key-value pairs). |
| `-10` | **`MPTAR_ERR_OVERFLOW`** | An arithmetic calculation wrapped around or an internal buffer write boundary violation occurred. |
| `-11` | **`MPTAR_ERR_RESERVED_LEGACY_2`** | Reserved / Legacy. Formerly used to signal PAX need due to an exceedingly long path, but now checks are done without an error signaling that. |
| `-12` | **`MPTAR_ERR_NS_CONVERSION_FAILED`** | Conversion of sub-second nanoseconds to string failed (nanosecond value exceeded `999,999,999`). |
| `-13` | **`MPTAR_ERR_BUFFER_TOO_SMALL`** | Output or destination buffer lacked sufficient capacity to hold the formatted data. |

You can check how to handle the errors in a [Error Handling section](error-handling.md)