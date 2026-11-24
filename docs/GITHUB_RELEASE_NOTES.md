# IMG Studio 2.1.0 — Release Notes (Draft)

Native **Windows x64** (`Windows 10` / `Windows 11`) desktop application for **GTA San Andreas** `IMG VER2` archives, RenderWare `DFF` 3D models, `TXD` textures, archive comparison, and batch processing.

> **Package Note**: Extract the full zip preserving the `assets/` folder next to `IMGStudio.exe`. Verify the SHA-256 checksum after downloading.

---

## Highlights in v2.1.0

- **Complete Localization System (`en` & `es`)**:
  - Live runtime language switching inside **File -> Settings** without restarting the application.
  - Operating system locale auto-detection on first launch (`auto`), persistent language preference in `settings.json`, English fallback for missing keys, and Unicode-safe UTF-8 rendering.
  - All workspaces fully translated: Archive Explorer, Inspector Panel, DFF Studio, TXD Studio, IMG Compare, Batch Processing, Settings, Dialogs, and Toasts (429 verified keys).
  - Community translation support via `assets/locales/template.json`, `scripts/validate_locales.py`, and `docs/LOCALIZATION.md`.
- **Open-Source GitHub Preparation**:
  - International English `README.md` and Spanish `README.es.md`.
  - Complete contributor docs (`CONTRIBUTING.md`, `CODE_OF_CONDUCT.md`, `SECURITY.md`), technical guides in `docs/`, and GitHub issue/PR templates.
- **Full v2.0.0 Studio Capabilities Included**:
  - **DFF Studio** (OpenGL 3.3 Core 3D viewer, disk & direct in-memory `.img` loading, shading modes, automatic/manual TXD texture resolution).
  - **TXD Studio** (PC D3D9/D3D8 `DXT1/3/5`, `BGRA`, `PAL4/8`, `LUM8` decoders, RGBA/RGB/Alpha views, mipmaps, PNG/TGA/BMP export).
  - **IMG Compare** (read-only streaming SHA-256 diffing with JSON/TXT reports).
  - **Batch Processing** (1–4 concurrent isolated threads for batch validation, backups, export, import, and matching replacement).
  - **VER2 Archive Safety** (streaming I/O, atomic rebuilds, SHA-256 payload checks, automatic pre-commit backups, Undo/Redo).

---

## Included Files (`IMGStudio-2.1.0-win64.zip`)

- `IMGStudio.exe` — Self-contained Windows x64 executable (static C++ / winpthreads runtime)
- `assets/` — Fonts (`Inter`, `JetBrains Mono`, `Font Awesome 6 Free`), app icons, and `assets/locales/{en,es,template}.json`
- `README.md` / `README.es.md`
- `LICENSE` & `THIRD_PARTY_LICENSES.txt`
- `RELEASE_NOTES.md`

---

## Verification Summary (Before Publishing)

- Locale integrity check: `python3 scripts/validate_locales.py` (429 keys verified across `en.json`, `es.json`, `template.json`)
- Linux Debug ASan + UBSan automated test suite: `ctest --preset linux-debug-tests --output-on-failure`
- Windows x64 Release & Debug test suites: `IMGStudioTests.exe`
- Clean extracted Windows directory smoke test confirming OpenGL 3.3 initialization, live language switching (`en` <-> `es`), and restart persistence in `settings.json`

## Checksums

```text
SHA-256 (IMGStudio-2.1.0-win64.zip): <run sha256sum dist/IMGStudio-2.1.0-win64.zip before publishing>
```
