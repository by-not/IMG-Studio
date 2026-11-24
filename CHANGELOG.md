# Changelog

All notable changes to **IMG Studio** are documented in this file.

## [2.1.0] - 2026-11-24

### Added
- **Full UI Internationalization (`src/app/Localization.*`, `assets/locales/`)**:
  - Centralized `LocalizationManager` supporting UTF-8 external JSON locales (`en.json`, `es.json`, and contributor `template.json`).
  - Complete English (`en`) and Spanish (`es`) translations covering 429 UI keys across Main Window, Archive Explorer, Inspector Panel, DFF Studio, TXD Studio, IMG Compare, Batch Processing, Settings, Dialogs, Tooltips, and Toasts.
  - Live runtime language switching inside Settings without restarting the application (preserving open popups, inspector tabs, and active workspace via stable Dear ImGui `###` widget IDs).
  - First-launch OS language detection (`GetUserDefaultLocaleName` on Win32, locale environment variables on POSIX), persistent `"language"` setting in `settings.json`, English fallback for missing keys, missing-key diagnostics, and parameterized `{arg}` formatting.
  - Extended Dear ImGui font glyph ranges (`Latin Extended-A` and Unicode punctuation) for clean accented UTF-8 rendering.
  - CLI options `--lang <code>`, `--settings-file <path>`, and `--save-lang` for automated localization testing.
- **Localization Tooling & Tests**:
  - Added `scripts/validate_locales.py` to validate UTF-8 without BOM, duplicate JSON keys, missing/extra translation keys, placeholder parity, and C++ translation key references.
  - Added `tests/test_localization.cpp` covering locale discovery, live switching, fallback, diagnostics, placeholder substitution, malformed JSON recovery, settings persistence, and 100% EN/ES parity.
- **Open-Source GitHub Readiness**:
  - Added international English `README.md` and equivalent Spanish `README.es.md`.
  - Added `CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md`, GitHub issue/PR templates (`.github/`), technical docs under `docs/`, and release notes draft `docs/GITHUB_RELEASE_NOTES.md`.

### Changed
- Bumped application version metadata to `2.1.0` (`CMakeLists.txt`, Windows `resources.rc`, `app.manifest`, and `scripts/package_release.sh`).

---

## [2.0.0] - 2026-11-24

Stable checkpoint preserved on `main` (annotated tag `v2.0.0`).

### Added
- **DFF Studio**: Modular RenderWare DFF parser (`src/core/rw/DffParser.*`) and real-time OpenGL 3.3 Core 3D viewer (`src/render/DffRenderer.*`, `src/ui/DffStudioView.*`) supporting external files and direct in-memory `.img` entries, shading modes (*Textured*, *Solid*, *Wireframe*, *Materials*), orbit camera, grid/axes, lighting controls, and geometry/material diagnostics.
- **TXD Studio**: RenderWare TXD parser and pure C++20 decoders (`DXT1`, `DXT3`, `DXT5`, `RASTER_8888`, `RASTER_888`, `1555`, `555`, `565`, `4444`, `LUM8`, `PAL8`, `PAL4`), thumbnail grid, RGBA/RGB/Alpha channel inspection, mipmap selector, and PNG/TGA/BMP export (`src/core/rw/ImageExporter.*` via `stb_image_write.h`).
- **DFF + TXD Integration**: `AssetLoader` resolves texture dictionaries automatically from the same `.img` archive (`<stem>.txd`, `vehicle.txd`, `generic.txd`) or manually attached `.txd` files without temporary disk extraction.
- **IMG Compare**: Streaming SHA-256 archive comparison (`src/core/ImgCompare.*`, `src/app/CompareService.*`, `src/ui/ImgCompareView.*`) with detailed classification and JSON/TXT report export.
- **Batch Processing**: Isolated multi-threaded batch worker queue (`src/app/BatchProcessor.*`, `src/ui/BatchView.*`) for validation, manual backups, full extraction, import, and matching replacement with JSON summary export.
- Technical documentation (`DFF_FORMAT.md`, `TXD_FORMAT.md`) and expanded Catch2 test coverage.

---

## [1.0.0] - 2026-11-24

Initial stable release (preserved on `main`, annotated tag `v1.0.0`).

### Added
- Independent GTA San Andreas **IMG VER2** archive parser, validator, streaming extractor, and atomic SHA-256-verified builder.
- Virtualized Archive Explorer UI with staged edits, Undo/Redo history, duplicate conflict handling, and automatic pre-rebuild backups.
