# Testing & Verification — IMG Studio

IMG Studio separates binary parsing/application logic from UI/OpenGL rendering so all non-GPU subsystems can be verified both under **Linux AddressSanitizer + UndefinedBehaviorSanitizer** and natively as **Windows x64** binaries.

---

## 1. Automated Test Suites (`IMGStudioTests`)

Implemented using **Catch2 v3.8.1** using only synthetic binary fixtures created in memory or temporary directories (zero copyrighted game assets):

| Test File | Scope |
| --- | --- |
| `tests/test_binary_io.cpp` | Little-endian I/O helpers, checked arithmetic, SHA-256 digests, VER2 filename validation, Windows path traversal protection |
| `tests/test_img_reader.cpp` | IMG VER2 parsing, empty archives, `archive_size` metadata, truncation, malformed headers, OOM guards |
| `tests/test_img_validator.cpp` | VER2 signature, directory overlap, sector out-of-bounds, filename checks, duplicates, 2 GiB warning |
| `tests/test_img_extractor.cpp` | Single/batch streaming extraction, staged external files |
| `tests/test_img_builder.cpp` | Round-trip rebuilds, multi-op staging, 2048-byte sector alignment |
| `tests/test_large_and_faults.cpp` | Large synthetic archives (2,500+ entries) and controlled fault injection |
| `tests/test_archive_document.cpp` | Duplicate policies (*Replace*, *Skip*, *AutoRename*), Undo/Redo stack, atomic commit + backup |
| `tests/test_backup_manager.cpp` | Manual/automatic backups, manifest indexing, SHA-256 integrity, restore verification |
| `tests/test_app_settings.cpp` | JSON settings round-trip and corrupt JSON fallback |
| `tests/test_entry_filter.cpp` | Filtering and sorting benchmarks over 10,000 entries |
| `tests/test_dff_parser.cpp` | `Clump`, `FrameList`, `Geometry`, `Material`, UVs, `BinMesh PLG` lists/strips, generated normals, truncation & native flags |
| `tests/test_txd_parser.cpp` | `DXT1/3/5`, `RASTER_8888/888/1555/555/565/4444/LUM8`, `PAL8/PAL4`, mipmaps, PNG/TGA/BMP export, unsupported platform reporting |
| `tests/test_dff_txd_integration.cpp` | In-memory DFF/TXD loading from IMG, multi-TXD lookup, missing texture diagnostics, 32-texture stress cycles |
| `tests/test_img_compare.cpp` | *Unchanged*, *Added*, *Removed*, *Modified*, *MetadataDiff*, *Ambiguous* states and JSON/TXT export |
| `tests/test_batch_processor.cpp` | Multi-archive concurrency, error isolation, *Validate*, *Backup*, *ExportAll*, *ImportFiles*, *ReplaceMatching*, cancellation |
| `tests/test_localization.cpp` | Locale loading (`en`, `es`), live switching, English fallback, missing-key diagnostics, `{arg}` substitution, corrupt JSON recovery, settings persistence, and 100% EN/ES key/placeholder parity |

---

## 2. Locale Validation Script

Before compiling or shipping locales, verify encoding (UTF-8 without BOM), key parity, placeholder matching, and source code usage:

```bash
python3 scripts/validate_locales.py
```

---

## 3. Running Tests on Linux & Windows

```bash
# Linux Debug + ASan + UBSan
cmake --preset linux-debug-tests
cmake --build --preset linux-debug-tests
ctest --preset linux-debug-tests --output-on-failure

# Windows x64 Release & Debug tests (from WSL2 pass < /dev/null)
cmake --preset win64-release && cmake --build --preset win64-release
./build/win64-release/IMGStudioTests.exe < /dev/null

cmake --preset win64-debug && cmake --build --preset win64-debug
./build/win64-debug/IMGStudioTests.exe < /dev/null
```

---

## 4. Non-Interactive GUI, OpenGL 3.3 & Persistence Smoke Tests (`IMGStudio.exe`)

`IMGStudio.exe` provides `--smoke-test` to verify the complete Windows GUI runtime without manual clicking:

```powershell
IMGStudio.exe --smoke-test --smoke-test-out smoke.txt
```

What `--smoke-test` validates:
- Dear ImGui + GLFW window creation and OpenGL 3.3 Core context initialization (`gl_error=0`).
- Shader compilation and offscreen `ViewportFbo` rendering (and optional `.img` / `.dff` / `.txd` load verification).
- Locale loading (`locale=`, `requested_locale=`) and translated string resolution (`sample_explorer=`, `sample_settings=`).

To verify localization persistence across application restarts inside an extracted release folder:

```powershell
# Run 1: select Spanish and save preference to isolated settings JSON
IMGStudio.exe --smoke-test --lang es --save-lang --settings-file test_settings.json --smoke-test-out r1.txt

# Run 2: restart WITHOUT --lang to confirm Spanish loads automatically from test_settings.json
IMGStudio.exe --smoke-test --settings-file test_settings.json --smoke-test-out r2.txt
```
