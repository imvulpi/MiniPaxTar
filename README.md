# minipaxtar

`minipaxtar` is a small, freestanding C library for reading and writing **POSIX PAX** and **USTAR** tar archives. It is distributed under the *MIT license*.

It was designed as a safe, modern alternative to legacy lightweight tar libraries like `microtar`. Many existing micro-libraries lack defensive parsing, making them vulnerable to memory corruption or infinite loops when handling malformed input. `minipaxtar` provides strict bounds checking and robust error handling while keeping a minimal binary footprint.

📚 **[Read the Full Documentation](https://imvulpi.github.io/MiniPaxTar/)**

---

## Documentation Links

* ⏩ **Overview & Getting Started:**
  * [Quickstart Guide](https://imvulpi.github.io/MiniPaxTar/overview/quickstart/)
  * [Reading Archives](https://imvulpi.github.io/MiniPaxTar/overview/reading/)
  * [Writing Archives](https://imvulpi.github.io/MiniPaxTar/overview/writing/)
  * [Extracting Files](https://imvulpi.github.io/MiniPaxTar/overview/extracting/)

* 📘 **Guides & Configuration:**
  * [Error Handling](https://imvulpi.github.io/MiniPaxTar/guides/error-handling/)
  * [Status Codes Reference](https://imvulpi.github.io/MiniPaxTar/guides/status-codes/)
  * [Compile-Time Configuration & Defines](https://imvulpi.github.io/MiniPaxTar/guides/config-defines/)

* ⚙️ **API Reference:**
  * [Full API Group Reference](https://imvulpi.github.io/MiniPaxTar/api/index_groups/)
  * [Full Structures API Reference](https://imvulpi.github.io/MiniPaxTar/api/index_classes/)
  * [All Files API Reference](https://imvulpi.github.io/MiniPaxTar/api/index_files/)
---

## Core Highlights

* **Simple Integration:** Available as both a standard `.c`/`.h` file pair and a single-header (`stb`-style) distribution.
* **Very Lightweight:** ~7 KB for read-only or write-only builds, ~14 KB for full functionality.
* **Portable:** Compiles cleanly across mainstream and minimal compilers, including `GCC`, `Clang`, `MSVC`, `TCC`, and `chibicc`.
* **Broad C Support:** Full compatibility from **C89** through modern C drafts (like C2Y).
* **Defensive Safety:** Thorough input validation, safe integer arithmetic, and defensive memory lifetime tracking.
* **100% Freestanding:** Zero standard library dependencies (`MPTAR_NO_STD`). Custom I/O and memory routines are passed explicitly through user configurations.

---

## Quick Integration

`minipaxtar` is designed to adapt to any C build system or project layout:

* **CMake Integration:** Fetch or include via `add_subdirectory()` or `FetchContent`.
* **Manual Drop-in:** Add `minipaxtar.c` and `minipaxtar.h` directly to your source tree.
* **Single-Header Mode (`stb`-style):** Include `minipaxtar.h` anywhere, and instantiate the implementation in **exactly one** C source file:

```c
#define MINIPAXTAR_IMPLEMENTATION
#include "minipaxtar.h"
```

📑 **[View Full Integration Guide](https://imvulpi.github.io/MiniPaxTar/overview/integration/)**

---

## Memory & Resource Design

* **Zero Implicit Allocation:** `minipaxtar` never calls `malloc` or system allocators internally unless you explicitly supply allocation callbacks in reader/writer contexts.
* **Clean Lifetime Control:** Metadata allocations are isolated and cleared defensively (`mptar_reader_free_metadata`). Pointers are reset to `NULL` upon being freed, preventing double-free errors even during complex error-recovery paths.
* **State Safety:** Read and write contexts maintain clear offset tracking, avoiding out-of-bounds reads on truncated or corrupt archive headers.

---

## Comparison with `microtar`

`minipaxtar` was created as a modern, safe replacement for `microtar`, which has been unmaintained since 2017 and suffers from severe limits and security vulnerabilities.

| Feature / Property | `microtar` | `minipaxtar` |
| --- | --- | --- |
| **Security Status** | Vulnerable ([CVE-2026-43623](https://www.cve.org/CVERecord?id=CVE-2026-43623)) | Defensive design, strict bounds checking |
| **Standard Support** | Basic V7 / Custom subset | USTAR, POSIX PAX, GNU binary extensions |
| **Max Filename Length** | 100 bytes (No prefix field) | Unlimited via PAX extended headers |
| **Max File Size** | 4 GB limit (`unsigned` 32-bit `size`) | Near-unlimited (Full 64-bit `uint64_t` support) |
| **Standard Library Reliance** | Heavily Coupled | Fully decoupled (`MPTAR_NO_STD`) |
| **Zero-Copy Skipping** | Requires seek/read operations | Instant offset and byte tracking |
| **Write API Size** | ~5.2 KB | ~6.3 KB |

> For a deep dive into code differences, API mechanics, and architectural breakdowns, see [COMPARISON.md](COMPARISON.md).

---

## License

This project is licensed under the MIT License. See the `LICENSE` file for details.

---

📶 **[More in the Official Documentation](https://imvulpi.github.io/MiniPaxTar/)**
