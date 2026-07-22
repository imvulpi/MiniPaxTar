# Architectural Comparison: `minipaxtar` vs `microtar`

This document outlines the design differences, feature coverage, and safety guarantees between `minipaxtar` and `microtar`.

---

## Quick comparison

| Feature / Property | `microtar` | `minipaxtar` |
| :--- | :--- | :--- |
| **Active Maintenance** | Abandoned (Last activity ~2017) | Actively maintained |
| **Security Status** | Vulnerable ([CVE-2026-43623](https://www.cve.org/CVERecord?id=CVE-2026-43623)) | Defensive design, strict bounds checking |
| **Standard Support** | No real standard (Stripped old V7) | USTAR, POSIX PAX, GNU binary extensions |
| **Max Filename Length** | 100 bytes (No prefix field) | Unlimited via PAX extended headers |
| **Max File Size** | 4 GB limit (`unsigned` 32-bit `size`) | Near-unlimited (Full 64-bit `uint64_t` support) |
| **Standard Library Reliance** | Heavily Coupled to the standard library | Fully decoupled from the standard library with `MPTAR_NO_STD` |
| **Zero-Copy Data Skipping** | Requires seek/read operations | Instant offset/remaining calculation |
| **Write API Size** | ~5.2 KB | ~6.3 KB |

---

### Security

| `microtar` | `minipaxtar` |
| --- | --- |
| **Vulnerable:** Unsafe string copies causing stack overflows on malformed inputs ([CVE-2026-43623](https://www.cve.org/CVERecord?id=CVE-2026-43623)). | **Defensive Checks:** Every input byte is treated as untrusted data, strings are safely bounded and null-terminated. (no strcpy) |
| **No Integer Guards:** Susceptible to integer wraparounds during header size parsing and stream offset calculations. | **Checked Arithmetic:** Strict overflow safeguards preventing invalid memory indexing or buffer allocations. |

---

### Format Support

| `microtar` | `minipaxtar` |
| --- | --- |
| **Old V7 Subset:** Only supports a stripped V7 header layout; no USTAR or POSIX compliance. | **Full Standard Support:** Normalizes USTAR, POSIX PAX extended attributes, and GNU binary encodings. |
| **Strict 100-Byte Paths:** Lacks standard USTAR `prefix` or PAX fields; paths limited to 100 bytes. | **Unlimited Path Lengths:** Dynamically handles standard `prefix` splitting and arbitrarily long PAX `path` attributes. |
| **4 GB Size Limit:** Uses a 32-bit `unsigned int` for `size`, hard-capping supported file sizes at 4 GB. | **64-bit Size Support:** Uses `uint64_t` for `size` fields, supporting near-unlimited file sizes via PAX or binary encodings. |
| **Missing Metadata:** No `uname`, `gname`, `devmajor`, `devminor`, `atime`, `ctime`, and explicit version magic. | **Comprehensive Metadata:** All standard USTAR metadata, All PAX extended metadata, device numbers, high-precision sub-second timestamps, and custom attributes. |

---

### API Architecture & Design

| `microtar` | `minipaxtar` |
| --- | --- |
| **Rigid Stream Requirements:** Mandates `seek` and `close` function pointers in its handle struct, breaking compatibility with non-seekable streams like network sockets, pipes, or append-only flash. | **Stream-Agnostic Design:** Operates strictly on linear read/write callbacks. Advances state by tracking relative byte offsets, making it natively compatible with forward-only streams without needing fake seek implementations. |
| **Reads for Payload Skipping:** Skipping unread file contents requires sequentially reading dummy data or forcing file stream seek calls (`mtar_seek`). | **Zero-Copy Payload Skipping:** `mptar_skip_data` adjusts internal byte counters directly without performing I/O reads, enabling fast `mmap` / `MapViewOfFile` integration. |
| **Cluttered & Unsafe:** Stuffed with obvious, redundant comments while performing zero return-value validation or error handling. | **Clean & Defensive:** Self-documenting code with systematic return-code checking and robust error checking at every step. |
| **All-or-Nothing Compilation:** Links read, write, and header utilities unconditionally, increasing binary footprint even if you only need a single feature. | **Granular Feature Guards:** Easily disable unneeded code paths at compile time using `MPTAR_NO_READ`, `MPTAR_NO_WRITE`, `MPTAR_SUPPORT_SPECIAL`, or `MPTAR_SUPPORT_EXTRA_TIMES`. |
| **Minimal Error & Bounds Checking:** Performs almost zero validation on input byte streams, leaving callers exposed to malformed input crashes. | **Hardened Input Guarding:** Strictly checks all buffer lengths, string terminators, and integer math operations before processing data. |
| **Dual-File Only:** Provided exclusively as separate `microtar.c` and `microtar.h` source files. | **Flexible Distribution:** Ships as both a standard `.c`/`.h` file pair and an `stb`-style single-header (`minipaxtar.h`) distribution. |
| **Cramming & Unsafe Types:** Crams read, write, seek, and close callbacks into a single `mtar_t` handle. Uses `unsigned int` for sizes and offsets, hard-capping files at 4 GB and risking integer wraparounds on 16/32-bit systems. | **Isolated Structs & Safe Types:** Completely separates `mptar_reader` and `mptar_writer` contexts. Uses `size_t` for memory bounds and `uint64_t` for file offsets, making file size handling near-unlimited and integer-safe. |
| **Old Non-Standard Struct:** Uses a stripped-down header struct (`mtar_header_t`). It is not real V7 or USTAR compliant header, completely omits standard fields like `gid`, `uname`, `gname`, and `prefix`, and hard-caps file paths at 100 bytes. | **Fully Standard Compliant:** Parses true POSIX USTAR, PAX extended attributes (`typeflag 'x'` and `'g'`), and GNU binary encodings transparently into a comprehensive, normalized `mptar_metadata` structure. |
| **Coarse Error Codes:** Generic status reporting (`MTAR_EFAILURE`, `MTAR_EOPENFAIL`) that leaves debugging to guesswork. | **Granular Error Codes:** Specific return codes for header checksum failures, truncated streams, invalid arguments, and arithmetic overflows. |
| **Manual Integration:** Lacks build system files, forcing manual compilation setup or custom script maintenance. | **First-Class CMake Support:** Plug-and-play integration via `add_subdirectory()` or `FetchContent` out of the box. |
| **Dead Code & Unused Logic:** Contains orphaned helper functions which causes binary size increases. | **Zero Dead Code:** Lean codebase where every single line and helper function serves an active purpose. |

---

## Extended Overview:

### Microtar Header Issues

#### Understanding the Legacy V7 Archive Format

To understand why `microtar`'s header design is problematic, it helps to look back at where tar started.

The original tar format originated in 1979 with Version 7 (V7) AT&T UNIX. You can view the raw C structure definition for this legacy layout in the [Ubuntu tar(5) man page for Old-Style Archive Format](https://manpages.ubuntu.com/manpages/trusty/man5/tar.5.html#Old-Style_Archive_Format).

V7 was designed for simple tape backups in 1979. Today, POSIX USTAR (Header standard since 1988) and PAX extended attributes are the universal standards. Modern tar tools expect these headers to ensure file metadata, permissions, and long filenames are preserved accurately across different operating systems.

---

#### The Hidden Disconnect: `mtar_raw_header_t` vs `mtar_header_t`

`microtar` handles header parsing in a very strange, incomplete way. Inside its internal `microtar.c` source file, it defines a standard 512-byte raw V7 header struct to read bytes off the disk:

```c
/* Defined internally inside microtar.c */
typedef struct {
  char name[100];
  char mode[8];
  char owner[8];
  char group[8];
  char size[12];
  char mtime[12];
  char checksum[8];
  char type;
  char linkname[100];
  char _padding[255];
} mtar_raw_header_t;

```

However, in its public-facing header file (`microtar.h`), it exposes a completely different, stripped-down metadata struct to the caller:

```c
/* Exposed publicly to users in microtar.h */
typedef struct {
  unsigned mode;
  unsigned owner;
  unsigned size;
  unsigned mtime;
  unsigned type;
  char name[100];
  char linkname[100];
} mtar_header_t;

```

This raises serious questions about the library's design. The internal parser reads V7 fields like `group` (`gid`) and `checksum`, but then **actively discards them** when populating the user's `mtar_header_t`. The public API simply hides metadata that the library already spent I/O cycles reading.
> I could understand discarding the checksum, but why skip over other fields? Especially gid (Group), I mean you already defined Owner in the incomplete public header. 

---

#### Missing POSIX USTAR & PAX Features

Because `microtar` never moved beyond this half-implemented V7 layout, it completely lacks support for modern archive formats:

* **No `prefix` Support:** Standard USTAR uses a 155-byte `prefix` field alongside `name` to support relative file paths up to 256 bytes without requiring extended attributes.
* **No `uname` / `gname`:** Lacks human-readable user and group string fields (e.g., `"root"`, `"www-data"`), forcing system utilities to rely on raw numeric IDs.
* **No `devmajor` / `devminor`:** Cannot represent character or block device nodes.
* **No `magic` / `version` Validation:** Lacks standard signature fields (`"ustar\000"`). It cannot verify whether an archive follows POSIX rules or if it is just a stream of corrupted garbage bytes.
* **No PAX Extended Attribute Support:** Lacks support for PAX header blocks (`typeflag 'x'` and `'g'`), preventing support for arbitrary-length paths, custom key-value metadata, or high-precision sub-second timestamps (`atime`/`ctime`).

---

#### Consequences of the header

* **Data Truncation:** Any relative file path longer than 100 characters is truncated or rejected because `microtar` hardcodes a 100-byte limit (`char name[100]`) with no fallback mechanism.
* **4 GB File Size Cap:** Defining `size` as a 32-bit `unsigned int` caps maximum file handling at 4 GB. Standard USTAR supports up to 8 GB via octal strings, while PAX attributes and `minipaxtar` support full 64-bit (`uint64_t`) sizes for near-unlimited file sizes.
* **Lost Ownership & Permission Context:** Extracting archives processed by `microtar` strips group ownership information entirely, even though the raw V7 bytes contained it.

### Microtar I/O Architecture Issues (`mtar_t`)

`microtar` wraps all stream operations, internal state tracking, and I/O function pointers into a single monolithic handle:

```c
struct mtar_t {
  int (*read)(mtar_t *tar, void *data, unsigned size);
  int (*write)(mtar_t *tar, const void *data, unsigned size);
  int (*seek)(mtar_t *tar, unsigned pos);
  int (*close)(mtar_t *tar);
  void *stream;
  unsigned pos;
  unsigned remaining_data;
  unsigned last_header;
};

```

While this appears simple at first glance, its design introduces several severe API friction points and unsafe coupling bugs.

---

#### 1. Inflexible Callback Signatures & Lack of Standard

Notice how `microtar` defines its read/write callbacks:

```c
int (*read)(mtar_t *tar, void *data, unsigned size);
```

Passing `mtar_t *tar` as the first argument instead of an opaque `void *user_data` pointer forces callers to wrap their stream handles inside `mtar_t`'s `void *stream` field.

* **Prevents Direct Function Binding:** Standard C I/O functions (and common OS primitives) expect signature patterns like `(void *context, const void *buf, size_t size)`. Because `microtar` passes its own `mtar_t *` struct, you cannot bind standard functions directly - you are forced to write boilerplate wrapper functions for every custom stream.
* **Risk of Undefined Behavior / State Corruption:** Handing the library's internal state struct (`mtar_t *`) directly to custom callback implementations is dangerous. If a custom `read` or `write` function modifies `tar->pos`, `tar->remaining_data`, or `tar->last_header` during an I/O operation, it corrupts the library’s internal stream tracking, leading to silent archive corruption or infinite loops.

In contrast, `minipaxtar` uses standard, clean callback signatures:

```c
typedef mptar_size_t (*mptar_write_fn)(void *user_data, const void *buffer, mptar_size_t size);
```

This accepts any user context pointer directly, eliminating the risk of stream state tampering and enabling seamless integration with existing stream APIs.

#### 2. Mandatory `seek` Obligation

In `microtar`, `seek` is a core struct member. The reader heavily relies on stream repositioning (e.g., seeking back to `last_header` when reading finished data, or seeking past raw headers).

* **Breaks Non-Seekable Streams:** This hard dependency on `seek` makes `microtar` fundamentally incompatible with streaming sources where standard seek operations are impossible - such as TCP sockets, HTTP payloads, IPC pipes, serial ports, or append-only flash memory.
* **`minipaxtar` Alternative:** `minipaxtar` operates on pure, forward-only stream callbacks. Payload skipping (`mptar_skip_data`) simply corrects internal byte counters without forcing stream rewind or active disk seeks.

#### 3. Fragile State Tracking (`last_header` and `remaining_data`)

Keeping state counters like `last_header` and `remaining_data` mixed in the same struct alongside function pointers and the stream context creates a fragile state machine:

* **State Spill Across Operations:** Because reading a header changes internal stream state (`pos`, `last_header`), calling `mtar_read_header` or `mtar_find` alters the stream's position indicator irreversibly. Interrupted reads leave the `mtar_t` struct in an inconsistent state (`remaining_data > 0`), making error recovery difficult without tearing down and re-opening the archive.
* **Separation of Concerns in `minipaxtar`:** `minipaxtar` decouples state tracking cleanly into distinct `mptar_reader` / `mptar_writer` execution contexts and pure `mptar_metadata` value containers.

#### 4. Type Truncation Risks

Using basic `unsigned` integers for `pos`, `remaining_data`, and `last_header` limits stream tracking to 32-bit values on standard platforms, triggering integer wraparound bugs on archives or streams larger than 4 GB. `minipaxtar` uses `uint64_t` for all file offsets and stream sizes.

### Error Diagnostics & Binary Bloat (`mtar_strerror`)

Another subtle issue in `microtar`’s design is how it handles string conversion for error codes.

Inside `microtar.c`, it provides `mtar_strerror`:

```c
const char* mtar_strerror(int err) {
  switch (err) {
    case MTAR_ESUCCESS     : return "success";
    case MTAR_EFAILURE     : return "failure";
    case MTAR_EOPENFAIL    : return "could not open";
    case MTAR_EREADFAIL    : return "could not read";
    case MTAR_EWRITEFAIL   : return "could not write";
    case MTAR_ESEEKFAIL    : return "could not seek";
    case MTAR_EBADCHKSUM   : return "bad checksum";
    case MTAR_ENULLRECORD  : return "null record";
    case MTAR_ENOTFOUND    : return "file not found";
  }
  return "unknown error";
}

```

While having a human-readable string lookup helper is convenient, bundling it directly inside the main `microtar.c` source file introduces several trade-offs:

#### 1. Dead Code and Mandatory Bloat

Because `microtar` itself never calls `mtar_strerror` internally, it exists purely as an optional user-facing helper. However, in embedded or space-constrained builds where dead-code elimination / Link-Time Optimization (LTO) or `-ffunction-sections` / `-fdata-sections` are not explicitly configured, compiling `microtar.c` unconditionally pulls in:

* The lookup table logic and switch statement overhead.
* The embedded string literals (`"could not open"`, `"bad checksum"`, etc.) in the `.rodata` section.

For extremely lean targets where every byte counts, forcing unused string tables into the object file adds unnecessary overhead.

### Header Writing API & Buffer Safety (`mtar_write_*`)

Look at the code below:

```c
int mtar_write_header(mtar_t *tar, const mtar_header_t *h) {
  mtar_raw_header_t rh;
  /* Build raw header and write */
  header_to_raw(&rh, h);
  tar->remaining_data = h->size;
  return twrite(tar, &rh, sizeof(rh));
}

int mtar_write_file_header(mtar_t *tar, const char *name, unsigned size) {
  mtar_header_t h;
  /* Build header */
  memset(&h, 0, sizeof(h));
  strcpy(h.name, name);
  h.size = size;
  h.type = MTAR_TREG;
  h.mode = 0664;
  /* Write header */
  return mtar_write_header(tar, &h);
}

int mtar_write_dir_header(mtar_t *tar, const char *name) {
  mtar_header_t h;
  /* Build header */
  memset(&h, 0, sizeof(h));
  strcpy(h.name, name);
  h.type = MTAR_TDIR;
  h.mode = 0775;
  /* Write header */
  return mtar_write_header(tar, &h);
}

```

This helper pattern highlights two distinct design problems: unsafe string handling and rigid, opinionated defaults.

---

#### 1. Unsafe String Operations (`strcpy` Buffer Overflow Risk)

Look closely at `mtar_write_file_header` and `mtar_write_dir_header`:

```c
strcpy(h.name, name);
```

* **No Truncation or Length Guarding:** It uses `strcpy` to copy the incoming `name` parameter directly into `h.name`, which is a fixed array of `char name[100]`.
* **Stack Memory Corruption:** If a caller passes a path longer than 100 bytes, `strcpy` will silently write past the bounds of `h.name`, corrupting adjacent fields on the stack frame (such as `h.linkname`, `h.mode`, or return addresses).
* **Missing Safe Alternatives:** Even a basic defensive check using `strncpy` or explicit length validation was omitted, leaving application code vulnerable to stack buffer overflows on unvetted file paths.

This is a pattern that's as well shared in `mtar_read_*` which could lead to **arbitrary code execution.** There even is an [unresolved high severity (8.8) CVE. ](https://www.cve.org/CVERecord?id=CVE-2026-43623) in the `microtar` library.

#### 2. Rigid Defaults vs. Transparent Struct Modification

Functions like `mtar_write_file_header` and `mtar_write_dir_header` attempt to simplify creation by hardcoding default file permissions (`0664` for files, `0775` for directories).

While convenience helpers can be useful, this pattern creates issues when you need control:

* **Inflexible Presets:** If you want custom permissions (e.g., `0600` or executable `0755`), custom timestamps, or ownership UIDs, these wrapper functions cannot be used. You are forced to drop down, manually allocate `mtar_header_t`, zero it out, and call `mtar_write_header` yourself.
* **Hidden Magic Values:** Hiding permissions and types inside wrapper functions makes it less obvious to the developer what exact metadata is being committed to the archive.

#### How `minipaxtar` Solves This

`minipaxtar` avoids opinionated, unsafe helper wrappers in favor of a clean, struct-driven approach:

* **Direct Metadata Customization:** You simply populate an `mptar_metadata` struct with whatever permissions (`mode`), timestamps (`mtime`), owner names, or custom attributes you want - without arbitrary default restrictions.
* **Strict Bounded String Copies:** String copies and path handling operations are bounded using explicit size limits, ensuring long file paths can never trigger buffer overflows or silent memory corruption.

---

### Vulnerability Overview: Stack Buffer Overflow ([CVE-2026-43623](https://www.cve.org/CVERecord?id=CVE-2026-43623))

One of the most critical risks when using `microtar` on untrusted or external archives is its vulnerability to memory corruption and remote code execution.

#### The Vulnerable Code Path

When reading or searching an archive (`mtar_open`, `mtar_read_header`, `mtar_find`), `microtar` invokes `raw_to_header()` to convert the on-disk byte block into its internal struct:

```c
static int raw_to_header(mtar_header_t *h, const mtar_raw_header_t *rh) {
  unsigned chksum1, chksum2;

  /* If the checksum starts with a null byte we assume the record is NULL */
  if (*rh->checksum == '\0') {
    return MTAR_ENULLRECORD;
  }

  /* Build and compare checksum */
  chksum1 = checksum(rh);
  sscanf(rh->checksum, "%o", &chksum2);
  if (chksum1 != chksum2) {
    return MTAR_EBADCHKSUM;
  }

  /* Load raw header into header */
  sscanf(rh->mode, "%o", &h->mode);
  sscanf(rh->owner, "%o", &h->owner);
  sscanf(rh->size, "%o", &h->size);
  sscanf(rh->mtime, "%o", &h->mtime);
  h->type = rh->type;
  strcpy(h->name, rh->name);          /* <--- VULNERABILITY 1 */
  strcpy(h->linkname, rh->linkname);  /* <--- VULNERABILITY 2 */

  return MTAR_ESUCCESS;
}

```

#### The reason: Unbounded `strcpy()` on Fixed-Width Fields

The POSIX TAR format explicitly permits the fixed 100-byte `name` and `linkname` fields to be **fully populated with non-null bytes** if the string reaches maximum capacity. Null termination on disk is optional in standard TAR streams when a string spans the full 100-byte allocation.

1. **Missing Null Termination:** If an attacker crafts a TAR header where `rh->name` or `rh->linkname` fills all 100 bytes without a trailing `\0`, `strcpy()` will continue scanning and copying bytes out-of-bounds.
2. **Stack Memory Overflow:** `strcpy()` reads past the end of `rh` and writes up to 355+ bytes into the destination stack buffer `h->name` (which is only sized for 100 bytes).
3. **Exploitability:** This results in an out-of-bounds read and a stack buffer overflow, leading to instant process crashes (Denial of Service) or potential arbitrary code execution when processing attacker-supplied files.

#### How `minipaxtar` Guarantees Bounded Parsing

`minipaxtar` treats every byte read from an archive stream as untrusted input:

* **Bounded String Copies:** Uses strict length-bounded copying routines (such as `memcpy` with explicit size limits) instead of unsafe unbounded string functions like `strcpy`. In fact there is no `strcpy` use at all, you can easily check that :)
* **Guaranteed Null Termination:** Destination buffers are explicitly terminated in memory regardless of how raw bytes are formatted on disk.
* **Header & Extended Field Guards:** Validates buffer offsets, field lengths, and integer calculations before performing memory ops, making input parsing immune to stack corruption attacks.

---

### Code Quality: Comments, Magic

Beyond memory safety and API architecture, looking directly at the source implementation reveals significant friction in code readability, naming conventions, and comment quality.

Despite being a very compact library (~400 lines), a large portion of `microtar`'s lines are taken up by "noisy" comments that explain obvious statements while omitting documentation where non-obvious logic and magic numbers are actually used.

#### 1. Redundant Comments on Self-Explanatory Code

In good C practices, code should be self-documenting. Comments should explain *why* something complex is being done, not restate *what* the C syntax is doing on the next line.

In `microtar`, we see noise comments repeating the obvious:

```c
/* Open file */
tar->stream = fopen(filename, mode);

/* Return ok */
return MTAR_ESUCCESS;

/* Write header */
return mtar_write_header(tar, &h);

```

It's real, check it yourself. The ridiculous second example is from `mtar_open`.

When every line of trivial code is paired with a literal restatement, reading the file becomes exhausting because half the vertical space is noise.

---

#### 2. Vague Function Naming vs. Missing Comments on Magic Numbers

Instead of using descriptive function names, `microtar` uses ambiguous nouns like `checksum()`:

```c
static unsigned checksum(const mtar_raw_header_t* rh) {
  unsigned i;
  unsigned char *p = (unsigned char*) rh;
  unsigned res = 256; /* <--- Why 256? No comment, macro, or explanation. */
  for (i = 0; i < offsetof(mtar_raw_header_t, checksum); i++) {
    res += p[i];
  }
  for (i = offsetof(mtar_raw_header_t, type); i < sizeof(*rh); i++) {
    res += p[i];
  }
  return res;
}

```

* **Vague Naming:** Calling the helper `checksum` forces the author to add redundant comments elsewhere (e.g., `/* Calculate and write checksum */`). Naming it `calc_header_checksum()` makes the code self-documenting without needing comment clutter.
* **Unexplained TAR Magic Numbers:** `unsigned res = 256;` is a classic TAR specification detail: during checksum calculation, the 8-byte header `checksum` field is treated as if filled with ASCII spaces (`' '`, value `32` / `0x20`). Since $8 \times 32 = 256$, starting `res` at `256` skips summing those 8 bytes.
* **The Contradiction:** The codebase spends a line writing `/* Open file */` before `fopen`, yet leaves a non-trivial algorithm trick like `res = 256` completely unmentioned and unexplained without a `#define MPTAR_SPACE_CHECKSUM_VAL 256` or an explanatory comment.

---

#### How `minipaxtar` Approaches Clean Code

* **Self-Documenting API:** Function names use descriptive verbs (`mptar_read_header`, `mptar_skip_data`) so callers and maintainers immediately understand their purpose without needing trivial comments.
* **Explicit Named Constants:** Replaces raw numbers with clear macros or enumeration definitions so protocol rules (like 512-byte block padding or POSIX USTAR magic strings) are obvious.
* **High-Signal Comments:** Comments are reserved exclusively for non-obvious bits - such as POSIX specification nuances, state machine invariants, or memory lifetime guarantees.

To see the practical difference between obscure magic numbers and expressive design, let's compare how `microtar` and `minipaxtar` handle calculating the standard TAR header checksum.

##### The Issue in `microtar`

Like we discussed above `microtar` hides the checksum logic inside an obscure function with an unmentioned trick:

```c
static unsigned checksum(const mtar_raw_header_t* rh) {
  unsigned i;
  unsigned char *p = (unsigned char*) rh;
  unsigned res = 256; /* Why 256? No comment, macro, or explanation. */
  for (i = 0; i < offsetof(mtar_raw_header_t, checksum); i++) {
    res += p[i];
  }
  for (i = offsetof(mtar_raw_header_t, type); i < sizeof(*rh); i++) {
    res += p[i];
  }
  return res;
}

```

##### The `minipaxtar` Approach: Explicit Constants & Clear Intent

Here is a real-world example of how `minipaxtar` handles the same TAR checksum calculation while making the `256` trick self-explanatory:

```c
static mptar_uint32 mptar_calculate_header_checksum(char* header_block) {
    if (header_block == MPTAR_NULL) return 0;

    const unsigned char* block = (const unsigned char*)header_block;
    mptar_uint32 sum = 0;    

    int i = 0;
    for ( ; i < MPTAR_CHECKSUM_OFFSET; i++) {
        sum += block[i];
    }

    /* Hardcoded constant value for the 8 space characters (8 * 32 = 256) */
    sum += MPTAR_SPACE_CHECKSUM_VAL; 

    for (i = MPTAR_CHECKSUM_END_OFFSET; i < MPTAR_USTAR_HEADER_SIZE; i++) {
        sum += block[i];
    }
    
    return sum;
}

```

Notice how `minipaxtar`:

1. Uses descriptive offsets (`MPTAR_CHECKSUM_OFFSET` and `MPTAR_CHECKSUM_END_OFFSET`) to explicitly jump over the checksum field.
2. Names the constant (`MPTAR_SPACE_CHECKSUM_VAL`) and adds a brief comment explaining **why** 256 is added ($8 \times 32$).

---

#### Knowing When *Not* to Over-Define (Overusing of Macros)

Writing clean code doesn't mean creating a macro for every single digit. Self-documenting code strikes a balance: macros should define protocol boundaries, memory allocations, and obscure values, while standard, self-explanatory math can remain inline.

For example, when `minipaxtar` calculates PAX record lengths, powers-of-ten thresholds don't need individual `#define` statements because decimal place values are universally understood. However, a targeted inline comment is used to explain non-obvious API behavior (such as returning `0` to signal overflow when no error pointer is passed):

```c
static mptar_uint32 mptar_pax_calculate_record_len(mptar_uint32 data_len) {
    if (data_len > MPTAR_PAX_MAX_DATA_LEN) return 0; /* Would overflow */

    mptar_uint32 digits = 1;
    if      (data_len >= 1000000000U) digits = 10;
    else if (data_len >= 100000000U)  digits = 9;
    else if (data_len >= 10000000U)   digits = 8;
    else if (data_len >= 1000000U)    digits = 7;
    else if (data_len >= 100000U)     digits = 6;
    else if (data_len >= 10000U)      digits = 5;
    else if (data_len >= 1000U)       digits = 4;
    else if (data_len >= 100U)        digits = 3;
    else if (data_len >= 10U)         digits = 2;

    mptar_uint32 total = data_len + digits;
    
    mptar_uint32 final_digits = 1;
    if      (total >= 1000000000U) final_digits = 10;
    else if (total >= 100000000U)  final_digits = 9;
    else if (total >= 10000000U)   final_digits = 8;
    else if (total >= 1000000U)    final_digits = 7;
    else if (total >= 100000U)     final_digits = 6;
    else if (total >= 10000U)      final_digits = 5;
    else if (total >= 1000U)       final_digits = 4;
    else if (total >= 100U)        final_digits = 3;
    else if (total >= 10U)         final_digits = 2;

    return data_len + final_digits;
}
```

That's enough of a lesson for this document. Now use `minipaxtar` ;)