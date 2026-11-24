# Changelog

## [1.0.0] - 2026-11-24

### Added
- Motor binario IMG VER2 independiente (`src/core/`) con analisis little-endian explicito, sectores de 2048 bytes, preservacion de `streaming_size` y `archive_size`, validacion estructural/lectura, extraccion streaming y reconstruccion verificada con SHA-256.
- Capa de plataforma Windows x64 y POSIX (`src/platform/`) con dialogos nativos COM (`IFileOpenDialog`, `IFileSaveDialog`), reemplazo atomico (`ReplaceFileW` / `MoveFileExW`), titulo oscuro DWM y apertura en el Explorador.
- Capa de servicios de aplicacion (`src/app/`) con gestion multi-pestaña, operaciones staged con Undo/Redo, politicas de duplicados, configuracion JSON persistente y gestor de respaldos automaticos/manuales con restauracion verificada.
- Interfaz grafica oscura en Dear ImGui + GLFW + OpenGL 3.3 (`src/ui/`) con barra de herramientas, panel lateral de resumen y filtros, tabla exploradora virtualizada, inspector de propiedades/historial/validacion/respaldos, drag & drop, modales de confirmacion y notificaciones toast.
- Suite automatizada Catch2 (23 casos de prueba) verificada bajo Linux ASan/UBSan y Windows x64 nativo.
- Configuracion de compilacion reproducible con CMake Presets, recursos Win32 (icono multi-resolucion, manifiesto PerMonitorV2/UTF-8, informacion de version) y script de empaquetado distribuible `scripts/package_release.sh`.
