#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="2.0.0"
PKG_NAME="IMGStudio-${VERSION}-win64"
DIST_ROOT="${ROOT_DIR}/dist"
STAGE_DIR="${DIST_ROOT}/${PKG_NAME}"
BUILD_DIR="${ROOT_DIR}/build/win64-release"
ZIP_PATH="${DIST_ROOT}/${PKG_NAME}.zip"

echo "=== Building Windows x64 Release (${VERSION}) ==="
cmake --preset win64-release -S "${ROOT_DIR}"
cmake --build --preset win64-release

echo "=== Staging distributable directory: ${STAGE_DIR} ==="
rm -rf "${STAGE_DIR}" "${ZIP_PATH}"
mkdir -p "${STAGE_DIR}"

cp "${BUILD_DIR}/IMGStudio.exe" "${STAGE_DIR}/IMGStudio.exe"
cp -R "${ROOT_DIR}/assets" "${STAGE_DIR}/assets"
cp "${ROOT_DIR}/README.md" "${STAGE_DIR}/README.md"
cp "${ROOT_DIR}/LICENSE" "${STAGE_DIR}/LICENSE"

cat > "${STAGE_DIR}/THIRD_PARTY_LICENSES.txt" <<'EOF'
IMG Studio 2.0 - Third-Party Software and Font Notices
=======================================================

1. Dear ImGui (v1.91.9b-docking)
   License: MIT License
   Repository: https://github.com/ocornut/imgui

2. GLFW (3.4)
   License: zlib/libpng License
   Repository: https://github.com/glfw/glfw

3. nlohmann/json (v3.11.3)
   License: MIT License
   Repository: https://github.com/nlohmann/json

4. stb_image_write.h (commit 6b5799bae8a1fe3792691ef4b72ae26434c3e799)
   License: MIT License / Public Domain (see third_party/stb/LICENSE-STB.txt in source repo)
   Repository: https://github.com/nothings/stb

5. Catch2 (v3.8.1, used in automated test suite)
   License: Boost Software License 1.0
   Repository: https://github.com/catchorg/Catch2

6. Inter Font Family
   License: SIL Open Font License 1.1 (see assets/fonts/Inter-OFL.txt)

7. JetBrains Mono Font Family
   License: SIL Open Font License 1.1 (see assets/fonts/JetBrainsMono-OFL.txt)

8. Font Awesome 6 Free Solid
   License: SIL OFL 1.1 / CC BY 4.0 (see assets/fonts/FontAwesome-LICENSE.txt)
EOF

cat > "${STAGE_DIR}/RELEASE_NOTES.md" <<'EOF'
# IMG Studio 2.0.0 — Windows x64 Release

Native Windows x64 application for GTA San Andreas IMG VER2 archives, RenderWare DFF 3D models, TXD texture dictionaries, archive comparison, and batch processing.

## Contents

- `IMGStudio.exe` — Standalone Windows x64 executable (static C++/pthread runtime; standard Windows system DLLs only).
- `assets/` — Application fonts (Inter, JetBrains Mono, Font Awesome 6 Free) and icon assets. Keep this folder next to `IMGStudio.exe`.
- `README.md` — Feature overview, quick start, and documentation index.
- `LICENSE` — MIT License for IMG Studio.
- `THIRD_PARTY_LICENSES.txt` — Licenses for bundled libraries and fonts.

## System Requirements

- Windows 10 or Windows 11 (x64)
- OpenGL 3.3 Core Profile compatible graphics driver

## Safe Usage Notes

- Archive Explorer edits remain staged in memory until **Rebuild & Save** (`Ctrl+S`), with automatic pre-commit backups enabled by default.
- DFF Studio and TXD Studio inspect entries directly from opened IMG archives in memory without modifying source archives.
- IMG Compare is strictly read-only. Batch modifying operations (`Import`, `Replace Matching`) create automatic backups by default before atomic rebuilds.
EOF

echo "=== Creating ZIP archive: ${ZIP_PATH} ==="
(cd "${DIST_ROOT}" && zip -r -9 "${PKG_NAME}.zip" "${PKG_NAME}")

echo "=== Package SHA-256 ==="
sha256sum "${ZIP_PATH}"
ls -lh "${STAGE_DIR}/IMGStudio.exe" "${ZIP_PATH}"
