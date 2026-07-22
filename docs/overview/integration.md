# Integration (Manual and CMake)

This guide details how to integrate `minipaxtar` into your project - whether you use **CMake** (`FetchContent`, submodules, packages) or prefer **Manual Integration** (stb-style header-only or classic `.c` + `.h` copying).

---

## 1. Manual Integration (Non-CMake)

If you don't use CMake, `minipaxtar` is designed to be trivial to vendor directly into your source.

## Option A: Classical (`minipaxtar.c` + `minipaxtar.h`)
1. Copy `minipaxtar.h` and `minipaxtar.c` into your project's include/source directory (or add as a Git submodule).
2. Add `minipaxtar.c` to your build system's source list (Makefile, MSBuild, Premake, Ninja, etc.).
3. `#include "minipaxtar.h"` wherever you need to interact with TAR archives.

```bash
# Example directory structure
my_project/
├── include/
│   └── minipaxtar.h
├── src/
│   ├── minipaxtar.c
│   └── main.c
└── Makefile
```

## Option B: STB-Style Single-Header

If you prefer a single-header workflow without compiling separate `.c` translation units:

1. Copy `minipaxtar.h` into your project.
2. In **exactly one** `.c` or `.cpp` file in your project, define `MINIPAXTAR_IMPLEMENTATION` before including the header to instantiate the code:

```c
/* main.c (or tar_implementation.c) */
#define MINIPAXTAR_IMPLEMENTATION
#include "minipaxtar.h"
```

3. In all other source files where you need the API, simply include the header normally without the `#define`:

```c
/* reader.c */
#include "minipaxtar.h"
```

## 2. CMake Integration

If your project uses CMake, `minipaxtar` provides clean target integration via `minipaxtar::minipaxtar`.


## Option A: FetchContent (Recommended for CMake)

Automatically downloads and builds `minipaxtar` at configure time without managing git submodules manually:

Add the following to your `CMakeLists.txt`:

```cmake
include(FetchContent)

FetchContent_Declare(
    minipaxtar
    GIT_REPOSITORY https://github.com/imvulpi/minipaxtar.git
    GIT_TAG v1.0.0 # Or target a specific git commit / tag
)

FetchContent_MakeAvailable(minipaxtar)

# Link minipaxtar to your executable or library
target_link_libraries(my_app PRIVATE minipaxtar::minipaxtar)
```

---

## Option B: Git Submodule or Subdirectory

If you keep `minipaxtar` inside your repository tree (e.g., under `third_party/minipaxtar`):

```cmake
# Add minipaxtar target
add_subdirectory(third_party/minipaxtar)

# Link to your target
target_link_libraries(my_app PRIVATE minipaxtar::minipaxtar)
```

## Option C: System Package (`find_package`)

If `minipaxtar` is installed globally on the host system or inside a toolchain sysroot:

```cmake
find_package(minipaxtar REQUIRED)

target_link_libraries(my_app PRIVATE minipaxtar::minipaxtar)
```

## Configuration Options

`minipaxtar` provides compile-time feature flags to shrink binary footprint or support extended POSIX features (such as PAX extended `atime`/`ctime` timestamps and special device nodes).

For manual builds, pass definitions via compiler flags (e.g., `-DMPTAR_SUPPORT_SPECIAL`).

For CMake users, these can be toggled using standard CMake options:

```bash
# Example: Disable special device nodes via CMake CLI
cmake -B build -DMPTAR_SUPPORT_SPECIAL=OFF
```

or in cmake:

```cmake
set(MPTAR_SUPPORT_SPECIAL OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/minipaxtar)
```

> 📖 **Full Reference:** For a complete list of feature flags and default values , see the [Configuration Flags Guide](/minipaxtar/guides/config-defines).

