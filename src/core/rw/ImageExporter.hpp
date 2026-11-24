#pragma once

#include "core/ImgTypes.hpp"
#include "core/rw/TxdTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace imgstudio::core::rw {

enum class ImageExportFormat : std::uint8_t {
    Png = 0,
    Tga,
    Bmp
};

[[nodiscard]] const char* image_export_extension(ImageExportFormat fmt) noexcept;

class ImageExporter {
public:
    [[nodiscard]] static Result<void> export_mip_to_file(
        const TxdMipLevel& mip,
        const std::filesystem::path& dest_file,
        ImageExportFormat format);
};

} // namespace imgstudio::core::rw
