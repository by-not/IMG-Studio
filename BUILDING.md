# Compilacion y Empaquetado (BUILDING)

IMG Studio 2.0 mantiene el flujo de compilacion cruzada desde **WSL2 Debian** hacia **Windows x64** con CMake, Ninja y MinGW-w64.

## Requisitos en WSL2 Debian

```bash
sudo apt update
sudo apt install -y \
  cmake ninja-build g++ git curl python3 zip \
  mingw-w64 g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix
```

## Dependencias fijadas

| Dependencia | Version / Commit | Ubicacion / Metodo |
| --- | --- | --- |
| Dear ImGui | `v1.91.9b-docking` (`52fe0a05a7b1aa180a202bb24f0f2a049a9c1b7d`) | CMake FetchContent (`cmake/Dependencies.cmake`) |
| GLFW | `3.4` (`70a9bb3881fe80fd483236e2b203cb451c6ecf40`) | CMake FetchContent (`cmake/Dependencies.cmake`) |
| nlohmann/json | `v3.11.3` (`9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03`) | CMake FetchContent (`cmake/Dependencies.cmake`) |
| Catch2 | `v3.8.1` (`56809e5282f104c5c8b570e7c2996cdc352d94f0`) | CMake FetchContent (`cmake/Dependencies.cmake`) |
| stb_image_write | `6b5799bae8a1fe3792691ef4b72ae26434c3e799` | Vendored en `third_party/stb/` (`LICENSE-STB.txt`) |

## Compilacion y pruebas

```bash
# Linux ASan + UBSan (core + platform + app + tests)
cmake --preset linux-debug-tests
cmake --build --preset linux-debug-tests
ctest --preset linux-debug-tests --output-on-failure

# Windows x64 Release y Debug
cmake --preset win64-release
cmake --build --preset win64-release
./build/win64-release/IMGStudioTests.exe < /dev/null

cmake --preset win64-debug
cmake --build --preset win64-debug
./build/win64-debug/IMGStudioTests.exe < /dev/null
```

## Paquete distribuible 2.0.0

```bash
./scripts/package_release.sh
```

Genera `dist/IMGStudio-2.0.0-win64/` y `dist/IMGStudio-2.0.0-win64.zip` manteniendo intactos los artefactos previos de `1.0.0`.
