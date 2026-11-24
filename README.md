# IMG Studio 2.0

IMG Studio es una aplicacion de escritorio nativa para Windows x64 (Windows 10 y Windows 11) diseñada para gestionar archivos **IMG VER2 de GTA San Andreas**, visualizar modelos **3D RenderWare DFF**, inspeccionar y exportar diccionarios de texturas **TXD**, comparar archivos IMG y ejecutar operaciones por lotes.

Esta desarrollada en C++20 desde cero con separacion estricta entre el motor binario, los servicios de aplicacion, el renderizador OpenGL 3.3 Core y la interfaz grafica moderna en modo oscuro (Dear ImGui + GLFW). No reutiliza codigo ni ingenieria inversa de herramientas heredadas.

## Novedades principales en 2.0.0

- **DFF Studio (visor 3D integrado)**: carga archivos `.dff` externos o directamente desde un archivo `.img` abierto; soporta modos *Textured*, *Solid*, *Wireframe* y *Material Colors*, camara orbital con pan/zoom/fit/reset, rejilla XY, ejes Z-up, fondo e iluminacion configurables, inspector completo de geometrias/materiales/bounds y diagnosticos claros para variantes no soportadas.
- **TXD Studio (visor de texturas)**: abre `.txd` externos o entradas dentro de un `.img`; decodifica en C++20 puro formatos PC D3D9/D3D8 (`DXT1`, `DXT3`, `DXT5`, `RASTER_8888`, `RASTER_888`, `1555`, `555`, `565`, `4444`, `LUM8`, `PAL8`, `PAL4`), muestra cuadricula de miniaturas, inspeccion de mipmaps, canales `RGBA`/`RGB`/`Alpha`, fondo tablero/oscuro/claro, filtro `Linear`/`Nearest`, zoom y exportacion a `PNG`, `TGA` o `BMP`.
- **Integracion DFF + TXD**: al abrir un `.dff` desde un `.img` o disco, resuelve automaticamente sus referencias de textura contra `<nombre>.txd`, `vehicle.txd`, `generic.txd` o diccionarios TXD cargados manualmente, sin extraer archivos temporales a disco.
- **IMG Compare**: compara dos archivos `.img` mediante SHA-256 por streaming sin cargar archivos completos en memoria, clasificando diferencias en *Unchanged*, *Added*, *Removed*, *Modified*, *Metadata Diff* y *Ambiguous*, con exportacion JSON/TXT.
- **Batch Processing**: cola concurrente (1 a 4 hilos aislados por archivo) para validar, respaldar, exportar todo, importar archivos o reemplazar entradas coincidentes en multiples `.img`, con respaldos automaticos y reporte JSON.
- **Estabilidad 1.0.0 conservada**: todas las operaciones seguras de lectura, validacion, extraccion, edicion staged con Undo/Redo, reconstruccion verificada con SHA-256 y respaldos automaticos permanecen intactas.

## Requisitos (Windows)

- Windows 10 o Windows 11 (x64).
- Adaptador grafico compatible con OpenGL 3.3 Core Profile.
- Sin dependencias de runtime externas de MinGW ni Visual C++.

## Uso rapido

1. Extrae `IMGStudio-2.0.0-win64.zip` manteniendo la carpeta `assets/` junto a `IMGStudio.exe`.
2. Ejecuta `IMGStudio.exe` (opcionalmente pasando `<archivo.img>`, `--dff <modelo.dff>` o `--txd <diccionario.txd>`).
3. Cambia entre los 5 espacios de trabajo desde la barra superior: **Archive Explorer**, **DFF Studio**, **TXD Studio**, **IMG Compare** y **Batch Processing**.
4. Haz doble clic o clic derecho sobre una entrada `.dff` o `.txd` en **Archive Explorer** para abrirla directamente en su estudio sin extraerla a disco.

## Documentacion tecnica

- [ARCHITECTURE.md](ARCHITECTURE.md) — Arquitectura modular en 4 capas y concurrencia segura.
- [IMG_FORMAT.md](IMG_FORMAT.md) — Especificacion IMG VER2 de GTA San Andreas.
- [DFF_FORMAT.md](DFF_FORMAT.md) — Chunks RenderWare DFF soportados y limitaciones.
- [TXD_FORMAT.md](TXD_FORMAT.md) — Formatos TXD, decodificadores DXT/PAL/RGBA y exportacion.
- [BUILDING.md](BUILDING.md) — Compilacion cruzada desde WSL2 Debian, presets y empaquetado.
- [TESTING.md](TESTING.md) — Pruebas automatizadas, sanitizers y verificacion Windows.
- [ROADMAP.md](ROADMAP.md) y [CHANGELOG.md](CHANGELOG.md).
