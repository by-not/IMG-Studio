#!/usr/bin/env python3
"""Generate multi-resolution Windows ICO and PNG icon for IMG Studio without external dependencies."""

import math
import os
import struct
import zlib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "assets", "icons")


def encode_png(width: int, height: int, rgba: bytes) -> bytes:
    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    raw_rows = bytearray()
    stride = width * 4
    for y in range(height):
        raw_rows.append(0)
        raw_rows.extend(rgba[y * stride : (y + 1) * stride])

    ihdr = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    idat = zlib.compress(bytes(raw_rows), level=9)
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) + chunk(b"IDAT", idat) + chunk(b"IEND", b"")


def blend(dst: tuple, src: tuple) -> tuple:
    sr, sg, sb, sa = src
    if sa <= 0.0:
        return dst
    if sa >= 1.0:
        return src
    dr, dg, db, da = dst
    out_a = sa + da * (1.0 - sa)
    if out_a <= 1e-6:
        return (0.0, 0.0, 0.0, 0.0)
    r = (sr * sa + dr * da * (1.0 - sa)) / out_a
    g = (sg * sa + dg * da * (1.0 - sa)) / out_a
    b = (sb * sa + db * da * (1.0 - sa)) / out_a
    return (r, g, b, out_a)


def sd_rounded_box(px: float, py: float, cx: float, cy: float, hx: float, hy: float, r: float) -> float:
    dx = abs(px - cx) - (hx - r)
    dy = abs(py - cy) - (hy - r)
    outside = math.hypot(max(dx, 0.0), max(dy, 0.0))
    inside = min(max(dx, dy), 0.0)
    return outside + inside - r


def smoothstep_edge(d: float, aa: float) -> float:
    return max(0.0, min(1.0, 0.5 - d / max(aa, 1e-5)))


def render_icon(size: int) -> bytes:
    buf = bytearray(size * size * 4)
    aa = 1.25 / size
    for y in range(size):
        ny = (y + 0.5) / size
        for x in range(size):
            nx = (x + 0.5) / size
            col = (0.0, 0.0, 0.0, 0.0)

            # Rounded dark slate base
            d_base = sd_rounded_box(nx, ny, 0.5, 0.5, 0.44, 0.44, 0.14)
            a_base = smoothstep_edge(d_base, aa)
            if a_base > 0.0:
                grad = 0.85 + 0.30 * (1.0 - ny)
                base_col = (
                    min(1.0, (24 / 255.0) * grad),
                    min(1.0, (28 / 255.0) * grad),
                    min(1.0, (36 / 255.0) * grad),
                    a_base,
                )
                col = blend(col, base_col)

            # Subtle steel-blue outer border
            d_ring = abs(d_base + 0.015) - 0.012
            a_ring = smoothstep_edge(d_ring, aa) * a_base
            if a_ring > 0.0:
                col = blend(col, (59 / 255.0, 130 / 255.0, 246 / 255.0, 0.55 * a_ring))

            # Layered archive slabs (representing 2048-byte sectors / directory blocks)
            slabs = [
                (0.50, 0.68, 0.27, 0.065, 0.025, (38 / 255.0, 46 / 255.0, 61 / 255.0, 1.0)),
                (0.50, 0.53, 0.27, 0.065, 0.025, (52 / 255.0, 68 / 255.0, 96 / 255.0, 1.0)),
                (0.50, 0.38, 0.27, 0.075, 0.028, (59 / 255.0, 130 / 255.0, 246 / 255.0, 1.0)),
            ]
            for cx, cy, hx, hy, rad, scol in slabs:
                d_slab = sd_rounded_box(nx, ny, cx, cy, hx, hy, rad)
                a_slab = smoothstep_edge(d_slab, aa)
                if a_slab > 0.0:
                    col = blend(col, (scol[0], scol[1], scol[2], scol[3] * a_slab))

            # Sector indicator notch on top accent bar
            d_notch = sd_rounded_box(nx, ny, 0.32, 0.38, 0.045, 0.025, 0.01)
            a_notch = smoothstep_edge(d_notch, aa)
            if a_notch > 0.0:
                col = blend(col, (226 / 255.0, 238 / 255.0, 255 / 255.0, 0.92 * a_notch))

            # Right-side data bars
            for bx in (0.44, 0.56, 0.67):
                d_bar = sd_rounded_box(nx, ny, bx, 0.38, 0.022, 0.018, 0.008)
                a_bar = smoothstep_edge(d_bar, aa)
                if a_bar > 0.0:
                    col = blend(col, (191 / 255.0, 219 / 255.0, 254 / 255.0, 0.78 * a_bar))

            idx = (y * size + x) * 4
            buf[idx + 0] = int(max(0, min(255, round(col[0] * 255.0))))
            buf[idx + 1] = int(max(0, min(255, round(col[1] * 255.0))))
            buf[idx + 2] = int(max(0, min(255, round(col[2] * 255.0))))
            buf[idx + 3] = int(max(0, min(255, round(col[3] * 255.0))))
    return bytes(buf)


def build_ico(sizes: list[int]) -> tuple[bytes, bytes]:
    pngs = []
    icon_256 = b""
    for sz in sizes:
        rgba = render_icon(sz)
        png = encode_png(sz, sz, rgba)
        pngs.append((sz, png))
        if sz == 256:
            icon_256 = png

    header = struct.pack("<HHH", 0, 1, len(pngs))
    dir_entries = bytearray()
    offset = 6 + 16 * len(pngs)
    blobs = bytearray()
    for sz, png in pngs:
        dim = 0 if sz >= 256 else sz
        dir_entries.extend(struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(png), offset))
        blobs.extend(png)
        offset += len(png)
    return header + bytes(dir_entries) + bytes(blobs), icon_256


def main() -> None:
    os.makedirs(OUT_DIR, exist_ok=True)
    ico_bytes, png256 = build_ico([16, 24, 32, 48, 64, 128, 256])
    ico_path = os.path.join(OUT_DIR, "imgstudio.ico")
    png_path = os.path.join(OUT_DIR, "imgstudio_icon.png")
    with open(ico_path, "wb") as f:
        f.write(ico_bytes)
    with open(png_path, "wb") as f:
        f.write(png256)
    print(f"Generated {ico_path} ({len(ico_bytes)} bytes) and {png_path} ({len(png256)} bytes)")


if __name__ == "__main__":
    main()
