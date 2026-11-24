#include "core/rw/TxdDecoder.hpp"
#include "core/BinaryIO.hpp"

#include <algorithm>
#include <array>
#include <cstdint>

namespace imgstudio::core::rw {

namespace {

void decode_rgb565(std::uint16_t c, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) noexcept {
    const std::uint8_t r5 = static_cast<std::uint8_t>((c >> 11U) & 0x1FU);
    const std::uint8_t g6 = static_cast<std::uint8_t>((c >> 5U) & 0x3FU);
    const std::uint8_t b5 = static_cast<std::uint8_t>(c & 0x1FU);
    r = static_cast<std::uint8_t>((r5 << 3U) | (r5 >> 2U));
    g = static_cast<std::uint8_t>((g6 << 2U) | (g6 >> 4U));
    b = static_cast<std::uint8_t>((b5 << 3U) | (b5 >> 2U));
}

void decode_bc1_color_block(
    std::span<const std::uint8_t> block,
    bool transparent_mode_allowed,
    std::array<std::uint8_t, 16 * 4>& out_rgba) noexcept {
    const std::uint16_t c0 = read_u16_le(std::span<const std::uint8_t, 2>(block.data(), 2U));
    const std::uint16_t c1 = read_u16_le(std::span<const std::uint8_t, 2>(block.data() + 2U, 2U));
    const std::uint32_t lookup = read_u32_le(std::span<const std::uint8_t, 4>(block.data() + 4U, 4U));

    std::array<std::array<std::uint8_t, 4>, 4> pal{};
    decode_rgb565(c0, pal[0][0], pal[0][1], pal[0][2]);
    pal[0][3] = 255U;
    decode_rgb565(c1, pal[1][0], pal[1][1], pal[1][2]);
    pal[1][3] = 255U;

    if (!transparent_mode_allowed || c0 > c1) {
        for (int ch = 0; ch < 3; ++ch) {
            pal[2][static_cast<std::size_t>(ch)] = static_cast<std::uint8_t>(
                (2U * pal[0][static_cast<std::size_t>(ch)] + pal[1][static_cast<std::size_t>(ch)] + 1U) / 3U);
            pal[3][static_cast<std::size_t>(ch)] = static_cast<std::uint8_t>(
                (pal[0][static_cast<std::size_t>(ch)] + 2U * pal[1][static_cast<std::size_t>(ch)] + 1U) / 3U);
        }
        pal[2][3] = 255U;
        pal[3][3] = 255U;
    } else {
        for (int ch = 0; ch < 3; ++ch) {
            pal[2][static_cast<std::size_t>(ch)] = static_cast<std::uint8_t>(
                (pal[0][static_cast<std::size_t>(ch)] + pal[1][static_cast<std::size_t>(ch)]) / 2U);
            pal[3][static_cast<std::size_t>(ch)] = 0U;
        }
        pal[2][3] = 255U;
        pal[3][3] = 0U;
    }

    for (std::size_t i = 0; i < 16U; ++i) {
        const std::size_t code = (lookup >> (i * 2U)) & 0x3U;
        out_rgba[i * 4U + 0U] = pal[code][0];
        out_rgba[i * 4U + 1U] = pal[code][1];
        out_rgba[i * 4U + 2U] = pal[code][2];
        out_rgba[i * 4U + 3U] = pal[code][3];
    }
}

void write_block_to_image(
    const std::array<std::uint8_t, 16 * 4>& block_rgba,
    std::uint16_t width,
    std::uint16_t height,
    std::uint32_t bx,
    std::uint32_t by,
    std::vector<std::uint8_t>& dst_rgba) noexcept {
    for (std::uint32_t py = 0; py < 4U; ++py) {
        const std::uint32_t iy = by * 4U + py;
        if (iy >= height) continue;
        for (std::uint32_t px = 0; px < 4U; ++px) {
            const std::uint32_t ix = bx * 4U + px;
            if (ix >= width) continue;
            const std::size_t src_i = (static_cast<std::size_t>(py) * 4U + px) * 4U;
            const std::size_t dst_i = (static_cast<std::size_t>(iy) * width + ix) * 4U;
            dst_rgba[dst_i + 0U] = block_rgba[src_i + 0U];
            dst_rgba[dst_i + 1U] = block_rgba[src_i + 1U];
            dst_rgba[dst_i + 2U] = block_rgba[src_i + 2U];
            dst_rgba[dst_i + 3U] = block_rgba[src_i + 3U];
        }
    }
}

} // namespace

std::size_t compute_mip_byte_size(
    TxdPixelFormat fmt,
    std::uint16_t width,
    std::uint16_t height,
    std::uint8_t depth) {
    const std::size_t w = width;
    const std::size_t h = height;
    const std::size_t bw = std::max<std::size_t>(1U, (w + 3U) / 4U);
    const std::size_t bh = std::max<std::size_t>(1U, (h + 3U) / 4U);

    switch (fmt) {
        case TxdPixelFormat::Dxt1:
            return bw * bh * 8U;
        case TxdPixelFormat::Dxt3:
        case TxdPixelFormat::Dxt5:
            return bw * bh * 16U;
        case TxdPixelFormat::Bgra8888:
        case TxdPixelFormat::Bgrx8888:
            return w * h * 4U;
        case TxdPixelFormat::Bgr888:
            return w * h * (depth == 24U ? 3U : 4U);
        case TxdPixelFormat::Bgra4444:
        case TxdPixelFormat::Bgra5551:
        case TxdPixelFormat::Bgrx5551:
        case TxdPixelFormat::Bgr565:
            return w * h * 2U;
        case TxdPixelFormat::Lum8:
        case TxdPixelFormat::Pal8:
            return w * h;
        case TxdPixelFormat::Pal4:
            return (w * h + 1U) / 2U;
        case TxdPixelFormat::Unknown:
            return 0U;
    }
    return 0U;
}

Result<std::vector<std::uint8_t>> decode_txd_mip_to_rgba8(
    TxdPixelFormat fmt,
    std::uint16_t width,
    std::uint16_t height,
    std::uint8_t depth,
    bool has_alpha,
    std::span<const std::uint8_t> raw_pixels,
    std::span<const std::uint8_t> palette_rgba) {
    if (width == 0U || height == 0U) {
        return Result<std::vector<std::uint8_t>>::failure("Texture mipmap has zero dimension.");
    }

    const std::size_t expected = compute_mip_byte_size(fmt, width, height, depth);
    if (expected == 0U || raw_pixels.size() < expected) {
        return Result<std::vector<std::uint8_t>>::failure(
            "Truncated texture pixel buffer for mipmap level.");
    }

    const std::size_t pixel_count = static_cast<std::size_t>(width) * height;
    std::vector<std::uint8_t> out(pixel_count * 4U, 255U);

    const std::uint32_t bw = std::max<std::uint32_t>(1U, (width + 3U) / 4U);
    const std::uint32_t bh = std::max<std::uint32_t>(1U, (height + 3U) / 4U);

    switch (fmt) {
        case TxdPixelFormat::Dxt1: {
            std::array<std::uint8_t, 16 * 4> blk{};
            std::size_t off = 0;
            for (std::uint32_t by = 0; by < bh; ++by) {
                for (std::uint32_t bx = 0; bx < bw; ++bx) {
                    decode_bc1_color_block(raw_pixels.subspan(off, 8U), true, blk);
                    if (!has_alpha) {
                        for (std::size_t p = 0; p < 16U; ++p) {
                            if (blk[p * 4U + 3U] == 0U) {
                                blk[p * 4U + 3U] = 255U;
                            }
                        }
                    }
                    write_block_to_image(blk, width, height, bx, by, out);
                    off += 8U;
                }
            }
            return Result<std::vector<std::uint8_t>>::success(std::move(out));
        }

        case TxdPixelFormat::Dxt3: {
            std::array<std::uint8_t, 16 * 4> blk{};
            std::size_t off = 0;
            for (std::uint32_t by = 0; by < bh; ++by) {
                for (std::uint32_t bx = 0; bx < bw; ++bx) {
                    const auto alpha_span = raw_pixels.subspan(off, 8U);
                    decode_bc1_color_block(raw_pixels.subspan(off + 8U, 8U), false, blk);
                    for (std::size_t p = 0; p < 16U; ++p) {
                        const std::uint8_t byte_val = alpha_span[p / 2U];
                        const std::uint8_t a4 = (p & 1U) == 0U
                            ? static_cast<std::uint8_t>(byte_val & 0x0FU)
                            : static_cast<std::uint8_t>((byte_val >> 4U) & 0x0FU);
                        blk[p * 4U + 3U] = static_cast<std::uint8_t>((a4 << 4U) | a4);
                    }
                    write_block_to_image(blk, width, height, bx, by, out);
                    off += 16U;
                }
            }
            return Result<std::vector<std::uint8_t>>::success(std::move(out));
        }

        case TxdPixelFormat::Dxt5: {
            std::array<std::uint8_t, 16 * 4> blk{};
            std::size_t off = 0;
            for (std::uint32_t by = 0; by < bh; ++by) {
                for (std::uint32_t bx = 0; bx < bw; ++bx) {
                    const std::uint8_t a0 = raw_pixels[off];
                    const std::uint8_t a1 = raw_pixels[off + 1U];
                    std::uint64_t bits = 0;
                    for (int k = 0; k < 6; ++k) {
                        bits |= (static_cast<std::uint64_t>(raw_pixels[off + 2U + static_cast<std::size_t>(k)]) << (8 * k));
                    }

                    std::array<std::uint8_t, 8> apal{};
                    apal[0] = a0;
                    apal[1] = a1;
                    if (a0 > a1) {
                        for (std::size_t i = 1; i <= 6U; ++i) {
                            apal[i + 1U] = static_cast<std::uint8_t>(((7U - i) * a0 + i * a1 + 3U) / 7U);
                        }
                    } else {
                        for (std::size_t i = 1; i <= 4U; ++i) {
                            apal[i + 1U] = static_cast<std::uint8_t>(((5U - i) * a0 + i * a1 + 2U) / 5U);
                        }
                        apal[6] = 0U;
                        apal[7] = 255U;
                    }

                    decode_bc1_color_block(raw_pixels.subspan(off + 8U, 8U), false, blk);
                    for (std::size_t p = 0; p < 16U; ++p) {
                        const std::size_t idx = static_cast<std::size_t>((bits >> (p * 3U)) & 0x7ULL);
                        blk[p * 4U + 3U] = apal[idx];
                    }
                    write_block_to_image(blk, width, height, bx, by, out);
                    off += 16U;
                }
            }
            return Result<std::vector<std::uint8_t>>::success(std::move(out));
        }

        case TxdPixelFormat::Bgra8888: {
            for (std::size_t i = 0; i < pixel_count; ++i) {
                const std::size_t s = i * 4U;
                out[s + 0U] = raw_pixels[s + 2U];
                out[s + 1U] = raw_pixels[s + 1U];
                out[s + 2U] = raw_pixels[s + 0U];
                out[s + 3U] = has_alpha ? raw_pixels[s + 3U] : 255U;
            }
            return Result<std::vector<std::uint8_t>>::success(std::move(out));
        }

        case TxdPixelFormat::Bgrx8888: {
            for (std::size_t i = 0; i < pixel_count; ++i) {
                const std::size_t s = i * 4U;
                out[s + 0U] = raw_pixels[s + 2U];
                out[s + 1U] = raw_pixels[s + 1U];
                out[s + 2U] = raw_pixels[s + 0U];
                out[s + 3U] = 255U;
            }
            return Result<std::vector<std::uint8_t>>::success(std::move(out));
        }

        case TxdPixelFormat::Bgr888: {
            const std::size_t stride = (depth == 24U) ? 3U : 4U;
            for (std::size_t i = 0; i < pixel_count; ++i) {
                const std::size_t s = i * stride;
                const std::size_t d = i * 4U;
                out[d + 0U] = raw_pixels[s + 2U];
                out[d + 1U] = raw_pixels[s + 1U];
                out[d + 2U] = raw_pixels[s + 0U];
                out[d + 3U] = 255U;
            }
            return Result<std::vector<std::uint8_t>>::success(std::move(out));
        }

        case TxdPixelFormat::Bgra4444: {
            for (std::size_t i = 0; i < pixel_count; ++i) {
                const std::uint16_t v = read_u16_le(
                    std::span<const std::uint8_t, 2>(raw_pixels.data() + i * 2U, 2U));
                const std::uint8_t b = static_cast<std::uint8_t>(v & 0x0FU);
                const std::uint8_t g = static_cast<std::uint8_t>((v >> 4U) & 0x0FU);
                const std::uint8_t r = static_cast<std::uint8_t>((v >> 8U) & 0x0FU);
                const std::uint8_t a = static_cast<std::uint8_t>((v >> 12U) & 0x0FU);
                out[i * 4U + 0U] = static_cast<std::uint8_t>((r << 4U) | r);
                out[i * 4U + 1U] = static_cast<std::uint8_t>((g << 4U) | g);
                out[i * 4U + 2U] = static_cast<std::uint8_t>((b << 4U) | b);
                out[i * 4U + 3U] = static_cast<std::uint8_t>((a << 4U) | a);
            }
            return Result<std::vector<std::uint8_t>>::success(std::move(out));
        }

        case TxdPixelFormat::Bgra5551:
        case TxdPixelFormat::Bgrx5551: {
            const bool use_alpha = (fmt == TxdPixelFormat::Bgra5551);
            for (std::size_t i = 0; i < pixel_count; ++i) {
                const std::uint16_t v = read_u16_le(
                    std::span<const std::uint8_t, 2>(raw_pixels.data() + i * 2U, 2U));
                const std::uint8_t b5 = static_cast<std::uint8_t>(v & 0x1FU);
                const std::uint8_t g5 = static_cast<std::uint8_t>((v >> 5U) & 0x1FU);
                const std::uint8_t r5 = static_cast<std::uint8_t>((v >> 10U) & 0x1FU);
                const bool a1 = ((v >> 15U) & 0x1U) != 0U;
                out[i * 4U + 0U] = static_cast<std::uint8_t>((r5 << 3U) | (r5 >> 2U));
                out[i * 4U + 1U] = static_cast<std::uint8_t>((g5 << 3U) | (g5 >> 2U));
                out[i * 4U + 2U] = static_cast<std::uint8_t>((b5 << 3U) | (b5 >> 2U));
                out[i * 4U + 3U] = use_alpha ? (a1 ? 255U : 0U) : 255U;
            }
            return Result<std::vector<std::uint8_t>>::success(std::move(out));
        }

        case TxdPixelFormat::Bgr565: {
            for (std::size_t i = 0; i < pixel_count; ++i) {
                const std::uint16_t v = read_u16_le(
                    std::span<const std::uint8_t, 2>(raw_pixels.data() + i * 2U, 2U));
                decode_rgb565(v, out[i * 4U + 0U], out[i * 4U + 1U], out[i * 4U + 2U]);
                out[i * 4U + 3U] = 255U;
            }
            return Result<std::vector<std::uint8_t>>::success(std::move(out));
        }

        case TxdPixelFormat::Lum8: {
            for (std::size_t i = 0; i < pixel_count; ++i) {
                const std::uint8_t l = raw_pixels[i];
                out[i * 4U + 0U] = l;
                out[i * 4U + 1U] = l;
                out[i * 4U + 2U] = l;
                out[i * 4U + 3U] = 255U;
            }
            return Result<std::vector<std::uint8_t>>::success(std::move(out));
        }

        case TxdPixelFormat::Pal8: {
            if (palette_rgba.size() < 256U * 4U) {
                return Result<std::vector<std::uint8_t>>::failure("Missing 256-entry palette for PAL8 texture.");
            }
            for (std::size_t i = 0; i < pixel_count; ++i) {
                const std::size_t idx = static_cast<std::size_t>(raw_pixels[i]) * 4U;
                out[i * 4U + 0U] = palette_rgba[idx + 0U];
                out[i * 4U + 1U] = palette_rgba[idx + 1U];
                out[i * 4U + 2U] = palette_rgba[idx + 2U];
                out[i * 4U + 3U] = has_alpha ? palette_rgba[idx + 3U] : 255U;
            }
            return Result<std::vector<std::uint8_t>>::success(std::move(out));
        }

        case TxdPixelFormat::Pal4: {
            if (palette_rgba.size() < 16U * 4U) {
                return Result<std::vector<std::uint8_t>>::failure("Missing 16-entry palette for PAL4 texture.");
            }
            for (std::size_t i = 0; i < pixel_count; ++i) {
                const std::uint8_t byte_val = raw_pixels[i / 2U];
                const std::size_t pal_idx = (i & 1U) == 0U
                    ? static_cast<std::size_t>(byte_val & 0x0FU)
                    : static_cast<std::size_t>((byte_val >> 4U) & 0x0FU);
                const std::size_t p = pal_idx * 4U;
                out[i * 4U + 0U] = palette_rgba[p + 0U];
                out[i * 4U + 1U] = palette_rgba[p + 1U];
                out[i * 4U + 2U] = palette_rgba[p + 2U];
                out[i * 4U + 3U] = has_alpha ? palette_rgba[p + 3U] : 255U;
            }
            return Result<std::vector<std::uint8_t>>::success(std::move(out));
        }

        case TxdPixelFormat::Unknown:
            break;
    }

    return Result<std::vector<std::uint8_t>>::failure("Unsupported TXD pixel format.");
}

} // namespace imgstudio::core::rw
