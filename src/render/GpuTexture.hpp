#pragma once

#include "core/rw/TxdTypes.hpp"
#include "render/GlLoader.hpp"

#include <cstdint>

namespace imgstudio::render {

enum class TextureFilterMode : std::uint8_t {
    Linear = 0,
    Nearest
};

class GpuTexture {
public:
    GpuTexture() = default;
    ~GpuTexture();

    GpuTexture(const GpuTexture&) = delete;
    GpuTexture& operator=(const GpuTexture&) = delete;
    GpuTexture(GpuTexture&& other) noexcept;
    GpuTexture& operator=(GpuTexture&& other) noexcept;

    bool upload_rgba8(
        std::uint16_t width,
        std::uint16_t height,
        const std::uint8_t* rgba8_data,
        TextureFilterMode filter = TextureFilterMode::Linear,
        bool generate_mipmaps = false);

    bool upload_mip(
        const core::rw::TxdMipLevel& mip,
        TextureFilterMode filter = TextureFilterMode::Linear,
        bool generate_mipmaps = false);

    void set_filter(TextureFilterMode filter);
    void release() noexcept;

    [[nodiscard]] GLuint id() const noexcept { return tex_id_; }
    [[nodiscard]] bool valid() const noexcept { return tex_id_ != 0U; }
    [[nodiscard]] std::uint16_t width() const noexcept { return width_; }
    [[nodiscard]] std::uint16_t height() const noexcept { return height_; }

private:
    GLuint tex_id_ = 0U;
    std::uint16_t width_ = 0U;
    std::uint16_t height_ = 0U;
};

} // namespace imgstudio::render
