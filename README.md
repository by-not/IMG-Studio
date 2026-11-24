# IMG Studio

IMG Studio es una aplicacion de escritorio nativa para Windows x64 (Windows 10 y Windows 11) diseñada para inspeccionar, validar, modificar y reconstruir archivos **IMG VER2 de GTA San Andreas**.

Esta desarrollada desde cero en C++20 con separacion estricta entre el motor binario, los servicios de plataforma y la interfaz grafica moderna en modo oscuro (Dear ImGui + GLFW + OpenGL 3.3). No reutiliza codigo ni ingenieria inversa de herramientas heredadas como Alci's IMG Editor.

## Caracteristicas principales

- **Motor binario IMG VER2 independiente**: lectura por streaming con analisis little-endian explicito de la cabecera de 8 bytes (`VER2` + contador de entradas) y entradas de directorio de 32 bytes (`offset_sectors`, `streaming_size`, `archive_size`, `raw_name` de 24 bytes), alineacion estricta a sectores de 2048 bytes y aritmetica verificada contra desbordamientos.
- **Preservacion de metadatos**: conserva `streaming_size`, `archive_size` y los 24 bytes crudos de nombre en entradas no modificadas, distinguiendo ambos campos de tamaño.
- **Reconstruccion segura y atomica**: nunca sobrescribe el archivo original en sitio. Escribe el archivo reconstruido en un temporal, verifica estructura + hashes SHA-256 de todos los payloads escritos y ejecuta reemplazo atomico Win32 (`ReplaceFileW` / `MoveFileExW` con `FlushFileBuffers`).
- **Respaldos automaticos y manuales**: crea copias de seguridad verificadas por SHA-256 antes de cada reconstruccion confirmada (configurable), mantiene un indice historico (`backups_index.json`) e incluye restauracion segura e instrucciones de recuperacion manual.
- **Validacion y diagnostico**: detecta firmas invalidas, cabeceras/directorios truncados, desbordamientos, entradas fuera de limites, solapamientos con directorio o entre entradas, nombres invalidos o sin terminador nulo, duplicados insensibles a mayusculas, metadatos de tamaño inusuales y errores de lectura de payloads.
- **Interfaz moderna y fluida**: soporte multi-pestaña, tabla virtualizada con ordenacion por columnas, busqueda instantanea, filtros por tipo y extension, seleccion multiple, arrastrar y soltar desde el Explorador de Windows, historial Undo/Redo previo al commit, inspector de propiedades/validacion/respaldos y dialogos nativos de Windows.

## Requisitos (Windows)

- Windows 10 o Windows 11 (x64).
- Adaptador grafico compatible con OpenGL 3.3 Core Profile.
- No requiere instalar redistribuibles externos de MinGW ni Visual C++ (el runtime C++/pthread se enlaza estaticamente).

## Uso rapido

1. Extrae el paquete ZIP `IMGStudio-1.0.0-win64.zip` manteniendo la carpeta `assets/` junto a `IMGStudio.exe`.
2. Ejecuta `IMGStudio.exe` (o pasa una ruta `.img` por linea de comandos / arrastrandola sobre la ventana).
3. Realiza importaciones, reemplazos, renombrados o eliminaciones en memoria (puedes usar `Ctrl+Z` / `Ctrl+Y` o descartar cambios).
4. Pulsa **Rebuild & Save** (`Ctrl+S`) para crear el respaldo automatico, reconstruir, verificar con SHA-256 y confirmar en disco.

## Atajos de teclado principales

| Atajo | Accion |
| --- | --- |
| `Ctrl+O` | Abrir archivo `.img` |
| `Ctrl+N` | Crear nuevo archivo `.img` VER2 vacio |
| `Ctrl+S` | Reconstruir y guardar cambios pendientes |
| `Ctrl+W` | Cerrar pestaña activa |
| `Ctrl+I` | Importar archivos al archivo activo |
| `Ctrl+E` | Exportar entradas seleccionadas |
| `Ctrl+F` | Enfocar cuadro de busqueda |
| `Ctrl+A` | Seleccionar todas las entradas visibles |
| `Ctrl+Z` / `Ctrl+Y` | Deshacer / Rehacer cambio pendiente |
| `F2` | Renombrar entrada seleccionada |
| `Supr` | Marcar seleccion para eliminar |
| `F5` | Validar estructura del archivo activo |

## Documentacion del proyecto

- [ARCHITECTURE.md](ARCHITECTURE.md) — Arquitectura por capas, concurrencia y flujo de commit seguro.
- [IMG_FORMAT.md](IMG_FORMAT.md) — Especificacion VER2 documentada y decisiones de implementacion.
- [BUILDING.md](BUILDING.md) — Compilacion cruzada desde WSL2 Debian, presets y empaquetado.
- [TESTING.md](TESTING.md) — Suite de pruebas automatizadas, sanitizers y verificacion en Windows.
- [ROADMAP.md](ROADMAP.md) — Alcance de la version 1.0.0 y lineas futuras.
- [CHANGELOG.md](CHANGELOG.md) — Historial de versiones.
