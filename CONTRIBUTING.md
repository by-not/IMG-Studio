# Contributing to IMG Studio

Thank you for your interest in contributing to **IMG Studio**! IMG Studio is an independent, modern C++20 desktop application for **GTA San Andreas** IMG VER2 archives, RenderWare DFF models, TXD textures, archive comparison, and batch operations.

Before contributing, please read the [Code of Conduct](CODE_OF_CONDUCT.md).

---

## Strict Copyright & Asset Policy

**Never submit copyrighted commercial game assets in issues, pull requests, tests, or documentation.**

- Do **not** upload or commit original GTA San Andreas `.img`, `.dff`, `.txd`, audio, or binary files from a commercial installation.
- Automated tests must construct synthetic binary fixtures in memory or temporary directories.
- External reverse-engineering of proprietary tools or reuse of non-free code is strictly prohibited. All archive and RenderWare parsers in IMG Studio must be cleanly implemented from public format documentation and verifiable binary behavior.

---

## Ways to Contribute

1. **Bug Reports**: Use the [Bug Report template](.github/ISSUE_TEMPLATE/bug_report.yml) and include exact steps to reproduce, IMG Studio version, GPU/Windows version, and validation/error logs.
2. **Feature Requests**: Use the [Feature Request template](.github/ISSUE_TEMPLATE/feature_request.yml). Please note that the current scope intentionally targets **GTA San Andreas IMG VER2**; multi-game adapters should be discussed first.
3. **Translations / Localization**: Add new languages or improve existing translations using [docs/LOCALIZATION.md](docs/LOCALIZATION.md) and the [Translation Contribution template](.github/ISSUE_TEMPLATE/translation_contribution.yml).
4. **Code Contributions**: Bug fixes, unit tests, parser edge cases, rendering improvements, and UI polish.

---

## Development Setup & Architecture

See [docs/BUILDING.md](docs/BUILDING.md) for toolchain prerequisites (CMake, Ninja, GCC/Clang with ASan/UBSan on Linux, MinGW-w64 cross-compilation for Windows x64) and [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for layer boundaries:

- `src/core/` — Pure C++20 archive, validation, RenderWare DFF/TXD, comparison, and image export logic (no UI or OS dependencies).
- `src/platform/` — OS file dialogs, path/UTF-8 conversion, Windows locale detection, and file explorer integration.
- `src/app/` — Archive document model, undo/redo history, backup manager, batch processor, localization manager, and async workers.
- `src/ui/` + `src/render/` — Dear ImGui panels, OpenGL 3.3 renderer, and localization bindings.

Keep these layer boundaries clean: never include ImGui or OpenGL headers inside `core/` or `app/`.

---

## Coding Standards

- Standard: **C++20** (`CMAKE_CXX_STANDARD 20`, extensions off).
- Compiler warnings are strict (`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual -Wformat=2 -Werror=return-type`). All builds must compile warning-free.
- All user-visible UI text **must** go through `app::tr()`, `app::tr_c()`, or `app::trf()` using stable translation keys in `assets/locales/en.json` and matching updates to `es.json` and `template.json`.
- Widgets whose visible label changes when switching languages live (modals, menus, tabs, workspace buttons) **must** carry stable Dear ImGui `###...` IDs so live language switching does not reset UI state.
- Preserve binary format identifiers (`VER2`, `.dff`, `.txd`, `DXT1`..`DXT5`, etc.) untranslated.

---

## Verification Checklist Before Opening a Pull Request

Run all checks locally before submitting a PR:

```bash
# 1. Validate all locale files and source translation keys
python3 scripts/validate_locales.py

# 2. Build and run Linux Debug tests under ASan + UBSan
cmake --preset linux-debug-tests
cmake --build --preset linux-debug-tests
ctest --preset linux-debug-tests --output-on-failure

# 3. Cross-compile Windows x64 Release and Debug targets
cmake --preset win64-release && cmake --build --preset win64-release
cmake --preset win64-debug && cmake --build --preset win64-debug
```

If you tested on Windows directly (or via WSL2), run `IMGStudioTests.exe` and the smoke test suite described in [docs/TESTING.md](docs/TESTING.md).

---

## Pull Request Process

1. Fork the repository and branch from `develop` (not `main` or release tags).
2. Keep pull requests focused on a single bug, feature, documentation improvement, or language file.
3. Update relevant documentation in `docs/` or `CHANGELOG.md` when behavior changes.
4. Fill out the [Pull Request Template](.github/PULL_REQUEST_TEMPLATE.md).
