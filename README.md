# IMG Studio

**IMG Studio** is an independent, native **Windows x64** desktop studio for **GTA San Andreas** **IMG VER2** archives, **RenderWare DFF** 3D models, and **RenderWare TXD** texture dictionaries.

Read this documentation in: **English** | [Español](README.es.md)

---

## Overview

Built from scratch in **C++20** with **Dear ImGui**, **GLFW**, and **OpenGL 3.3 Core**, IMG Studio provides a modern dark-themed workflow designed to replace brittle legacy tools with streaming I/O, atomic archive rebuilds, integrity verification, automatic backups, integrated 3D/texture inspection, and live internationalization.

IMG Studio contains zero proprietary third-party archive code and does not reverse-engineer closed-source modding utilities. All binary parsers and writers (`VER2`, RenderWare `.dff`, `.txd`, DXT decoders) are independently implemented from public format specifications and verifiable binary behavior.

> **Current Game Scope**: IMG Studio **2.1.0** specifically targets **GTA San Andreas (PC)** (`IMG VER2`, RenderWare `3.6.0.3` / `0x1803FFFF` and documented SA variants). Legacy GTA III / Vice City `IMG VER1` (`.dir` + `.img`) and console platforms are currently out of scope.

---

## Feature Matrix

### 1. Archive Explorer (IMG VER2)
- Open large `IMG VER2` archives (tested up to real 16,316+ entry GTA San Andreas archives via streaming I/O and virtualized table rendering).
- Create empty `VER2` archives.
- Inspect structural integrity (`Quick Check` and `Full Read Verification`) and export machine-readable JSON validation reports.
- Search by filename (`Ctrl+F`), filter by extension or asset category (`.dff`, `.txd`, `.col`, `.ifp`, `.ipl/.ide`), sort columns, and multi-select entries.
- Stage **Import**, **Replace**, **Rename** (validated against the 23-char ASCII `VER2` limit), and **Delete** operations in memory with **Undo / Redo** before touching disk.
- Duplicate handling policy (*Ask*, *Replace Existing*, *Skip*, *Auto-Rename*).
- Atomic **Rebuild & Save** (`Ctrl+S`): writes to a temporary file, computes streaming **SHA-256** digests to verify every payload, creates an automatic backup (configurable), and atomically replaces the target archive.

### 2. DFF Studio (Integrated 3D Viewer)
- Load `.dff` models directly from disk or double-click/context-menu inside any open `.img` archive without extracting temporary files.
- Shading modes: **Textured**, **Solid**, **Wireframe**, and **Material Colors**.
- Orbit camera (orbit, pan, zoom, fit to bounds, reset), configurable viewport background, XY reference grid, axes, backface culling, and interactive lighting controls (azimuth, elevation, ambient).
- Model inspector displaying RenderWare version, geometries, materials, vertex/triangle counts, bounding dimensions, texture reference resolution status (`<stem>.txd`, `vehicle.txd`, `generic.txd`, or manually loaded `.txd`), and parser diagnostics.

### 3. TXD Studio (Texture Inspector & Exporter)
- Open `.txd` dictionaries from disk or directly from an open `.img` archive.
- Pure C++20 decoders for PC D3D9/D3D8 raster formats: `DXT1`, `DXT3`, `DXT5`, `RASTER_8888`, `RASTER_888`, `1555`, `555`, `565`, `4444`, `LUM8`, `PAL8`, and `PAL4`.
- Searchable thumbnail grid with dimension filters and disk/name/size sorting.
- Channel switcher (`RGBA`, `RGB`, `Alpha`), backdrop switcher (`Checker`, `Dark`, `Light`), texture filtering (`Linear` / `Nearest`), mipmap selector, fit/zoom controls, and batch texture export to **PNG**, **TGA**, and **BMP**.

### 4. IMG Compare
- Strictly read-only streaming **SHA-256** comparison between two `IMG VER2` archives without loading full archives into memory.
- Classifies entries into **Unchanged**, **Added**, **Removed**, **Modified**, **Metadata Diff**, and **Ambiguous** (duplicate-name entries).
- Export structured comparison reports to **JSON** and **TXT**.

### 5. Batch Processing
- Concurrent isolated worker queue (1 to 4 threads) operating across multiple `.img` archives.
- Supported operations: **Validate Archives**, **Create Manual Backups**, **Export All Entries**, **Import Files Into Archives**, and **Replace Matching Entries**.
- Automatic pre-rebuild backups enabled by default on modifying batch jobs and **JSON** summary report export.

### 6. Internationalization (New in 2.1.0)
- Live runtime language switching in **File -> Settings** (`en` and `es` included out of the box) without restarting the application or losing open modal/tab state.
- Operating system locale auto-detection on first launch (`auto` mode on Windows via `GetUserDefaultLocaleName` and POSIX via locale env vars).
- Persistent language preference in `settings.json`, English fallback for missing keys, missing-key diagnostics, and parameterized UTF-8 translations.
- CLI flags `--lang <code>`, `--settings-file <path>`, and `--save-lang` for reproducible testing and scripting.

---

## Windows Installation

1. Download the release package `IMGStudio-2.1.0-win64.zip` from Releases (once published).
2. Extract the entire archive to a folder of your choice. **Keep the `assets/` directory next to `IMGStudio.exe`** (it contains bundled fonts and `assets/locales/*.json`).
3. Run `IMGStudio.exe`.

### System Requirements
- **OS**: Windows 10 or Windows 11 (x64)
- **Graphics**: OpenGL 3.3 Core Profile compatible GPU/driver
- **Runtime**: Self-contained executable (statically linked C++ / MinGW winpthreads runtime; only standard Windows system DLLs required)

### Optional Command-Line Usage
```text
IMGStudio.exe [archive.img]
IMGStudio.exe --dff model.dff [--txd textures.txd]
IMGStudio.exe --txd textures.txd
IMGStudio.exe --lang es
```

---

## Building from Source

IMG Studio is developed and cross-compiled on **WSL2 Debian** targeting **Windows x64** (`x86_64-w64-mingw32`), with a native **Linux Debug ASan/UBSan** preset for memory-safety testing.

```bash
# Locale integrity check
python3 scripts/validate_locales.py

# Linux Debug + ASan/UBSan test suite
cmake --preset linux-debug-tests
cmake --build --preset linux-debug-tests
ctest --preset linux-debug-tests --output-on-failure

# Windows x64 Release build
cmake --preset win64-release
cmake --build --preset win64-release

# Create portable Windows x64 distributable zip
./scripts/package_release.sh
```

See [docs/BUILDING.md](docs/BUILDING.md) for full toolchain prerequisites and pinned dependency details.

---

## Dependencies

All third-party libraries and fonts are pinned in [`cmake/Dependencies.cmake`](cmake/Dependencies.cmake) or vendored with licenses in `third_party/` and `assets/fonts/`:

| Component | Version / Commit | Purpose | License |
| --- | --- | --- | --- |
| [Dear ImGui](https://github.com/ocornut/imgui) | `v1.91.9b-docking` (`52fe0a05...`) | Desktop GUI framework | MIT |
| [GLFW](https://github.com/glfw/glfw) | `3.4` (`70a9bb38...`) | Windowing & OpenGL context | zlib/libpng |
| [nlohmann/json](https://github.com/nlohmann/json) | `v3.11.3` (`9cca280a...`) | Settings, reports & locale JSON | MIT |
| [stb_image_write.h](third_party/stb/stb_image_write.h) | `6b5799ba...` | PNG / TGA / BMP texture export | MIT / Public Domain |
| [Catch2](https://github.com/catchorg/Catch2) | `v3.8.1` (`56809e52...`) | Automated unit/integration tests | BSL-1.0 |
| Inter & JetBrains Mono | `v4.0` / `v2.304` | UI & monospace typography | SIL OFL 1.1 |
| Font Awesome 6 Free Solid | `6.7.2` | UI vector icons | SIL OFL 1.1 / CC BY 4.0 |

---

## Current Limitations

- **Format scope**: Supports GTA San Andreas **IMG VER2** (`VER2` header). `IMG VER1` (`.dir`/`.img` from GTA III / Vice City) is not supported yet.
- **RenderWare scope**: Complex non-standard or console-exclusive R* raster formats, bone skinning (`Skin PLG`), animations (`.ifp`), and `.col` 3D preview are not implemented yet. Unsupported raster formats are reported safely in TXD Studio without crashing.
- **Read-only TXD / DFF inspection**: TXD Studio exports textures (`PNG`/`TGA`/`BMP`) and inspects metadata; direct in-place `.txd` repacking is planned for a future release.
- **UI Scale font reload**: Adjusting `UI Scale` in Settings applies after restarting the application so font atlases rebuild cleanly.

---

## Documentation

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — 4-layer architecture, localization subsystem, and future game-adapter extension model
- [docs/FORMATS.md](docs/FORMATS.md) — IMG VER2, DFF, and TXD technical reference and boundaries
- [docs/BUILDING.md](docs/BUILDING.md) — Build prerequisites, cross-compilation, and release packaging
- [docs/TESTING.md](docs/TESTING.md) — Automated test suites, sanitizers, locale validation, and Windows smoke tests
- [docs/LOCALIZATION.md](docs/LOCALIZATION.md) — Guide for contributing new languages
- [CHANGELOG.md](CHANGELOG.md) — Release history (`1.0.0`, `2.0.0`, `2.1.0`)
- [ROADMAP.md](ROADMAP.md) — Post-2.1 milestones

---

## Contributing & Community

Contributions, bug reports, and new translations are welcome. Please read:

- [CONTRIBUTING.md](CONTRIBUTING.md)
- [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)
- [SECURITY.md](SECURITY.md)

**Do not upload or commit copyrighted GTA San Andreas game archives or extracted game resources to this repository.**

---

## License Notice

The repository currently includes a [`LICENSE`](LICENSE) file (`MIT License`). Before publishing public GitHub releases or packages, verify that the project owner has explicitly confirmed the final open-source license selection.
