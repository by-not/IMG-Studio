# IMG Studio — Arquitectura

IMG Studio separa estrictamente la manipulacion binaria del formato, los servicios de sistema operativo, la logica de aplicacion y la interfaz grafica.

## Capas del sistema

```
+-------------------------------------------------------------+
|                        src/ui/                              |
|  AppWindow, MainView, ExplorerTable, InspectorPanel,        |
|  Dialogs, Theme, Icons (Dear ImGui + GLFW + OpenGL 3.3)     |
+------------------------------+------------------------------+
                               | usa exclusivamente
                               v
+-------------------------------------------------------------+
|                        src/app/                             |
|  ArchiveController (hilo worker + pestañas),                |
|  ArchiveDocument (cambios staged), OperationHistory,        |
|  BackupManager, EntryFilter, AppSettings                    |
+------------------------------+------------------------------+
                               | usa
             +-----------------+-----------------+
             v                                   v
+-------------------------+         +-------------------------+
|       src/core/         |         |     src/platform/       |
|  Motor IMG VER2 puro    |         |  platform_win32.cpp     |
|  ImgReader, ImgArchive, |         |  platform_posix.cpp     |
|  ImgValidator,          |         |  Dialogos COM, DWM dark |
|  ImgExtractor,          |         |  chrome, ReplaceFileW,  |
|  ImgBuilder, BinaryIO,  |         |  rutas UTF-8 <-> UTF-16 |
|  ImgFilename, ImgTypes  |         +-------------------------+
+-------------------------+
```

### Reglas de dependencia

1. `src/core/` no incluye encabezados de Windows, GLFW, Dear ImGui ni `nlohmann/json`. Puede compilarse y probarse de forma aislada bajo Linux con AddressSanitizer y UndefinedBehaviorSanitizer.
2. `src/ui/` nunca interpreta bytes ni offsets binarios IMG directamente; toda lectura, validacion, extraccion y reconstruccion pasa por `ArchiveController` y `ArchiveDocument`.
3. Ningun manejador de archivo (`std::ifstream` / `HANDLE`) permanece abierto de forma persistente sobre un archivo `.img` entre operaciones, evitando bloqueos (`ERROR_SHARING_VIOLATION`) en Windows durante el reemplazo atomico.

## Modelo de edicion y reconstruccion segura

1. **Edicion staged en memoria (`ArchiveDocument`)**:
   - Las operaciones `Import`, `Replace`, `Delete` y `Rename` modifican un vector de `DocumentEntry` en memoria.
   - `OperationHistory` guarda instantaneas antes/despues para soportar `Undo` y `Redo` deterministas antes de confirmar en disco.
   - Las entradas eliminadas permanecen recuperables hasta confirmar la reconstruccion.
2. **Respaldo previo (`BackupManager`)**:
   - Antes de reconstruir, si el respaldo automatico esta habilitado, se copia el archivo original a un temporal en la carpeta de respaldos, se reemplaza atomicamente y se verifica la igualdad exacta mediante SHA-256.
3. **Reconstruccion streaming (`ImgBuilder`)**:
   - Construye un `RebuildPlan` validando nombres (<= 23 bytes ASCII) y limites de tamaño (`<= 65,535` sectores).
   - Escribe en un archivo temporal en el mismo directorio del archivo destino: cabecera de 8 bytes, tabla de directorio de 32 bytes por entrada, relleno al primer sector de datos y payloads copiados en bloques de 256 KiB con relleno a multiplos de 2048 bytes.
   - Calcula SHA-256 de cada payload alineado durante la escritura.
4. **Verificacion post-construccion**:
   - Reabre el archivo temporal con `ImgReader` + `ImgValidator`, exige cero errores estructurales, compara metadatos de directorio y recalcula SHA-256 de todos los payloads escritos.
5. **Commit atomico (`AtomicFileOps`)**:
   - Ejecuta `FlushFileBuffers` sobre el temporal y reemplaza el archivo destino con `ReplaceFileW` (o `MoveFileExW` con `MOVEFILE_WRITE_THROUGH` si es nuevo). Si algo falla, el archivo original permanece intacto.
   - Tras el commit exitoso, se recarga el documento desde disco y se limpia la pila de `Undo`/`Redo` previa al commit (la recuperacion de estados confirmados se realiza desde la pestaña de Respaldos).

## Concurrencia y fluidez de UI

`ArchiveController` ejecuta las operaciones de E/S pesadas (abrir con verificacion de lectura, exportar lotes, reconstruir y confirmar, validar payloads, crear/restaurar respaldos) en un hilo trabajador dedicado (`std::thread`) con progreso atomico y bandera de cancelacion (`std::atomic<bool>`), comunicando resultados a la UI mediante una cola protegida por `std::mutex` consultada en cada fotograma.
