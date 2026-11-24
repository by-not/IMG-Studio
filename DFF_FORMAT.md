# Formato RenderWare DFF en IMG Studio 2.0

Este documento describe los chunks y estructuras binarias RenderWare (`.dff`) soportadas por el analizador independiente `src/core/rw/DffParser.*` y el renderizador `src/render/DffRenderer.*`.

## Cabecera de Chunk RenderWare (12 bytes)

| Offset | Tamaño | Tipo | Campo | Descripcion |
| --- | --- | --- | --- | --- |
| `0x00` | 4 bytes | `uint32` | `type` | Identificador de chunk RenderWare |
| `0x04` | 4 bytes | `uint32` | `size` | Tamaño del payload en bytes (sin incluir los 12 bytes de cabecera) |
| `0x08` | 4 bytes | `uint32` | `library_id` | Version / Build ID codificado (en GTA San Andreas tipicamente `0x1803FFFF` -> RW `3.6.0.3`) |

## Jerarquia soportada

- **`Clump` (`0x0010`)** o **`Geometry` (`0x000F`)** como raiz.
  - **`FrameList` (`0x000E`)**: matrices de rotacion 3x3, traslacion, `parent_index`, flags y extension `FrameName` (`0x0253F2FE`). Calcula matrices de mundo protegiendo contra ciclos de jerarquia.
  - **`GeometryList` (`0x001A`)** -> **`Geometry` (`0x000F`)**:
    - Soporta propiedades de superficie en `Geometry Struct` para RW `< 3.4.0.0` y en `Material Struct` para RW `>= 3.4.0.0` (GTA San Andreas).
    - Colores pre-iluminados (`rpGEOMETRYPRELIT`, `0x0008`).
    - Coordenadas UV (`rpGEOMETRYTEXTURED`, `0x0004` / `rpGEOMETRYTEXTURED2`, `0x0080` / contador en bits `16..23`): lee el canal UV 0 y salta canales adicionales de forma segura.
    - Posiciones y normales del morph target 0. Si el modelo carece de normales, genera normales suaves por vertice y registra una advertencia.
    - **`MaterialList` (`0x0008`)** -> **`Material` (`0x0007`)** (incluyendo reutilizacion por referencia `>= 0`) -> **`Texture` (`0x0006`)** -> **`String` (`0x0002`)** con nombre de textura difusa y mascara alfa.
    - **`Extension` (`0x0003`)** -> **`BinMesh PLG` (`0x050E`)**: soporta tanto listas de triangulos (`flags == 0`) como tiras de triangulos (`flags == 1`, triangle strips) con alternancia de orientacion (*winding*) y descarte de triangulos degenerados.
    - Fallback a tabla `RpTriangle` (`v2, v1, matId, v3`) cuando `BinMesh PLG` no esta presente.
  - **`Atomic` (`0x0014`)**: vincula cada geometria con su frame padre dentro del Clump.

## Variantes no soportadas y limitaciones documentadas

- **Geometria nativa (`rpGEOMETRYNATIVE`, bit `0x01000000`)**: presente en ciertos archivos compilados especificamente para consola o hardware propietario; `DffParser` la detecta explicitamente, marca `is_native_unsupported = true` y reporta un diagnostico de error sin bloquear ni renderizar geometria corrupta.
- **Skinning / huesos (`Skin PLG`, `0x0116`)** y **animaciones `.ifp`**: el visor muestra los modelos en su pose base de referencia sin deformacion esqueletica animada.
- **Iluminacion nocturna Rockstar (`Night Vertex Colors`)**: actualmente no se mezcla como segunda tabla de color por vertice.
- **Segundo juego de coordenadas UV (`UV2`)**: utilizado para reflejos de entorno/coche en tiempo de ejecucion dentro del motor del juego; el visor 3D utiliza el canal UV 0 para el texturizado difuso principal.
