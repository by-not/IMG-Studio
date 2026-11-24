#pragma once

#include "render/GlLoader.hpp"

#include <cstdint>

namespace imgstudio::render {

class ViewportFbo {
public:
    ViewportFbo() = default;
    ~ViewportFbo();

    ViewportFbo(const ViewportFbo&) = delete;
    ViewportFbo& operator=(const ViewportFbo&) = delete;

    bool ensure_size(int width, int height);
    void bind() const;
    void unbind() const;
    void release() noexcept;

    [[nodiscard]] GLuint color_texture() const noexcept { return color_tex_; }
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] bool valid() const noexcept { return fbo_ != 0U && color_tex_ != 0U; }

private:
    GLuint fbo_ = 0U;
    GLuint color_tex_ = 0U;
    GLuint depth_rbo_ = 0U;
    int width_ = 0;
    int height_ = 0;
};

} // namespace imgstudio::render
