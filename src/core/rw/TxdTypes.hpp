#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace imgstudio::core::rw {

enum class TxdPixelFormat : std::uint8_t {
    Unknown = 0,
    Dxt1,
    Dxt3,
    Dxt5,
    Bgra8888,
    Bgrx8888,
    Bgr888,
    Bgra4444,
    Bgra5551,
    Bgrx5551,
    Bgr565,
    Lum8,
    Pal8,
    Pal4
};

struct TxdMipLevel {
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::vector<std::uint8_t> rgba8; // Tightly packed RGBA8 (width * height * 4)
};

struct TxdTexture {
    std::string name;
    std::string mask_name;

    std::uint32_t platform_id = 0;
    std::uint32_t filter_flags = 0;
    std::uint32_t raster_format_raw = 0;
    std::uint32_t d3d_format_raw = 0;

    TxdPixelFormat pixel_format = TxdPixelFormat::Unknown;
    std::string format_name;

    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint8_t depth = 0;
    std::uint8_t mipmap_count_declared = 0;
    std::uint8_t raster_type = 0;

    bool has_alpha = false;
    bool is_compressed = false;
    bool is_cubemap = false;
    bool is_paletted = false;
    bool supported = false;
    std::string unsupported_reason;

    std::vector<TxdMipLevel> mipmaps;
};

struct TxdWarning {
    bool is_error = false;
    std::string texture_name;
    std::string message;
};

struct TxdDictionary {
    std::string source_name;
    std::uint64_t file_size_bytes = 0;
    std::uint32_t rw_version = 0;
    std::uint16_t declared_count = 0;
    std::uint16_t device_id = 0;
    std::vector<TxdTexture> textures;
    std::vector<TxdWarning> warnings;
};

} // namespace imgstudio::core::rw
