#include "render/GpuTexture.hpp"

#include <utility>

namespace imgstudio::render {

GpuTexture::~GpuTexture() {
    release();
}

GpuTexture::GpuTexture(GpuTexture&& other) noexcept
    : tex_id_(std::exchange(other.tex_id_, 0U))
    , width_(std::exchange(other.width_, 0U))
    , height_(std::exchange(other.height_, 0U)) {}

GpuTexture& GpuTexture::operator=(GpuTexture&& other) noexcept {
    if (this != &other) {
        release();
        tex_id_ = std::exchange(other.tex_id_, 0U);
        width_ = std::exchange(other.width_, 0U);
        height_ = std::exchange(other.height_, 0U);
    }
    return *this;
}

void GpuTexture::release() noexcept {
    if (tex_id_ != 0U) {
        glDeleteTextures(1, &tex_id_);
        tex_id_ = 0U;
    }
    width_ = 0U;
    height_ = 0U;
}

bool GpuTexture::upload_rgba8(
    std::uint16_t width,
    std::uint16_t height,
    const std::uint8_t* rgba8_data,
    TextureFilterMode filter,
    bool generate_mipmaps) {
    if (width == 0U || height == 0U || rgba8_data == nullptr) {
        return false;
    }

    if (tex_id_ == 0U) {
        glGenTextures(1, &tex_id_);
    }
    glBindTexture(GL_TEXTURE_2D, tex_id_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        static_cast<GLsizei>(width),
        static_cast<GLsizei>(height),
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        rgba8_data);

    if (generate_mipmaps && gl::GenerateMipmap != nullptr) {
        gl::GenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MIN_FILTER,
            filter == TextureFilterMode::Linear ? GL_LINEAR_MIPMAP_LINEAR : GL_NEAREST_MIPMAP_NEAREST);
    } else {
        glTexParameteri(
            GL_TEXTURE_2D,
            GL_TEXTURE_MIN_FILTER,
            filter == TextureFilterMode::Linear ? GL_LINEAR : GL_NEAREST);
    }
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        filter == TextureFilterMode::Linear ? GL_LINEAR : GL_NEAREST);

    glBindTexture(GL_TEXTURE_2D, 0U);
    width_ = width;
    height_ = height;
    return true;
}

bool GpuTexture::upload_mip(
    const core::rw::TxdMipLevel& mip,
    TextureFilterMode filter,
    bool generate_mipmaps) {
    if (mip.rgba8.size() != static_cast<std::size_t>(mip.width) * mip.height * 4U) {
        return false;
    }
    return upload_rgba8(mip.width, mip.height, mip.rgba8.data(), filter, generate_mipmaps);
}

void GpuTexture::set_filter(TextureFilterMode filter) {
    if (tex_id_ == 0U) return;
    glBindTexture(GL_TEXTURE_2D, tex_id_);
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        filter == TextureFilterMode::Linear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(
        GL_TEXTURE_2D,
        GL_TEXTURE_MAG_FILTER,
        filter == TextureFilterMode::Linear ? GL_LINEAR : GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0U);
}

} // namespace imgstudio::render
