#include "core/rw/TxdParser.hpp"
#include "core/rw/RwStream.hpp"
#include "core/rw/TxdDecoder.hpp"

#include <algorithm>
#include <fstream>

namespace imgstudio::core::rw {

namespace {

inline constexpr std::uint32_t kPlatformPcD3d8 = 8U;
inline constexpr std::uint32_t kPlatformPcD3d9 = 9U;

inline constexpr std::uint32_t kRasterFmtMask   = 0x0F00U;
inline constexpr std::uint32_t kRaster1555      = 0x0100U;
inline constexpr std::uint32_t kRaster565       = 0x0200U;
inline constexpr std::uint32_t kRaster4444      = 0x0300U;
inline constexpr std::uint32_t kRasterLum8      = 0x0400U;
inline constexpr std::uint32_t kRaster8888      = 0x0500U;
inline constexpr std::uint32_t kRaster888       = 0x0600U;
inline constexpr std::uint32_t kRaster555       = 0x0A00U;
inline constexpr std::uint32_t kRasterPal8      = 0x2000U;
inline constexpr std::uint32_t kRasterPal4      = 0x4000U;

inline constexpr std::uint32_t kFourCcDxt1 = 0x31545844U; // 'DXT1'
inline constexpr std::uint32_t kFourCcDxt3 = 0x33545844U; // 'DXT3'
inline constexpr std::uint32_t kFourCcDxt5 = 0x35545844U; // 'DXT5'

void resolve_pc_pixel_format(TxdTexture& tex, std::uint8_t flags_or_comp) {
    const std::uint32_t base_fmt = tex.raster_format_raw & kRasterFmtMask;
    const bool pal8 = (tex.raster_format_raw & kRasterPal8) != 0U;
    const bool pal4 = (tex.raster_format_raw & kRasterPal4) != 0U;

    if (tex.platform_id == kPlatformPcD3d9) {
        tex.has_alpha = (flags_or_comp & 0x01U) != 0U;
        tex.is_cubemap = (flags_or_comp & 0x02U) != 0U;
        tex.is_compressed = (flags_or_comp & 0x08U) != 0U;

        if (tex.d3d_format_raw == kFourCcDxt1) {
            tex.pixel_format = TxdPixelFormat::Dxt1;
            tex.format_name = "DXT1";
            tex.is_compressed = true;
            tex.supported = true;
            return;
        }
        if (tex.d3d_format_raw == kFourCcDxt3) {
            tex.pixel_format = TxdPixelFormat::Dxt3;
            tex.format_name = "DXT3";
            tex.is_compressed = true;
            tex.has_alpha = true;
            tex.supported = true;
            return;
        }
        if (tex.d3d_format_raw == kFourCcDxt5) {
            tex.pixel_format = TxdPixelFormat::Dxt5;
            tex.format_name = "DXT5";
            tex.is_compressed = true;
            tex.has_alpha = true;
            tex.supported = true;
            return;
        }
    } else if (tex.platform_id == kPlatformPcD3d8) {
        // In D3D8, d3d_format_raw holds hasAlpha uint32, and flags_or_comp holds compression (0,1,3,5)
        tex.has_alpha = (tex.d3d_format_raw != 0U);
        if (flags_or_comp == 1U) {
            tex.pixel_format = TxdPixelFormat::Dxt1;
            tex.format_name = "DXT1 (D3D8)";
            tex.is_compressed = true;
            tex.supported = true;
            return;
        }
        if (flags_or_comp == 3U) {
            tex.pixel_format = TxdPixelFormat::Dxt3;
            tex.format_name = "DXT3 (D3D8)";
            tex.is_compressed = true;
            tex.has_alpha = true;
            tex.supported = true;
            return;
        }
        if (flags_or_comp == 5U) {
            tex.pixel_format = TxdPixelFormat::Dxt5;
            tex.format_name = "DXT5 (D3D8)";
            tex.is_compressed = true;
            tex.has_alpha = true;
            tex.supported = true;
            return;
        }
        if (flags_or_comp != 0U) {
            tex.pixel_format = TxdPixelFormat::Unknown;
            tex.format_name = "Unknown D3D8 Compression (" + std::to_string(flags_or_comp) + ")";
            tex.unsupported_reason = "Unsupported D3D8 compression code.";
            tex.supported = false;
            return;
        }
    }

    if (pal8) {
        tex.is_paletted = true;
        tex.pixel_format = TxdPixelFormat::Pal8;
        tex.has_alpha = (base_fmt == kRaster8888 || base_fmt == kRaster4444 || base_fmt == kRaster1555);
        tex.format_name = tex.has_alpha ? "PAL8 (RGBA)" : "PAL8 (RGB)";
        tex.supported = true;
        return;
    }
    if (pal4) {
        tex.is_paletted = true;
        tex.pixel_format = TxdPixelFormat::Pal4;
        tex.has_alpha = (base_fmt == kRaster8888 || base_fmt == kRaster4444 || base_fmt == kRaster1555);
        tex.format_name = tex.has_alpha ? "PAL4 (RGBA)" : "PAL4 (RGB)";
        tex.supported = true;
        return;
    }

    if (tex.platform_id == kPlatformPcD3d9 && tex.is_compressed) {
        tex.pixel_format = TxdPixelFormat::Unknown;
        tex.format_name = "Unsupported Compressed D3D9";
        tex.unsupported_reason = "Unknown D3D9 FourCC compression format.";
        tex.supported = false;
        return;
    }

    switch (base_fmt) {
        case kRaster8888:
            tex.pixel_format = TxdPixelFormat::Bgra8888;
            tex.format_name = "RASTER_8888 (BGRA32)";
            tex.has_alpha = true;
            tex.supported = true;
            break;
        case kRaster888:
            tex.pixel_format = (tex.depth == 24U) ? TxdPixelFormat::Bgr888 : TxdPixelFormat::Bgrx8888;
            tex.format_name = (tex.depth == 24U) ? "RASTER_888 (BGR24)" : "RASTER_888 (BGRX32)";
            tex.has_alpha = false;
            tex.supported = true;
            break;
        case kRaster1555:
            tex.pixel_format = TxdPixelFormat::Bgra5551;
            tex.format_name = "RASTER_1555 (BGRA5551)";
            tex.has_alpha = true;
            tex.supported = true;
            break;
        case kRaster555:
            tex.pixel_format = TxdPixelFormat::Bgrx5551;
            tex.format_name = "RASTER_555 (BGRX5551)";
            tex.has_alpha = false;
            tex.supported = true;
            break;
        case kRaster565:
            tex.pixel_format = TxdPixelFormat::Bgr565;
            tex.format_name = "RASTER_565 (BGR565)";
            tex.has_alpha = false;
            tex.supported = true;
            break;
        case kRaster4444:
            tex.pixel_format = TxdPixelFormat::Bgra4444;
            tex.format_name = "RASTER_4444 (BGRA4444)";
            tex.has_alpha = true;
            tex.supported = true;
            break;
        case kRasterLum8:
            tex.pixel_format = TxdPixelFormat::Lum8;
            tex.format_name = "RASTER_LUM8";
            tex.has_alpha = false;
            tex.supported = true;
            break;
        default:
            tex.pixel_format = TxdPixelFormat::Unknown;
            tex.format_name = "Unsupported Raster";
            tex.unsupported_reason = "Unsupported RenderWare raster format flags.";
            tex.supported = false;
            break;
    }
}

TxdTexture parse_texture_native(RwCursor tcur, std::vector<TxdWarning>& warnings) {
    TxdTexture tex;
    RwChunkHeader shdr{};
    auto scur = tcur.read_chunk_payload(shdr);
    if (!scur.has_value() || shdr.type != kRwStruct) {
        warnings.push_back(TxdWarning{true, {}, "TextureNative chunk missing Struct payload."});
        tex.unsupported_reason = "Missing Struct chunk.";
        return tex;
    }

    if (!scur->read_u32(tex.platform_id) ||
        !scur->read_u32(tex.filter_flags) ||
        !scur->read_fixed_string(32U, tex.name) ||
        !scur->read_fixed_string(32U, tex.mask_name) ||
        !scur->read_u32(tex.raster_format_raw) ||
        !scur->read_u32(tex.d3d_format_raw) ||
        !scur->read_u16(tex.width) ||
        !scur->read_u16(tex.height) ||
        !scur->read_u8(tex.depth) ||
        !scur->read_u8(tex.mipmap_count_declared) ||
        !scur->read_u8(tex.raster_type)) {
        warnings.push_back(TxdWarning{true, tex.name, "Truncated TextureNative 88-byte header."});
        tex.unsupported_reason = "Truncated TextureNative header.";
        return tex;
    }

    std::uint8_t flags_or_comp = 0;
    if (!scur->read_u8(flags_or_comp)) {
        warnings.push_back(TxdWarning{true, tex.name, "Truncated TextureNative flags byte."});
        tex.unsupported_reason = "Truncated TextureNative header.";
        return tex;
    }

    if (tex.platform_id != kPlatformPcD3d9 && tex.platform_id != kPlatformPcD3d8) {
        tex.format_name = "Platform #" + std::to_string(tex.platform_id);
        tex.supported = false;
        tex.unsupported_reason = "Unsupported non-PC TextureNative platform ID (" + std::to_string(tex.platform_id) + ").";
        warnings.push_back(TxdWarning{false, tex.name, tex.unsupported_reason});
        return tex;
    }

    resolve_pc_pixel_format(tex, flags_or_comp);
    if (!tex.supported) {
        warnings.push_back(TxdWarning{false, tex.name, tex.unsupported_reason});
        return tex;
    }

    if (tex.width == 0U || tex.height == 0U || tex.width > 16384U || tex.height > 16384U) {
        tex.supported = false;
        tex.unsupported_reason = "Invalid or out-of-range texture dimensions.";
        warnings.push_back(TxdWarning{true, tex.name, tex.unsupported_reason});
        return tex;
    }

    std::span<const std::uint8_t> palette_span;
    if (tex.pixel_format == TxdPixelFormat::Pal8) {
        auto p = scur->read_span(256U * 4U);
        if (!p.has_value()) {
            tex.supported = false;
            tex.unsupported_reason = "Truncated PAL8 1024-byte palette.";
            warnings.push_back(TxdWarning{true, tex.name, tex.unsupported_reason});
            return tex;
        }
        palette_span = *p;
    } else if (tex.pixel_format == TxdPixelFormat::Pal4) {
        auto p = scur->read_span(16U * 4U);
        if (!p.has_value()) {
            tex.supported = false;
            tex.unsupported_reason = "Truncated PAL4 64-byte palette.";
            warnings.push_back(TxdWarning{true, tex.name, tex.unsupported_reason});
            return tex;
        }
        palette_span = *p;
    }

    const std::uint8_t levels = std::max<std::uint8_t>(1U, tex.mipmap_count_declared);
    std::uint16_t mw = tex.width;
    std::uint16_t mh = tex.height;

    for (std::uint8_t lvl = 0; lvl < levels; ++lvl) {
        std::uint32_t data_size = 0;
        if (!scur->read_u32(data_size)) {
            if (lvl == 0U) {
                tex.supported = false;
                tex.unsupported_reason = "Missing base mipmap data size.";
            }
            warnings.push_back(TxdWarning{lvl == 0U, tex.name, "Truncated mipmap size header at level " + std::to_string(lvl)});
            break;
        }
        auto raw_opt = scur->read_span(static_cast<std::size_t>(data_size));
        if (!raw_opt.has_value()) {
            if (lvl == 0U) {
                tex.supported = false;
                tex.unsupported_reason = "Truncated base mipmap pixel data.";
            }
            warnings.push_back(TxdWarning{lvl == 0U, tex.name, "Truncated mipmap pixel payload at level " + std::to_string(lvl)});
            break;
        }

        auto dec = decode_txd_mip_to_rgba8(
            tex.pixel_format,
            mw,
            mh,
            tex.depth,
            tex.has_alpha,
            *raw_opt,
            palette_span);
        if (!dec.ok()) {
            if (lvl == 0U) {
                tex.supported = false;
                tex.unsupported_reason = dec.error->message;
            }
            warnings.push_back(TxdWarning{lvl == 0U, tex.name, dec.error->message});
            break;
        }

        tex.mipmaps.push_back(TxdMipLevel{mw, mh, std::move(*dec.value)});
        mw = static_cast<std::uint16_t>(std::max<int>(1, static_cast<int>(mw) / 2));
        mh = static_cast<std::uint16_t>(std::max<int>(1, static_cast<int>(mh) / 2));
    }

    if (tex.mipmaps.empty()) {
        tex.supported = false;
        if (tex.unsupported_reason.empty()) {
            tex.unsupported_reason = "No decodable mipmap levels.";
        }
    }

    return tex;
}

} // namespace

Result<TxdDictionary> TxdParser::parse_bytes(
    std::span<const std::uint8_t> bytes,
    std::string source_name) {
    if (bytes.size() < kRwChunkHeaderSize) {
        return Result<TxdDictionary>::failure("File is too small to contain a RenderWare TXD header.");
    }

    RwCursor root(bytes);
    RwChunkHeader top_hdr{};
    auto top_cur = root.read_chunk_payload(top_hdr);
    if (!top_cur.has_value() || top_hdr.type != kRwTexDictionary) {
        return Result<TxdDictionary>::failure("Invalid or truncated TexDictionary (0x16) root chunk.");
    }

    TxdDictionary dict;
    dict.source_name = std::move(source_name);
    dict.file_size_bytes = bytes.size();
    dict.rw_version = top_hdr.rw_version;

    RwChunkHeader shdr{};
    auto scur = top_cur->read_chunk_payload(shdr);
    if (!scur.has_value() || shdr.type != kRwStruct) {
        return Result<TxdDictionary>::failure("TexDictionary missing Struct header.");
    }

    if (shdr.size >= 4U) {
        (void)scur->read_u16(dict.declared_count);
        (void)scur->read_u16(dict.device_id);
    } else if (shdr.size >= 2U) {
        (void)scur->read_u16(dict.declared_count);
    }

    while (!top_cur->at_end()) {
        RwChunkHeader ch{};
        auto child = top_cur->read_chunk_payload(ch);
        if (!child.has_value()) {
            dict.warnings.push_back(TxdWarning{true, {}, "Truncated child chunk inside TexDictionary."});
            break;
        }
        if (ch.type == kRwTextureNative) {
            dict.textures.push_back(parse_texture_native(*child, dict.warnings));
        }
    }

    if (dict.declared_count != dict.textures.size()) {
        dict.warnings.push_back(TxdWarning{
            false,
            {},
            "Declared texture count (" + std::to_string(dict.declared_count) +
                ") differs from parsed TextureNative chunks (" + std::to_string(dict.textures.size()) + ")."
        });
    }

    return Result<TxdDictionary>::success(std::move(dict));
}

Result<TxdDictionary> TxdParser::parse_file(const std::filesystem::path& path) {
    std::error_code ec;
    const std::uint64_t fsize = std::filesystem::file_size(path, ec);
    if (ec) {
        return Result<TxdDictionary>::failure("Failed to stat TXD file.", ec.message());
    }
    if (fsize > 256ULL * 1024ULL * 1024ULL) {
        return Result<TxdDictionary>::failure("TXD file exceeds 256 MiB safety limit.");
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return Result<TxdDictionary>::failure("Failed to open TXD file.", path.string());
    }

    std::vector<std::uint8_t> buf(static_cast<std::size_t>(fsize));
    if (fsize > 0ULL) {
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(fsize));
        if (!in || static_cast<std::uint64_t>(in.gcount()) != fsize) {
            return Result<TxdDictionary>::failure("Failed to read complete TXD file.");
        }
    }

    return parse_bytes(buf, path.filename().string());
}

} // namespace imgstudio::core::rw
