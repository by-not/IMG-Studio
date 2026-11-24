#include "render/ViewportFbo.hpp"

#include <algorithm>

namespace imgstudio::render {

ViewportFbo::~ViewportFbo() {
    release();
}

void ViewportFbo::release() noexcept {
    if (depth_rbo_ != 0U && gl::DeleteRenderbuffers != nullptr) {
        gl::DeleteRenderbuffers(1, &depth_rbo_);
        depth_rbo_ = 0U;
    }
    if (color_tex_ != 0U) {
        glDeleteTextures(1, &color_tex_);
        color_tex_ = 0U;
    }
    if (fbo_ != 0U && gl::DeleteFramebuffers != nullptr) {
        gl::DeleteFramebuffers(1, &fbo_);
        fbo_ = 0U;
    }
    width_ = 0;
    height_ = 0;
}

bool ViewportFbo::ensure_size(int width, int height) {
    if (!gl::is_gl_ready()) {
        return false;
    }
    width = std::clamp(width, 16, 4096);
    height = std::clamp(height, 16, 4096);
    if (fbo_ != 0U && width_ == width && height_ == height) {
        return true;
    }

    release();

    gl::GenFramebuffers(1, &fbo_);
    gl::BindFramebuffer(GL_FRAMEBUFFER, fbo_);

    glGenTextures(1, &color_tex_);
    glBindTexture(GL_TEXTURE_2D, color_tex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    gl::FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_tex_, 0);

    gl::GenRenderbuffers(1, &depth_rbo_);
    gl::BindRenderbuffer(GL_RENDERBUFFER, depth_rbo_);
    gl::RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, width, height);
    gl::FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, depth_rbo_);

    const GLenum status = gl::CheckFramebufferStatus(GL_FRAMEBUFFER);
    gl::BindFramebuffer(GL_FRAMEBUFFER, 0U);
    glBindTexture(GL_TEXTURE_2D, 0U);
    gl::BindRenderbuffer(GL_RENDERBUFFER, 0U);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        release();
        return false;
    }

    width_ = width;
    height_ = height;
    return true;
}

void ViewportFbo::bind() const {
    if (fbo_ != 0U && gl::BindFramebuffer != nullptr) {
        gl::BindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glViewport(0, 0, width_, height_);
    }
}

void ViewportFbo::unbind() const {
    if (gl::BindFramebuffer != nullptr) {
        gl::BindFramebuffer(GL_FRAMEBUFFER, 0U);
    }
}

} // namespace imgstudio::render
