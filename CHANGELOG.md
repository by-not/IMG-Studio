# Changelog

## [2.0.0] - 2026-11-24

### Added
- **DFF Studio**: analizador modular RenderWare (`src/core/rw/DffParser.*`) y visor 3D OpenGL 3.3 Core (`src/render/DffRenderer.*`, `src/ui/DffStudioView.*`) con carga desde archivos externos o entradas IMG, camara orbital, rejilla, ejes, modos de sombreado e inspector completo.
- **TXD Studio**: analizador y decodificadores RenderWare TXD PC D3D9/D3D8 (`DXT1`, `DXT3`, `DXT5`, `RASTER_8888`, `888`, `1555`, `555`, `565`, `4444`, `LUM8`, `PAL8`, `PAL4`), cuadricula de miniaturas, visor con canal alfa/mipmaps y exportacion PNG/TGA/BMP (`src/core/rw/ImageExporter.*` con `stb_image_write.h`).
- **Integracion DFF + TXD**: `AssetLoader` carga assets directamente desde IMG en memoria y resuelve texturas desde `<stem>.txd`, `vehicle.txd`, `generic.txd` o diccionarios TXD cargados manualmente.
- **IMG Compare**: comparacion streaming SHA-256 entre dos archivos IMG (`src/core/ImgCompare.*`, `src/app/CompareService.*`, `src/ui/ImgCompareView.*`) con exportacion JSON y TXT.
- **Batch Processing**: procesamiento por lotes concurrente y aislado (`src/app/BatchProcessor.*`, `src/ui/BatchView.*`) para validacion, respaldo, exportacion, importacion y reemplazo sobre multiples IMG.
- Documentacion tecnica dedicada en `DFF_FORMAT.md` y `TXD_FORMAT.md`, y nuevas suites Catch2 de pruebas unitarias e integracion.

## [1.0.0] - 2026-11-24

- Lanzamiento inicial estable (preservado en rama `main` y etiqueta `v1.0.0`) con motor IMG VER2, explorador virtualizado, edicion staged con Undo/Redo, respaldos automaticos y reconstruccion atomica verificada.
