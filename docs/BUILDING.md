# Building & Packaging — IMG Studio

IMG Studio is built with **CMake** (presets in `CMakePresets.json`) and **Ninja** on **WSL2 Debian** (or compatible Linux) using native **GCC/Clang** for Linux ASan/UBSan testing and **MinGW-w64** (`x86_64-w64-mingw32`) for portable **Windows x64** binaries.

---

## Prerequisites (WSL2 Debian / Ubuntu)

```bash
sudo apt update
sudo apt install -y \
  cmake ninja-build g++ git curl python3 zip \
  mingw-w64 g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix
```

Required tool minimums:
- CMake >= 3.24
- Ninja >= 1.10
- C++20 compiler (`g++` >= 11 on Linux, `x86_64-w64-mingw32-g++-posix` for Windows x64)
- Python 3.9+ (for `scripts/validate_locales.py`)

---

## Pinned Dependencies

All dependencies are pinned by exact version/tag and Git commit hash in [`cmake/Dependencies.cmake`](../cmake/Dependencies.cmake), or vendored under `third_party/` and `assets/fonts/`:

| Dependency | Version / Tag | Pinned Commit / Origin | Purpose |
| --- | --- | --- | --- |
| Dear ImGui | `v1.91.9b-docking` | `52fe0a05a7b1aa180a202bb24f0f2a049a9c1b7d` | Immediate-mode desktop GUI |
| GLFW | `3.4` | `70a9bb3881fe80fd483236e2b203cb451c6ecf40` | Win32 windowing & OpenGL context |
| nlohmann/json | `v3.11.3` | `9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03` | Settings, reports & JSON locales |
| Catch2 | `v3.8.1` | `56809e5282f104c5c8b570e7c2996cdc352d94f0` | Unit & integration test suite |
| `stb_image_write.h` | `1.16` era | `6b5799bae8a1fe3792691ef4b72ae26434c3e799` (`third_party/stb/`) | PNG / TGA / BMP image export |

Third-party source licenses are preserved in `third_party/` and packaged into `THIRD_PARTY_LICENSES.txt`.

---

## Build Presets

### 1. Linux Debug + ASan / UBSan (Headless Engine & App Tests)
`IMGSTUDIO_BUILD_GUI=OFF`, `IMGSTUDIO_BUILD_TESTS=ON`, `IMGSTUDIO_ENABLE_SANITIZERS=ON`:

```bash
cmake --preset linux-debug-tests
cmake --build --preset linux-debug-tests
ctest --preset linux-debug-tests --output-on-failure
```

### 2. Windows x64 Release (Cross-Compiled GUI + Test Runner)
Uses `cmake/toolchains/mingw-w64-x86_64.cmake`, statically links `libstdc++`, `libgcc`, and `winpthreads`, builds `IMGStudio.exe` (`-mwindows`), copies `assets/`, and builds `IMGStudioTests.exe`:

```bash
cmake --preset win64-release
cmake --build --preset win64-release
```

### 3. Windows x64 Debug (Cross-Compiled)
Includes debug symbols for diagnostics and verification:

```bash
cmake --preset win64-debug
cmake --build --preset win64-debug
```

Note for WSL2 users executing Windows `.exe` test runners directly from Bash: pass `< /dev/null` so Win32 console stdin does not block:

```bash
./build/win64-release/IMGStudioTests.exe < /dev/null
./build/win64-debug/IMGStudioTests.exe < /dev/null
```

---

## Compiler Warning Policy

All targets enforce:
```text
-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual -Wformat=2 -Werror=return-type
```
Code contributions must build cleanly with zero warnings across all presets.

---

## Packaging a Portable Windows x64 Release

```bash
./scripts/package_release.sh
```

This script:
1. Configures and builds `win64-release`.
2. Stages `dist/IMGStudio-<VERSION>-win64/` containing `IMGStudio.exe`, the complete `assets/` tree (fonts, icons, `assets/locales/*.json`), `README.md`, `README.es.md`, `LICENSE`, `THIRD_PARTY_LICENSES.txt`, and `RELEASE_NOTES.md`.
3. Archives `dist/IMGStudio-<VERSION>-win64.zip` and prints its SHA-256 digest without modifying prior packaged releases.
