# Roadmap — IMG Studio

## Version 1.0.0 (Etiqueta `v1.0.0` en `main`)

- Motor IMG VER2 independiente, explorador virtualizado, validacion, respaldos automaticos y reconstruccion atomica verificada.

## Version 2.0.0 (Rama `develop`)

- **DFF Studio**: visor 3D OpenGL 3.3 Core con carga directa desde `.img` o disco, modos Textured/Solid/Wireframe/Materials, camara orbital, iluminacion, bounds y diagnosticos.
- **TXD Studio**: lector de diccionarios PC D3D9/D3D8, decodificadores DXT1/DXT3/DXT5/BGRA/PAL, visor de mipmaps y canales alfa, cuadricula de miniaturas y exportacion PNG/TGA/BMP.
- **Integracion DFF + TXD**: resolucion automatica de texturas desde el mismo IMG o diccionarios TXD manuales sin extraer a disco.
- **IMG Compare**: comparador streaming por SHA-256 con clasificacion detallada y reportes JSON/TXT.
- **Batch Processing**: cola concurrente aislada para validar, respaldar, exportar, importar y reemplazar multiples archivos IMG.

## Lineas futuras (Post-2.0)

- Soporte de skinning/huesos (`Skin PLG`) y previsualizacion de animaciones `.ifp`.
- Soporte de lectura/conversion para IMG VER1 (`.dir` + `.img`) y archivos `.col` de colisiones.
- Edicion directa de texturas dentro de archivos `.txd` (reemplazo/importacion y re-empaquetado TXD).
