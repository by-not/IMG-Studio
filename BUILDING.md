# Compilacion y Empaquetado (BUILDING)

Este documento describe como compilar, probar y empaquetar IMG Studio desde **WSL2 Debian** hacia **Windows x64** usando CMake, Ninja y MinGW-w64.

## Requisitos en WSL2 Debian

Paquetes necesarios en Debian:

```bash
sudo apt update
sudo apt install -y \
  cmake ninja-build g++ git curl python3 zip \
  mingw-w64 g++-mingw-w64-x86-64-posix gcc-mingw-w64-x86-64-posix
```

Nota: el toolchain utiliza `x86_64-w64-mingw32-g++-posix` y `x86_64-w64-mingw32-gcc-posix` (modelo de hilos POSIX necesario para `std::thread` y `std::mutex` en C++20).

## Dependencias de terceros fijadas

Las dependencias C++ se gestionan automaticamente con CMake `FetchContent` en `cmake/Dependencies.cmake`:

| Dependencia | Tag fijado | Commit | Uso |
| --- | --- | --- | --- |
| Dear ImGui | `v1.91.9b-docking` | `52fe0a05a7b1aa180a202bb24f0f2a049a9c1b7d` | Interfaz grafica + backend GLFW/OpenGL 3.3 |
| GLFW | `3.4` | `70a9bb3881fe80fd483236e2b203cb451c6ecf40` | Ventana Win32, contexto OpenGL 3.3, drag & drop |
| nlohmann/json | `v3.11.3` | `9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03` | Configuracion, indice de respaldos y reportes |
| Catch2 | `v3.8.1` | `56809e5282f104c5c8b570e7c2996cdc352d94f0` | Suite de pruebas automatizadas |

Los assets locales (icono `.ico` y fuentes `Inter`, `JetBrains Mono`, `Font Awesome 6 Free Solid` con sus licencias OFL) residen en `assets/` y pueden regenerarse con:

```bash
python3 scripts/generate_icon.py
python3 scripts/fetch_fonts.py
```

## Presets de CMake disponibles

Definidos en `CMakePresets.json`:

1. `win64-release` — Compilacion cruzada Windows x64 Release (`IMGStudio.exe` + `IMGStudioTests.exe`).
2. `win64-debug` — Compilacion cruzada Windows x64 Debug.
3. `linux-debug-tests` — Compilacion nativa Linux Debug con `-fsanitize=address,undefined` para pruebas de `core`, `platform` y `app`.
4. `linux-release-tests` — Compilacion nativa Linux Release para pruebas.

## Comandos de compilacion

### Windows x64 Release

```bash
cmake --preset win64-release
cmake --build --preset win64-release
```

Los artefactos se generan en `build/win64-release/IMGStudio.exe`, `build/win64-release/IMGStudioTests.exe` y `build/win64-release/assets/`.

### Windows x64 Debug

```bash
cmake --preset win64-debug
cmake --build --preset win64-debug
```

### Pruebas Linux con ASan + UBSan

```bash
cmake --preset linux-debug-tests
cmake --build --preset linux-debug-tests
ctest --preset linux-debug-tests --output-on-failure
```

## Generacion del paquete distribuible

```bash
./scripts/package_release.sh
```

Genera la carpeta `dist/IMGStudio-1.0.0-win64/` y el archivo comprimido `dist/IMGStudio-1.0.0-win64.zip`.
