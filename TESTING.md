# Estrategia de Pruebas y Verificacion (TESTING)

IMG Studio 2.0 amplia la suite Catch2 v3.8.1 manteniendo todas las pruebas de regresion de 1.0.0 y cubriendo los nuevos subsistemas RenderWare, comparacion y procesamiento por lotes.

## Suites de prueba

| Archivo | Cobertura |
| --- | --- |
| `tests/test_binary_io.cpp` | Little-endian, aritmetica segura, SHA-256, nombres VER2 y rutas seguras |
| `tests/test_img_reader.cpp` | Lectura VER2, archivos vacios, `archive_size`, truncamientos y proteccion OOM |
| `tests/test_img_validator.cpp` | Firma, solapamientos, limites, nombres y duplicados |
| `tests/test_img_extractor.cpp` | Extraccion individual, por lotes y archivos staged |
| `tests/test_img_builder.cpp` | Round-trip, reconstruccion multi-operacion y alineacion 2048 B |
| `tests/test_large_and_faults.cpp` | Archivos grandes (2500 entradas) y fallos controlados |
| `tests/test_archive_document.cpp` | Politicas de duplicados, Undo/Redo y commit con respaldo |
| `tests/test_backup_manager.cpp` | Respaldos, indice JSON, verificacion SHA-256 y restauracion |
| `tests/test_app_settings.cpp` | Persistencia JSON y recuperacion ante corrupcion |
| `tests/test_entry_filter.cpp` | Filtrado y ordenacion sobre 10,000 entradas |
| `tests/test_dff_parser.cpp` | Clump, Geometry, Materials, UVs, BinMesh list/strip, normales generadas, truncados y geometria nativa |
| `tests/test_txd_parser.cpp` | Decodificacion DXT1, DXT3, DXT5, BGRA8888, BGR565, PAL8, mipmaps, exportacion PNG/TGA/BMP y plataformas no soportadas |
| `tests/test_dff_txd_integration.cpp` | Carga DFF/TXD desde IMG, resolucion multi-TXD, referencias faltantes y ciclos repetidos con diccionarios de 32 texturas |
| `tests/test_img_compare.cpp` | Clasificacion *Unchanged*, *Added*, *Removed*, *Modified*, *MetadataDiff*, *Ambiguous* y reportes JSON/TXT |
| `tests/test_batch_processor.cpp` | Cola multi-archivo concurrente, aislamiento de fallos, *ExportAll*, *ImportFiles*, *ReplaceMatching*, *Backup* y cancelacion |

## Verificacion GUI y OpenGL 3.3 en Windows (`--smoke-test`)

`IMGStudio.exe --smoke-test --open <archivo.img> --smoke-test-out <reporte.txt>` valida de forma no interactiva en Windows:
- Inicializacion de GLFW + OpenGL 3.3 Core Profile y carga de `GlLoader`.
- Compilacion de shaders GLSL 330 y creacion de `ViewportFbo`.
- Apertura de `.img`, carga automatica de `.dff` y `.txd`, resolucion de textura y renderizado 3D sin errores OpenGL (`gl_error=0`).
