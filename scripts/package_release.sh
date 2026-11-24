#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="1.0.0"
PKG_NAME="IMGStudio-${VERSION}-win64"
DIST_ROOT="${ROOT_DIR}/dist"
STAGE_DIR="${DIST_ROOT}/${PKG_NAME}"
BUILD_DIR="${ROOT_DIR}/build/win64-release"
ZIP_PATH="${DIST_ROOT}/${PKG_NAME}.zip"

echo "=== Building Windows x64 Release ==="
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
IMG Studio - Third-Party Software and Font Notices
===================================================

1. Dear ImGui (v1.91.9b-docking)
   License: MIT License
   Repository: https://github.com/ocornut/imgui

2. GLFW (3.4)
   License: zlib/libpng License
   Repository: https://github.com/glfw/glfw

3. nlohmann/json (v3.11.3)
   License: MIT License
   Repository: https://github.com/nlohmann/json

4. Catch2 (v3.8.1, used in automated test suite)
   License: Boost Software License 1.0
   Repository: https://github.com/catchorg/Catch2

5. Inter Font Family
   License: SIL Open Font License 1.1 (see assets/fonts/Inter-OFL.txt)

6. JetBrains Mono Font Family
   License: SIL Open Font License 1.1 (see assets/fonts/JetBrainsMono-OFL.txt)

7. Font Awesome 6 Free Solid
   License: SIL OFL 1.1 / CC BY 4.0 (see assets/fonts/FontAwesome-LICENSE.txt)
EOF

cat > "${STAGE_DIR}/RELEASE_NOTES.md" <<'EOF'
# IMG Studio 1.0.0 — Windows x64 Release

Native Windows x64 application for managing GTA San Andreas IMG VER2 archives.

## Contents

- `IMGStudio.exe` — Standalone Windows x64 executable (MinGW-w64 static C++/pthread runtime; uses standard Windows system DLLs only).
- `assets/` — Application fonts (Inter, JetBrains Mono, Font Awesome 6 Free) and icon assets. Keep this folder next to `IMGStudio.exe`.
- `README.md` — Product overview, quick start, and keyboard shortcuts.
- `LICENSE` — MIT License for IMG Studio.
- `THIRD_PARTY_LICENSES.txt` — Licenses for bundled libraries and fonts.

## System Requirements

- Windows 10 or Windows 11 (x64)
- OpenGL 3.3 Core Profile compatible graphics driver

## Safe Usage Notes

- Edits (`Import`, `Replace`, `Rename`, `Delete`) remain staged in memory until you click **Rebuild & Save** (`Ctrl+S`).
- Automatic pre-commit backups are enabled by default and stored in `%LOCALAPPDATA%\IMGStudio\backups\`.
- Always back up original game files before modifying stock archives.
EOF

echo "=== Creating ZIP archive: ${ZIP_PATH} ==="
(cd "${DIST_ROOT}" && zip -r -9 "${PKG_NAME}.zip" "${PKG_NAME}")

echo "=== Package SHA-256 ==="
sha256sum "${ZIP_PATH}"
ls -lh "${STAGE_DIR}/IMGStudio.exe" "${ZIP_PATH}"
