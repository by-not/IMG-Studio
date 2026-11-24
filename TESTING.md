# Estrategia de Pruebas y Verificacion (TESTING)

IMG Studio cuenta con una suite automatizada basada en Catch2 v3.8.1 que cubre tanto el motor binario puro (`src/core/`) como los servicios de aplicacion (`src/app/`). Todas las pruebas destructivas operan exclusivamente sobre archivos sintéticos temporales creados y eliminados mediante `TempDirGuard`.

## Suites de prueba implementadas

| Archivo | Cobertura principal |
| --- | --- |
| `tests/test_binary_io.cpp` | Lectura/escritura little-endian, aritmetica verificada contra overflow, vectores SHA-256, validacion de nombres VER2 y rutas seguras contra path traversal |
| `tests/test_img_reader.cpp` | Lectura de archivos VER2 validos, archivos vacios (0 entradas), preservacion de `archive_size != 0`, cabeceras truncadas y proteccion contra asignaciones OOM por `entry_count` gigante |
| `tests/test_img_validator.cpp` | Deteccion de firma invalida, solapamiento con directorio, entradas fuera de limites, solapamiento entre entradas, nombres sin `\0`, basura tras `\0` y duplicados insensibles a mayusculas |
| `tests/test_img_extractor.cpp` | Extraccion individual, por lotes y desde archivos externos staged; rechazo de nombres inseguros sin descartar silenciosamente |
| `tests/test_img_builder.cpp` | Round-trip de archivo sin cambios (igualdad byte a byte y preservacion de `archive_size`), reconstruccion tras añadir/reemplazar/renombrar/eliminar, alineacion a 2048 bytes y construccion de archivo vacio |
| `tests/test_large_and_faults.cpp` | Archivo grande de 2500 entradas con directorio multi-sector (40 sectores) y pruebas de fallo controlado en escritura/origen conservando intacto el original |
| `tests/test_archive_document.cpp` | Politicas de duplicados (`Ask`, `Skip`, `Replace`, `AutoRename`), Undo/Redo, renombrado/eliminacion/importacion staged y commit completo con respaldo |
| `tests/test_backup_manager.cpp` | Creacion de respaldo manual, indice JSON, verificacion SHA-256 y restauracion segura tras corrupcion simulada del archivo original |
| `tests/test_app_settings.cpp` | Serializacion/deserializacion JSON de `settings.json`, deduplicacion de archivos recientes y recuperacion ante JSON corrupto |
| `tests/test_entry_filter.cpp` | Busqueda insensible a mayusculas, filtros combinados, ordenacion por columnas y rendimiento con 10,000 entradas |

## Ejecucion de pruebas

### Linux con AddressSanitizer + UndefinedBehaviorSanitizer

```bash
cmake --preset linux-debug-tests
cmake --build --preset linux-debug-tests
ctest --preset linux-debug-tests --output-on-failure
```

### Windows x64 nativo (desde WSL2 o consola Windows)

En Windows PowerShell / CMD:

```cmd
build\win64-release\IMGStudioTests.exe
```

Desde WSL2 (redirigiendo entrada estandar para evitar bloqueos de interoperabilidad):

```bash
./build/win64-release/IMGStudioTests.exe < /dev/null
```

### Verificacion de arranque GUI en Windows (`--smoke-test`)

El binario `IMGStudio.exe` incluye un modo `--smoke-test` no interactivo que inicializa GLFW, crea la ventana oculta con OpenGL 3.3 Core Profile, carga las fuentes y el tema oscuro, abre un archivo `.img` de prueba si se indica con `--open`, renderiza 3 fotogramas completos y escribe un informe de diagnostico:

```bash
IMGStudio.exe --smoke-test --open "<ruta_windows.img>" --smoke-test-out "<reporte.txt>"
```
