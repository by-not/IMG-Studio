# IMG Studio 2.0 — Arquitectura

IMG Studio 2.0 mantiene la separacion estricta introducida en 1.0.0 y añade un motor RenderWare independiente y un renderizador OpenGL 3.3 Core aislado exclusivamente en el binario GUI.

## Capas del sistema

```
+-----------------------------------------------------------------------+
|                              src/ui/                                  |
|  AppWindow, MainView, ExplorerTable, InspectorPanel, Dialogs, Theme,  |
|  DffStudioView, TxdStudioView, ImgCompareView, BatchView              |
+-------------------------------+---------------------------------------+
                                | usa
          +---------------------+---------------------+
          v                                           v
+-----------------------------------+   +-------------------------------+
|            src/app/               |   |         src/render/           |
|  ArchiveController, ArchiveDoc,   |   |  GlLoader, Math3D, GpuTexture |
|  OperationHistory, BackupManager, |   |  ViewportFbo, DffRenderer     |
|  EntryFilter, AppSettings,        |   |  (solo en target GUI IMGStudio)|
|  AssetLoader, CompareService,     |   +-------------------------------+
|  BatchProcessor                   |
+-----------------+-----------------+
                  | usa
      +-----------+-----------+
      v                       v
+---------------------------+ +---------------------------+
|         src/core/         | |      src/platform/        |
|  IMG VER2 + ImgCompare +  | |  platform_win32.cpp       |
|  src/core/rw/             | |  platform_posix.cpp       |
|  (RwStream, DffParser,    | |  COM dialogs, DWM dark,   |
|   TxdParser, TxdDecoder,  | |  ReplaceFileW atomico     |
|   ImageExporter)          | +---------------------------+
+---------------------------+
```

## Principios de diseño en 2.0

1. **Separacion entre parsing binario y renderizado GPU**: `DffParser`, `TxdParser`, `TxdDecoder` e `ImageExporter` residen en `src/core/rw/` sin ninguna referencia a OpenGL, GLFW o ImGui, permitiendo probarlos bajo Linux ASan/UBSan.
2. **Carga directa desde IMG sin extraccion a disco**: `AssetLoader` lee rangos exactos del `.img` (o archivos staged externos) directamente en buffers en memoria y resuelve texturas desde `<stem>.txd`, `vehicle.txd`, `generic.txd` o diccionarios TXD cargados manualmente.
3. **Propiedad RAII de recursos OpenGL 3.3**: `GpuTexture`, `ViewportFbo` y `DffRenderer` encapsulan VAOs, VBOs, EBOs, programas GLSL y texturas 2D, liberandolos al cambiar de modelo o antes de destruir el contexto GLFW.
4. **Concurrencia aislada en Batch y Compare**: `CompareService` y `BatchProcessor` ejecutan E/S pesada en hilos trabajadores; en `BatchProcessor`, los archivos objetivo se deduplican por ruta canonica y cada worker trabaja con instancias locales independientes de `ArchiveDocument`, serializando unicamente el indice de respaldos y la tabla de resultados.
5. **Integridad de 1.0.0 intacta**: `ImgReader`, `ImgValidator`, `ImgExtractor`, `ImgBuilder` y `ArchiveDocument` conservan su flujo de reconstruccion temporal + verificacion SHA-256 + reemplazo atomico.
