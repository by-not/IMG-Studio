# IMG Studio

**IMG Studio** es una aplicacion de escritorio nativa para **Windows x64** diseñada para gestionar archivos **IMG VER2** de **GTA San Andreas**, visualizar modelos 3D **RenderWare DFF**, inspeccionar y exportar diccionarios de texturas **TXD**, comparar archivos IMG y procesar lotes.

Leer en: [English](README.md) | **Español**

---

## Descripcion general

Desarrollada desde cero en **C++20** con **Dear ImGui**, **GLFW** y **OpenGL 3.3 Core**, IMG Studio ofrece un flujo de trabajo moderno en modo oscuro orientado a reemplazar utilidades heredadas fragiles mediante lectura/escritura por streaming, reconstruccion atomica, verificacion SHA-256, respaldos automaticos e internacionalizacion en vivo.

IMG Studio no reutiliza codigo propietario ni aplica ingenieria inversa a herramientas cerradas heredadas. Todos sus analizadores y escritores (`VER2`, `.dff`, `.txd`, decodificadores DXT) estan implementados de forma independiente a partir de especificaciones publicas del formato.

> **Alcance actual**: IMG Studio **2.1.0** soporta especificamente **GTA San Andreas (PC)** (`IMG VER2`, RenderWare `3.6.0.3` / `0x1803FFFF` y variantes documentadas de SA). Las variantes heredadas `IMG VER1` (`.dir` + `.img` de GTA III / Vice City) y formatos exclusivos de consola estan fuera del alcance actual.

---

## Funcionalidades principales

### 1. Explorador IMG (IMG VER2)
- Apertura rapida de archivos `IMG VER2` grandes (verificado manualmente con archivos reales de GTA San Andreas de 16,316 entradas mediante streaming y tabla virtualizada).
- Creacion de archivos `VER2` vacios.
- Validacion estructural (`Quick Check` / `Verificacion rapida` y `Full Read Verification` / `Lectura completa`) con exportacion de reportes JSON.
- Busqueda (`Ctrl+F`), filtros por extension o categoria (`.dff`, `.txd`, `.col`, `.ifp`, `.ipl/.ide`), ordenacion por columnas y seleccion multiple.
- Edicion en memoria (**Importar**, **Reemplazar**, **Renombrar** con validacion del limite ASCII de 23 caracteres `VER2`, y **Eliminar**) con pila **Deshacer / Rehacer** antes de escribir en disco.
- Politicas configurables para nombres duplicados (*Preguntar*, *Reemplazar existente*, *Omitir*, *Auto-renombrar*).
- **Reconstruir y Guardar** atomico (`Ctrl+S`): escribe en archivo temporal, verifica cada payload mediante **SHA-256**, genera respaldo automatico previo y reemplaza el archivo de forma segura.

### 2. DFF Studio (Visor 3D integrado)
- Carga modelos `.dff` desde disco o directamente desde cualquier `.img` abierto (doble clic o menu contextual) sin extraer archivos temporales.
- Modos de renderizado: **Con texturas**, **Solido**, **Malla** (*Wireframe*) y **Colores de material**.
- Camara orbital (orbitar, desplazar, zoom, encuadrar, reiniciar), fondo configurable, rejilla XY, ejes Z-up, culling e iluminacion ajustable (azimut, elevacion, luz ambiental).
- Inspector de modelo con version RenderWare, conteo de geometrias/materiales/vertices/triangulos, dimensiones, resolucion de referencias de textura (`<nombre>.txd`, `vehicle.txd`, `generic.txd` o diccionarios `.txd` adjuntos) y diagnosticos.

### 3. TXD Studio (Visor y exportador de texturas)
- Abre archivos `.txd` externos o entradas dentro de un `.img`.
- Decodificadores en C++20 puro para formatos PC D3D9/D3D8: `DXT1`, `DXT3`, `DXT5`, `RASTER_8888`, `RASTER_888`, `1555`, `555`, `565`, `4444`, `LUM8`, `PAL8` y `PAL4`.
- Cuadricula de miniaturas con busqueda, filtro por dimensiones y ordenacion (disco, nombre, tamaño).
- Vista previa con canales (`RGBA`, `RGB`, `Alpha`), fondos (`Tablero`, `Oscuro`, `Claro`), filtrado (`Lineal` / `Mas cercano`), selector de mipmaps, ajuste a pantalla/zoom y exportacion a **PNG**, **TGA** o **BMP**.

### 4. IMG Compare
- Comparacion estrictamente de solo lectura por **SHA-256** mediante streaming entre dos archivos `IMG VER2` sin cargar archivos completos en RAM.
- Clasificacion en *Sin cambios*, *Agregado*, *Eliminado*, *Modificado*, *Dif. de metadatos* y *Ambiguo*.
- Exportacion de reportes en formato **JSON** y **TXT**.

### 5. Batch Processing (Procesamiento por lotes)
- Cola concurrente aislada por archivo (1 a 4 hilos) para procesar multiples `.img`.
- Operaciones soportadas: validar archivos, crear respaldos manuales, exportar todas las entradas, importar archivos o reemplazar entradas coincidentes.
- Respaldo automatico previo habilitado por defecto en operaciones modificadoras y reporte resumen **JSON**.

### 6. Internacionalizacion (Nuevo en 2.1.0)
- Cambio de idioma en tiempo real desde **Archivo -> Configuracion** (`en` y `es` incluidos) sin reiniciar la aplicacion ni perder el estado de dialogos o pestañas abiertas.
- Deteccion automatica del idioma del sistema operativo en el primer arranque (`auto`, usando `GetUserDefaultLocaleName` en Windows y variables POSIX en Linux).
- Preferencia persistente en `settings.json`, fallback automatico al ingles por clave faltante, diagnosticos de traduccion y cadenas parametrizadas UTF-8.
- Flags CLI `--lang <codigo>`, `--settings-file <ruta>` y `--save-lang` para pruebas reproducibles.

---

## Instalacion en Windows

1. Descarga `IMGStudio-2.1.0-win64.zip` desde la seccion Releases (una vez publicado).
2. Descomprime el paquete completo conservando la carpeta `assets/` junto a `IMGStudio.exe` (incluye las fuentes tipograficas y `assets/locales/*.json`).
3. Ejecuta `IMGStudio.exe`.

### Requisitos del sistema
- **Sistema operativo**: Windows 10 o Windows 11 (x64)
- **Graficos**: GPU y controlador compatibles con OpenGL 3.3 Core Profile
- **Runtime**: Ejecutable autocontenido (enlazado estatico de C++ / MinGW winpthreads; solo requiere DLLs estandar del sistema Windows)

### Uso opcional por linea de comandos
```text
IMGStudio.exe [archivo.img]
IMGStudio.exe --dff modelo.dff [--txd texturas.txd]
IMGStudio.exe --txd texturas.txd
IMGStudio.exe --lang es
```

---

## Compilacion desde el codigo fuente

IMG Studio se desarrolla y compila de forma cruzada desde **WSL2 Debian** hacia **Windows x64** (`x86_64-w64-mingw32`), manteniendo un preset nativo **Linux Debug ASan/UBSan** para deteccion de errores de memoria.

```bash
# Validar integridad de archivos de idioma
python3 scripts/validate_locales.py

# Compilar y ejecutar pruebas en Linux con ASan/UBSan
cmake --preset linux-debug-tests
cmake --build --preset linux-debug-tests
ctest --preset linux-debug-tests --output-on-failure

# Compilar binario Windows x64 Release
cmake --preset win64-release
cmake --build --preset win64-release

# Generar paquete portable ZIP Windows x64
./scripts/package_release.sh
```

Consulta [docs/BUILDING.md](docs/BUILDING.md) para ver dependencias exactas y versiones fijadas.

---

## Dependencias

Todas las librerias y fuentes estan fijadas por version/commit en [`cmake/Dependencies.cmake`](cmake/Dependencies.cmake), `third_party/` y `assets/fonts/`:

| Componente | Version / Commit | Proposito | Licencia |
| --- | --- | --- | --- |
| [Dear ImGui](https://github.com/ocornut/imgui) | `v1.91.9b-docking` (`52fe0a05...`) | Interfaz grafica de escritorio | MIT |
| [GLFW](https://github.com/glfw/glfw) | `3.4` (`70a9bb38...`) | Ventana y contexto OpenGL | zlib/libpng |
| [nlohmann/json](https://github.com/nlohmann/json) | `v3.11.3` (`9cca280a...`) | Configuracion, reportes y locales JSON | MIT |
| [stb_image_write.h](third_party/stb/stb_image_write.h) | `6b5799ba...` | Exportacion de imagenes PNG / TGA / BMP | MIT / Dominio Publico |
| [Catch2](https://github.com/catchorg/Catch2) | `v3.8.1` (`56809e52...`) | Suite automatizada de pruebas | BSL-1.0 |
| Inter & JetBrains Mono | `v4.0` / `v2.304` | Tipografia de interfaz y codigo | SIL OFL 1.1 |
| Font Awesome 6 Free Solid | `6.7.2` | Iconos vectoriales | SIL OFL 1.1 / CC BY 4.0 |

---

## Limitaciones actuales

- **Alcance de archivos IMG**: Solo soporta **GTA San Andreas IMG VER2** (`VER2`). El formato `IMG VER1` (`.dir` + `.img` de GTA III / Vice City) no esta soportado.
- **Alcance RenderWare**: Formatos raster no estandar o de consola, esqueletos/skinning (`Skin PLG`), animaciones `.ifp` y visor 3D de colisiones `.col` no estan implementados aun. Los formatos TXD no soportados se reportan sin cerrar la aplicacion.
- **Inspeccion TXD/DFF de solo lectura**: TXD Studio exporta texturas (`PNG`/`TGA`/`BMP`) e inspecciona metadatos; la re-empaquetacion directa de `.txd` queda para una version futura.
- **Escala de interfaz**: Cambiar `Escala de UI` en Configuracion requiere reiniciar para regenerar las fuentes con nitidez.

---

## Documentacion

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) — Arquitectura en 4 capas, sistema de idiomas y extensibilidad futura
- [docs/FORMATS.md](docs/FORMATS.md) — Especificaciones IMG VER2, DFF y TXD soportadas y limites
- [docs/BUILDING.md](docs/BUILDING.md) — Requisitos, compilacion cruzada y empaquetado
- [docs/TESTING.md](docs/TESTING.md) — Pruebas automatizadas, sanitizers y verificacion Windows
- [docs/LOCALIZATION.md](docs/LOCALIZATION.md) — Guia para colaboradores que deseen traducir el programa
- [CHANGELOG.md](CHANGELOG.md) y [ROADMAP.md](ROADMAP.md)

---

## Contribuciones y politica de recursos

Las contribuciones de codigo, reportes de errores y traducciones son bienvenidas. Consulta [CONTRIBUTING.md](CONTRIBUTING.md), [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md) y [SECURITY.md](SECURITY.md).

**Nunca subas ni adjuntes archivos `.img`, `.dff`, `.txd` ni recursos comerciales originales de GTA San Andreas en el repositorio, issues o pull requests.**

---

## Aviso sobre licencia

El repositorio conserva actualmente el archivo [`LICENSE`](LICENSE) (`MIT License`). Antes de publicar el repositorio o lanzar versiones publicas en GitHub, confirma con el propietario del proyecto la licencia definitiva seleccionada.
