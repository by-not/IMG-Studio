# Supported Formats & Technical Reference — IMG Studio

IMG Studio **2.1.0** independently implements parsers, validators, and writers for **GTA San Andreas (PC)** formats:
1. **IMG VER2 Archives (`.img`)**
2. **RenderWare 3D Models (`.dff`)**
3. **RenderWare Texture Dictionaries (`.txd`)**

> Legacy GTA III / Vice City `IMG VER1` (`.dir` + `.img`) and proprietary console asset containers are not supported in this version.

---

## 1. GTA San Andreas IMG VER2 (`.img`)

All integers are **little-endian**, read/written explicitly byte-by-byte without compiler struct-packing assumptions.

### Binary Layout
- **Header (`8` bytes)**:
  - `0x00` (`4` bytes): Magic signature `'V'`, `'E'`, `'R'`, `'2'`
  - `0x04` (`4` bytes): `entry_count` (`uint32`)
- **Directory Table (`entry_count * 32` bytes starting at `0x08`)**:
  - `0x00` (`4` bytes): `offset_sectors` (`uint32`) — byte offset `= offset_sectors * 2048`
  - `0x04` (`2` bytes): `streaming_size` (`uint16`) — payload size in `2048`-byte sectors
  - `0x06` (`2` bytes): `archive_size` (`uint16`) — stored archive size in sectors (`0` in standard Rockstar archives)
  - `0x08` (`24` bytes): `name` (`char[24]`) — null-terminated ASCII filename (max `23` usable characters)
- **Sector Alignment**: `1` sector = `2048` bytes (`0x800`). Non-aligned payloads are zero-padded to sector boundaries.

### Engineering Policies & Known Limits
- **Effective Sector Size**: Reads use `(archive_size != 0) ? archive_size : streaming_size`. Unmodified/renamed entries preserve original `streaming_size`, `archive_size`, and raw 24-byte name buffers; newly imported/replaced entries set `streaming_size = ceil(size / 2048)` and `archive_size = 0`.
- **Single Entry Cap**: `65,535` sectors (`134,215,680` bytes ≈ `128 MiB`) enforced by the 16-bit sector field.
- **2 GiB Game Threshold**: `ImgValidator` emits `LargeArchiveAbove2GiB` warnings when an archive exceeds `2 GiB`, as unpatched classic GTA San Andreas executables use signed 32-bit byte arithmetic.
- **Filename Safety**: Enforces `1..23` printable ASCII characters, rejects path separators and Windows reserved device names (`CON`, `PRN`, `AUX`, `NUL`, `COM1..9`, `LPT1..9`).

---

## 2. RenderWare DFF Models (`.dff`)

Target RenderWare build IDs include GTA San Andreas (`0x1803FFFF` / RW `3.6.0.3`) and compatible chunk structures.

### Supported Chunk Hierarchy
- **Root**: `Clump` (`0x0010`) or standalone `Geometry` (`0x000F`).
- **`FrameList` (`0x000E`)**: 3x3 rotation matrix, translation, `parent_index`, flags, and `FrameName` (`0x0253F2FE`) extension with cycle-safe hierarchy evaluation.
- **`GeometryList` (`0x001A`) -> `Geometry` (`0x000F`)**:
  - Surface properties in `Geometry Struct` (RW `< 3.4.0.0`) or `Material Struct` (RW `>= 3.4.0.0`).
  - Pre-lit vertex colors (`rpGEOMETRYPRELIT`, `0x0008`).
  - Texture coordinates (`rpGEOMETRYTEXTURED` / `UV` channel `0`, safely skipping extra UV channels).
  - Morph target `0` positions and normals (synthesizes smooth vertex normals with a warning when missing).
  - `MaterialList` -> `Material` (supports material instancing references) -> `Texture` -> `String` (diffuse texture name and alpha mask name).
  - `Extension` -> `BinMesh PLG` (`0x050E`): triangle lists (`flags == 0`) and triangle strips (`flags == 1`) with alternating winding and degenerate triangle filtering; falls back to `RpTriangle` table when absent.
- **`Atomic` (`0x0014`)**: Links geometries to parent frames.

### Known DFF Limitations
- **Native Geometry (`rpGEOMETRYNATIVE`, `0x01000000`)**: Detected explicitly, flagged `is_native_unsupported`, and reported cleanly without attempting unsafe rendering.
- **Bone Skinning (`Skin PLG`, `0x0116`) & `.ifp` Animations**: Rendered in base bind pose without animated skeletal deformation.
- **Extra Channels**: Rockstar Night Vertex Colors and secondary reflection UVs (`UV2`) are not blended in DFF Studio.

---

## 3. RenderWare TXD Texture Dictionaries (`.txd`)

Parses `TexDictionary` (`0x0016`) containers holding sequential `TextureNative` (`0x0015`) chunks.

### Supported Platforms & Pixel Formats
- **Supported Platforms**: `platformId == 9` (`rwID_PCD3D9`, native GTA San Andreas PC) and `platformId == 8` (`rwID_PCD3D8`). Unsupported console platforms (e.g., PS2, Xbox) report metadata safely and skip decoding.
- **Decoded Formats (`RGBA8` linear)**:
  - **S3TC Block Compression**: `DXT1` (1-bit alpha when `has_alpha && c0 <= c1`), `DXT3` (explicit 4-bit alpha), `DXT5` (interpolated 8-level alpha)
  - **Uncompressed**: `RASTER_8888`, `RASTER_888`, `RASTER_1555`, `RASTER_555`, `RASTER_565`, `RASTER_4444`, `RASTER_LUM8`
  - **Paletted**: `PAL8` (256-entry RGBA palette), `PAL4` (16-entry RGBA palette)
- **Mipmaps & Export**: Decodes all declared mipmap levels sequentially with bounds checks (`<= 16384` px) and exports selected/all textures to `PNG` (RGBA8), `TGA` (32-bit RGBA), or `BMP` (24-bit RGB).

### Known TXD Limitations
- Console-swizzled textures (PS2/Xbox), `DXT2`/`DXT4` premultiplied alpha, and mobile ETC/PVRTC formats are not supported.
- Legacy external `maskName` alpha pairing is shown in metadata rather than composited across separate textures.
