# Architecture — IMG Studio

IMG Studio uses a strict 4-layer C++20 architecture designed for memory safety, testability under Linux AddressSanitizer/UBSan, zero coupling between binary format parsers and Dear ImGui/OpenGL, and clean Windows x64 deployment.

---

## System Layers

```
+-----------------------------------------------------------------------+
|                              src/ui/                                  |
|  AppWindow, MainView, ExplorerTable, InspectorPanel, Dialogs, Theme,  |
|  DffStudioView, TxdStudioView, ImgCompareView, BatchView              |
+-------------------------------+---------------------------------------+
                                | uses
          +---------------------+---------------------+
          v                                           v
+-----------------------------------+   +-------------------------------+
|            src/app/               |   |         src/render/           |
|  ArchiveController, ArchiveDoc,   |   |  GlLoader, Math3D, GpuTexture |
|  OperationHistory, BackupManager, |   |  ViewportFbo, DffRenderer     |
|  EntryFilter, AppSettings,        |   |  (linked only in IMGStudio    |
|  AssetLoader, CompareService,     |   |   desktop GUI executable)     |
|  BatchProcessor, Localization     |   +-------------------------------+
+-----------------+-----------------+
                  | uses
      +-----------+-----------+
      v                       v
+---------------------------+ +---------------------------+
|         src/core/         | |      src/platform/        |
|  IMG VER2 + ImgCompare +  | |  platform_win32.cpp       |
|  Sha256 + src/core/rw/    | |  platform_posix.cpp       |
|  (RwStream, DffParser,    | |  COM dialogs, DWM dark,   |
|   TxdParser, TxdDecoder,  | |  locale detection, atomic |
|   ImageExporter)          | |  ReplaceFileW replace     |
+---------------------------+ +---------------------------+
```

---

## Layer Responsibilities

### 1. `src/core/` (`imgstudio_core`)
Pure C++20 binary engines with **zero UI, OpenGL, or OS-GUI dependencies**:
- **`ImgReader` / `ImgValidator` / `ImgExtractor` / `ImgBuilder` / `ImgFilename` / `Sha256`**:
  - Streaming I/O over `std::ifstream` / `std::ofstream` using bounded chunk transfers (`64 KiB` / `256 KiB` copy buffers).
  - Never assumes whole archives fit in RAM; supports huge multi-thousand-entry archives safely.
  - Atomic rebuild pipeline writes a temporary `.tmp` archive, validates streaming **SHA-256** checksums, and replaces atomically via `src/platform/`.
- **`ImgCompare`**:
  - Strictly read-only archive comparison comparing directory metadata and streaming payload **SHA-256** digests across two `IMG VER2` files.
- **`src/core/rw/` (`RwStream`, `DffParser`, `TxdParser`, `TxdDecoder`, `ImageExporter`)**:
  - Independent RenderWare chunk parser (`Clump`, `FrameList`, `GeometryList`, `Geometry`, `MaterialList`, `Material`, `Texture`, `BinMesh PLG`, `Atomic`, `TexDictionary`, `TextureNative`).
  - Pure C++20 texture decoders (`DXT1`, `DXT3`, `DXT5`, `RASTER_8888/888/1555/555/565/4444/LUM8`, `PAL8`, `PAL4`) producing `RGBA8` buffers.
  - Exports decoded mipmaps to PNG, TGA, or BMP via `stb_image_write.h`.

### 2. `src/platform/` (`imgstudio_platform`)
Isolated OS abstractions (`Platform.hpp`, `FileDialogs.hpp`) with separate Win32 (`platform_win32.cpp`, `FileDialogs_win32.cpp`) and POSIX test stubs (`platform_posix.cpp`):
- Native Windows IFileOpenDialog / IFileSaveDialog / Folder Picker and Dark Titlebar (DWM).
- UTF-8 <-> UTF-16 (`std::filesystem::path`) conversion.
- Atomic replacement (`ReplaceFileW` / fallback move).
- OS language detection (`detect_system_language()` via `GetUserDefaultLocaleName` on Win32 and `LC_ALL` / `LC_MESSAGES` / `LANG` on POSIX).
- Asset path resolution (`resolve_asset_path`) checking next to the executable first and falling back cleanly for development/test runners.

### 3. `src/app/` (`imgstudio_app`)
Application state, background tasks, persistence, and localization:
- **`ArchiveDocument` & `OperationHistory`**: In-memory staged changes (Import, Replace, Rename, Delete) with full Undo/Redo before committing disk rebuilds.
- **`BackupManager`**: Automatic pre-commit and manual archive backups with SHA-256 index tracking and recovery instructions.
- **`AssetLoader`**: Loads DFF/TXD entries directly from open `.img` archives in memory or disk and resolves texture references automatically (`<stem>.txd`, `vehicle.txd`, `generic.txd`, plus manually attached dictionaries).
- **`BatchProcessor` & `CompareService`**: Concurrent background job runners isolating per-worker `ArchiveDocument` instances and serializing backup manifest writes.
- **`AppSettings` & `LocalizationManager`**: Persistent JSON configuration (`settings.json`) and live UTF-8 translation management (`en.json`, `es.json`, extensible via `assets/locales/`).

### 4. `src/render/` and `src/ui/` (`IMGStudio`)
- **OpenGL 3.3 Core Renderer (`GlLoader`, `GpuTexture`, `ViewportFbo`, `DffRenderer`, `Math3D`)**: RAII GPU resource management decoupled from binary parsing and bound strictly to the GUI target so core/app unit tests run headlessly under ASan/UBSan.
- **Dear ImGui Panels (`AppWindow`, `MainView`, `ExplorerTable`, `InspectorPanel`, `Dialogs`, `DffStudioView`, `TxdStudioView`, `ImgCompareView`, `BatchView`)**:
  - Uses `ImGuiListClipper` for virtualized rendering of 16,000+ entries.
  - Uses `ImFontGlyphRangesBuilder` (`Latin Extended-A` + Unicode punctuation) for clean UTF-8 international text rendering.
  - Employs stable `###...` ImGui widget IDs on modals, tabs, menus, and workspace buttons so live language switching does not interrupt user workflows.

---

## Localization Architecture

1. At startup (`AppWindow::run`), `Localization::instance()` scans `assets/locales/*.json` (ignoring `template.json`), loads optional `builtin` fallbacks, and resolves `"auto"` via `platform::detect_system_language()`.
2. CLI overrides (`--lang <code>`, `--settings-file <path>`, `--save-lang`) allow reproducible headless verification.
3. All user-facing lookups use `app::tr(key)`, `app::tr_c(key)`, or `app::trf(key, {{...}})`:
   - Active locale -> English fallback -> bracketed missing-key sentinel (`[key]`) + `missing_keys()` diagnostic capture.
4. Selecting a new language in **File -> Settings** invokes `set_language(...)` immediately across the next ImGui frame without restarting.

---

## Future Extension Strategy (Multi-Game & New Formats)

Although IMG Studio **2.1.0** specifically targets **GTA San Andreas (`IMG VER2`)**, the 4-layer separation makes future support straightforward without destabilizing existing VER2 behavior:

- **Abstract Archive Backend**: Future `IMG VER1` (`.dir` + `.img` from GTA III / Vice City) or PS2/Xbox containers can implement an independent reader/writer behind `ArchiveDocument` while keeping `ImgReader`/`ImgBuilder` VER2 logic isolated and regression-tested.
- **Pluggable RenderWare Platform Decoders**: Additional RenderWare console raster decoders or `Skin PLG` chunks plug into `src/core/rw/` cleanly as isolated functions with dedicated synthetic Catch2 tests.
- **Drop-In Language Packs**: Adding new community translations requires only placing `<lang>.json` in `assets/locales/` and passing `scripts/validate_locales.py`—zero C++ recompilation required.
