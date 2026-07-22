# minipaxtar

`minipaxtar` is a small, freestanding C library for reading and writing **POSIX PAX** and **USTAR** tar archives. It is distributed under the *MIT license*.

It is designed as a safe, modern alternative to lightweight tar libraries like `microtar`. Many existing micro-libraries lack defensive parsing, making them vulnerable to memory corruption or infinite loops when handling malformed input. `minipaxtar` provides strict bounds checking and robust error handling while keeping a minimal binary footprint.

---

## Core Highlights

* **Simple Structure:** Available as both a standard `.c`/`.h` file pair and a single-header (`stb`-style) distribution.
* **Very Lightweight:** ~7 KB for read-only or write-only builds, ~14 KB for full functionality.
* **Portable:** Compiles cleanly across mainstream and minimal compilers, including `GCC`, `Clang`, `MSVC`, `TCC`, and `chibicc`.
* **C89 Compatible:** Broad standard support from **C89** through current C drafts (like C2Y).
* **Safe Error Handling:** Thorough input validation, safe integer checks, and defensive memory lifetime tracking.
* **Freestanding:** Zero standard library dependencies. Custom I/O and memory routines are passed explicitly through user configurations. Just remember to define `MPTAR_NO_STD` for builds with no standard library dependency.

---

## Modular Configuration

You can tailor the library at compile time to include only the functions your project needs. If you only need to read or write archives, disabling the unneeded path reduces the binary size significantly.

### Compile-Time Defines

| Define | Description |
| --- | --- |
| `MPTAR_NO_READ` | Excludes the archive parsing and reading implementation. |
| `MPTAR_NO_WRITE` | Excludes the archive creation and writing implementation. |
| `MPTAR_SUPPORT_SPECIAL` | Enables handling of device files (includes devminor/devmajor in build). |
| `MPTAR_SUPPORT_EXTRA_TIMES` | Enables parsing/writing extended timestamp metadata (atime, ctime). |

If your system or runtime framework already provides string-to-integer or integer-to-string conversion utilities, you can supply your own via externs and defines enabling it. Overriding these built-in helpers can reduce binary footprint even further in space-constrained embedded setups.

---

## Binary Footprint

`minipaxtar` keeps code size low without sacrificing structural checks:

* **Write-only or Read-only:** ~7 KB
* **Full feature set:** ~14 KB (with everything enabled)

Because the codebase does not depend on `libc` primitives like `printf` or standard heap routines, there is no unexpected code bloat pulled in at link time.

---

## Memory & Resource Management

* **Zero Implicit Allocation:** The library does not call `malloc` or system allocators internally unless you explicitly supply allocation function pointers in the reader or writer configuration.
* **Clean Lifetime Control:** Metadata allocations are isolated and cleared defensively (`mptar_reader_free_metadata`). Pointers are reset to null upon being freed, preventing double-free errors even if cleanup routines are called multiple times in error recovery paths.
* **State Safety:** Read and write contexts maintain clear offset tracking, avoiding out-of-bounds reads on truncated or corrupt archive headers.

---

## Standards & Compiler Compatibility

The codebase is structured to build quietly under strict compiler settings across generations of C standards.

* **C89 to C2Y:** Written to respect C89 variable scope and type rules while avoiding features deprecated in newer standard drafts.
* **Tested Compilers:** Builds cleanly on GCC, Clang, MSVC, TCC (Tiny C Compiler), and `chibicc`.

---

## Comparison with `microtar`

`minipaxtar` was created as a modern, safe alternative to `microtar`, which has been unmaintained since 2017 and suffers from severe limitations and memory vulnerabilities.

| Feature / Property | `microtar` | `minipaxtar` |
| :--- | :--- | :--- |
| **Security Status** | Vulnerable ([CVE-2026-43623](https://www.cve.org/CVERecord?id=CVE-2026-43623)) | Defensive design, strict bounds checking |
| **Standard Support** | Basic V7 / Custom subset | USTAR, POSIX PAX, GNU binary extensions |
| **Max Filename Length** | 100 bytes (No prefix field) | Unlimited via PAX extended headers |
| **Max File Size** | 4 GB limit (`unsigned` 32-bit `size`) | Near-unlimited (Full 64-bit `uint64_t` support) |
| **Standard Library Reliance** | Heavily Coupled to the standard library | Fully decoupled from the standard library with `MPTAR_NO_STD` |
| **Zero-Copy Skipping** | Requires seek/read operations | Instant offset and byte tracking |
| **Write API Size** | ~5.2 KB | ~6.3 KB |

> For a deep dive into code differences, API mechanics, and architectural breakdowns, see [COMPARISON.md](COMPARISON.md).
---

## Integration

### Option 1: Standard `.c` and `.h`

Add `minipaxtar.c` and `minipaxtar.h` directly to your project's build system or Makefile.

### Option 2: Single-Header Mode

Include `minipaxtar.h` in your project. In **one** C source file, define `MINIPAXTAR_IMPLEMENTATION` before including the header to instantiate the code:

```c
#define MINIPAXTAR_IMPLEMENTATION
#include "minipaxtar.h"
```

In all other source files, simply include the header normally:

```c
#include "minipaxtar.h"
```

---

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.