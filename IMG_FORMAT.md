# Formato GTA San Andreas IMG VER2 en IMG Studio

Este documento resume la estructura binaria de los archivos IMG VER2 de GTA San Andreas, el comportamiento verificado en referencias tecnicas publicas y las garantias/limites aplicados por IMG Studio.

## Disposicion binaria

Todos los campos enteros estan codificados en **little-endian**. IMG Studio lee y escribe cada campo byte a byte (`read_u16_le`, `read_u32_le`, `write_u16_le`, `write_u32_le`) sin depender de empaquetado de estructuras del compilador.

### 1. Cabecera (8 bytes)

| Offset | Tamaño | Tipo | Campo | Descripcion |
| --- | --- | --- | --- | --- |
| `0x00` | 4 bytes | `char[4]` | `magic` | Firma exacta `'V'`, `'E'`, `'R'`, `'2'` |
| `0x04` | 4 bytes | `uint32` | `entry_count` | Numero declarado de entradas en la tabla de directorio |

### 2. Entrada de directorio (32 bytes por entrada)

Inmediatamente tras la cabecera (`offset 8`), aparecen `entry_count` registros consecutivos de 32 bytes:

| Offset | Tamaño | Tipo | Campo | Descripcion |
| --- | --- | --- | --- | --- |
| `0x00` | 4 bytes | `uint32` | `offset_sectors` | Desplazamiento de inicio del payload en sectores de 2048 bytes (`offset_bytes = offset_sectors * 2048`) |
| `0x04` | 2 bytes | `uint16` | `streaming_size` | Tamaño del recurso para el sistema de streaming (en sectores de 2048 bytes) |
| `0x06` | 2 bytes | `uint16` | `archive_size` | Tamaño almacenado en archivo (en sectores de 2048 bytes). En archivos originales de Rockstar suele ser `0`. |
| `0x08` | 24 bytes | `char[24]` | `name` | Nombre de archivo ASCII terminado en `\0` (maximo 23 caracteres utiles) |

### 3. Unidad de sector y payloads

- **Tamaño de sector**: `2048` bytes (`0x800`).
- **Inicio de datos**: el primer sector disponible para payloads es `ceil((8 + entry_count * 32) / 2048)`.
- **Archivo vacio (`entry_count == 0`)**: ocupa `8` bytes de cabecera + relleno hasta `2048` bytes (1 sector) cuando es generado por `ImgBuilder`, manteniendo alineacion limpia de sector. Al abrir archivos vacios externos de 8 bytes exactos, `ImgReader` y `ImgValidator` tambien los aceptan como validos.
- **Relleno**: los payloads que no son multiplos exactos de 2048 bytes se completan con bytes `0x00` hasta el limite del ultimo sector.

## Semantica verificada de `streaming_size` y `archive_size`

1. En los archivos originales de GTA San Andreas, `streaming_size` contiene el numero de sectores del recurso y `archive_size` es `0`.
2. Algunas herramientas de modding o contenedores modificados escriben un valor distinto de cero en `archive_size` (igual o distinto a `streaming_size`).
3. **Politica de IMG Studio**:
   - No asume que ambos campos sean siempre iguales ni normaliza silenciosamente entradas existentes.
   - Al leer o calcular limites de extraccion/validacion, usa `effective_sectors = (archive_size != 0) ? archive_size : streaming_size`.
   - En entradas sin modificar o unicamente renombradas, `ImgBuilder` preserva exactamente los valores originales de `streaming_size` y `archive_size`, asi como los 24 bytes crudos del nombre cuando no se ha renombrado.
   - En entradas importadas o reemplazadas desde archivos externos, calcula `sectors = ceil(file_size / 2048)`, valida `sectors <= 65535`, asigna `streaming_size = sectors` y `archive_size = 0`.
   - Si `archive_size != 0`, `ImgValidator` emite un diagnostico informativo (`SuspiciousSizeMetadata`, severidad `Info`) para visibilidad sin marcarlo como corrupcion.

## Limites conocidos e incertidumbres documentadas

- **Limite por entrada**: como `streaming_size` y `archive_size` son enteros de 16 bits sin signo, una entrada no puede superar `65,535` sectores (`134,215,680` bytes ≈ 128 MiB).
- **Umbral de 2 GiB en el juego**: aunque el offset en sectores de 32 bits permite direccionar mas de 2 GiB en el formato, ejecutables clasicos de GTA San Andreas sin limit adjusters pueden fallar por aritmetica de 32 bits con signo en desplazamientos en bytes; `ImgValidator` emite una advertencia (`LargeArchiveAbove2GiB`) si el archivo excede 2 GiB.
- **Nombres**: VER2 usa buffers de 24 bytes terminados en nulo. IMG Studio exige nombres ASCII imprimibles de 1 a 23 bytes sin separadores de ruta ni nombres reservados de Windows (`CON`, `PRN`, `AUX`, `NUL`, `COM1..9`, `LPT1..9`).
