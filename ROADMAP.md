# Roadmap — IMG Studio

## Version 1.0.0 (Preserved at tag `v1.0.0` on `main`)
- Independent GTA San Andreas `IMG VER2` streaming parser, validator, extractor, and atomic SHA-256-verified archive rebuilder.
- Virtualized desktop Archive Explorer with staged edits, Undo/Redo history, and automatic backups.

## Version 2.0.0 (Preserved at tag `v2.0.0` on `main`)
- **DFF Studio**: OpenGL 3.3 Core 3D viewer loaded from disk or `.img` entries directly in memory.
- **TXD Studio**: PC D3D9/D3D8 RenderWare texture decoder (`DXT1/3/5`, `BGRA`, `PAL4/8`, `LUM8`), mipmap/alpha inspector, and PNG/TGA/BMP export.
- **DFF + TXD Integration**: Automatic texture resolution from open `.img` archives or manually attached `.txd` dictionaries.
- **IMG Compare**: Streaming SHA-256 archive diffing with JSON/TXT export.
- **Batch Processing**: Concurrent isolated queue for archive validation, backup, export, import, and replacement.

## Version 2.1.0 (Current development on `develop`)
- **Internationalization (`i18n`)**: Centralized `LocalizationManager`, UTF-8 JSON locale files (`en`, `es`, `template.json`), live runtime switching, OS language detection, persistent settings, validation tooling (`scripts/validate_locales.py`), and Catch2 tests.
- **Open-Source Readiness**: International English & Spanish READMEs, contributor guidelines, Code of Conduct, Security Policy, GitHub issue/PR templates, and consolidated `docs/` engineering guides.

## Future Planned Milestones (Post-2.1)
- **Additional Community Localizations**: Additional languages contributed via `docs/LOCALIZATION.md` (no code changes required to drop in new JSON locales).
- **Direct TXD Texture Editing**: Replacing, adding, and repacking textures directly inside `.txd` dictionaries.
- **Extended Asset Support**: Bone/skinning preview (`Skin PLG`), `.ifp` animation inspection, and `.col` collision wireframe preview.
- **Multi-Game Archive Adapters**: Optional future adapters (e.g., legacy `IMG VER1` `.dir`/`.img` reader/converter) behind an abstract archive backend interface without altering the verified GTA San Andreas `VER2` engine.
