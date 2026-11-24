#include "core/rw/ImageExporter.hpp"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <fstream>
#include <vector>

namespace imgstudio::core::rw {

namespace {

void write_callback(void* context, void* data, int size) {
    auto* buf = static_cast<std::vector<std::uint8_t>*>(context);
    const auto* bytes = static_cast<const std::uint8_t*>(data);
    if (bytes && size > 0) {
        buf->insert(buf->end(), bytes, bytes + size);
    }
}

} // namespace

const char* image_export_extension(ImageExportFormat fmt) noexcept {
    switch (fmt) {
        case ImageExportFormat::Png: return ".png";
        case ImageExportFormat::Tga: return ".tga";
        case ImageExportFormat::Bmp: return ".bmp";
    }
    return ".png";
}

Result<void> ImageExporter::export_mip_to_file(
    const TxdMipLevel& mip,
    const std::filesystem::path& dest_file,
    ImageExportFormat format) {
    if (mip.width == 0U || mip.height == 0U ||
        mip.rgba8.size() != static_cast<std::size_t>(mip.width) * mip.height * 4U) {
        return Result<void>::failure("Invalid mipmap dimensions or RGBA8 buffer size.");
    }

    std::vector<std::uint8_t> encoded;
    encoded.reserve(static_cast<std::size_t>(mip.width) * mip.height * 4U);
    int ok = 0;
    const int w = static_cast<int>(mip.width);
    const int h = static_cast<int>(mip.height);

    switch (format) {
        case ImageExportFormat::Png:
            ok = stbi_write_png_to_func(write_callback, &encoded, w, h, 4, mip.rgba8.data(), w * 4);
            break;
        case ImageExportFormat::Tga:
            ok = stbi_write_tga_to_func(write_callback, &encoded, w, h, 4, mip.rgba8.data());
            break;
        case ImageExportFormat::Bmp:
            ok = stbi_write_bmp_to_func(write_callback, &encoded, w, h, 4, mip.rgba8.data());
            break;
    }

    if (ok == 0 || encoded.empty()) {
        return Result<void>::failure("Failed to encode image bytes for export.");
    }

    std::ofstream out(dest_file, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return Result<void>::failure("Failed to open output image file for writing.", dest_file.string());
    }
    out.write(reinterpret_cast<const char*>(encoded.data()), static_cast<std::streamsize>(encoded.size()));
    out.flush();
    if (!out) {
        return Result<void>::failure("Write error while saving exported image.", dest_file.string());
    }
    return Result<void>::success();
}

} // namespace imgstudio::core::rw
