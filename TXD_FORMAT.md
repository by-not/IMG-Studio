# Formato RenderWare TXD en IMG Studio 2.0

Este documento describe los formatos de diccionarios de texturas (`.txd`) soportados por `src/core/rw/TxdParser.*`, los decodificadores implementados en `src/core/rw/TxdDecoder.*` y la exportacion de imagenes en `src/core/rw/ImageExporter.*`.

## Estructura soportada

- **`TexDictionary` (`0x0016`)**:
  - `Struct` (`0x0001`): lee `declared_count` (`uint16`) y `device_id` (`uint16`).
  - Secuencia de chunks **`TextureNative` (`0x0015`)** con su cabecera de 88 bytes en `Struct` (`0x0001`).

## Plataformas soportadas

- **`platformId == 9` (`rwID_PCD3D9`)**: formato nativo PC Direct3D 9 de **GTA San Andreas**.
- **`platformId == 8` (`rwID_PCD3D8`)**: formato nativo PC Direct3D 8 de GTA III / Vice City.
- Cualquier otro `platformId` (como PS2 = `4` o Xbox = `5`) conserva sus metadatos, marca `supported = false` e informa el motivo sin interrumpir el resto del diccionario.

## Formatos de pixel y compresion implementados y probados

Todos los formatos soportados se decodifican en C++20 puro a `RGBA8` lineal (`width * height * 4` bytes) preservando el canal alfa:

| Formato / Codigo | Compresion / Layout | Canal Alfa | Estado |
| --- | --- | --- | --- |
| `DXT1` (`'DXT1'` / D3D8 `1`) | S3TC BC1 bloques 4x4 | 1-bit cuando `has_alpha` y `c0 <= c1`; opaco si no | Implementado y probado |
| `DXT3` (`'DXT3'` / D3D8 `3`) | S3TC BC2 bloques 4x4 | Alfa explicito de 4 bits por pixel | Implementado y probado |
| `DXT5` (`'DXT5'` / D3D8 `5`) | S3TC BC3 bloques 4x4 | Alfa interpolado de 8 niveles por bloque | Implementado y probado |
| `RASTER_8888` (`0x0500`) | BGRA 32-bit sin compresion | 8 bits (`has_alpha` respetado) | Implementado y probado |
| `RASTER_888` (`0x0600`) | BGRX 32-bit o BGR 24-bit | Opaco (`A = 255`) | Implementado y probado |
| `RASTER_1555` (`0x0100`) | BGRA 16-bit (1-5-5-5) | 1-bit (`A = 0` o `255`) | Implementado y probado |
| `RASTER_555` (`0x0A00`) | BGRX 16-bit (1-5-5-5) | Opaco (`A = 255`) | Implementado y probado |
| `RASTER_565` (`0x0200`) | BGR 16-bit (5-6-5) | Opaco (`A = 255`) | Implementado y probado |
| `RASTER_4444` (`0x0300`) | BGRA 16-bit (4-4-4-4) | 4 bits expandidos a 8 bits | Implementado y probado |
| `RASTER_LUM8` (`0x0400`) | Escala de grises 8-bit | Opaco (`A = 255`) | Implementado y probado |
| `PAL8` (`0x2000`) | Paleta 256x4 RGBA + indices 8-bit | Segun formato base (`8888`/`4444`/`1555` vs `888`) | Implementado y probado |
| `PAL4` (`0x4000`) | Paleta 16x4 RGBA + indices 4-bit | Segun formato base (`8888`/`4444`/`1555` vs `888`) | Implementado y probado |

## Mipmaps y exportacion

- Decodifica todos los niveles mipmap declarados secuencialmente (`numLevels`), comprobando limites (`<= 16384` px) y tamaño exacto de bufer antes de decodificar cada nivel.
- **Exportacion**: `PNG` (RGBA8 sin perdida con alfa), `TGA` (32-bit con alfa) y `BMP` (24-bit RGB). Para preservar transparencia exacta se recomienda exportar en PNG o TGA.

## Limitaciones conocidas

- No se soportan texturas nativas especificas de consola (PS2 swizzled, Xbox, formatos moviles ETC/PVRTC) ni formatos `DXT2`/`DXT4` pre-multiplicados.
- Las mascaras alfa heredadas referenciadas por `maskName` (usadas en versiones antiguas de RenderWare como textura separada) se muestran en el inspector como metadato, pero no se combinan automaticamente sobre otra textura distinta.
