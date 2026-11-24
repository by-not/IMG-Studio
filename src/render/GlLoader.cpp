#include "render/GlLoader.hpp"

#include <GLFW/glfw3.h>

namespace imgstudio::render::gl {

namespace {

bool g_gl_ready = false;

template <typename Fn>
bool load_proc(Fn& dst, const char* name) {
    dst = reinterpret_cast<Fn>(glfwGetProcAddress(name));
    return dst != nullptr;
}

} // namespace

bool init_gl_functions() {
    if (g_gl_ready) {
        return true;
    }

    bool ok = true;
    ok &= load_proc(GenFramebuffers, "glGenFramebuffers");
    ok &= load_proc(DeleteFramebuffers, "glDeleteFramebuffers");
    ok &= load_proc(BindFramebuffer, "glBindFramebuffer");
    ok &= load_proc(FramebufferTexture2D, "glFramebufferTexture2D");
    ok &= load_proc(CheckFramebufferStatus, "glCheckFramebufferStatus");

    ok &= load_proc(GenRenderbuffers, "glGenRenderbuffers");
    ok &= load_proc(DeleteRenderbuffers, "glDeleteRenderbuffers");
    ok &= load_proc(BindRenderbuffer, "glBindRenderbuffer");
    ok &= load_proc(RenderbufferStorage, "glRenderbufferStorage");
    ok &= load_proc(FramebufferRenderbuffer, "glFramebufferRenderbuffer");

    ok &= load_proc(GenVertexArrays, "glGenVertexArrays");
    ok &= load_proc(DeleteVertexArrays, "glDeleteVertexArrays");
    ok &= load_proc(BindVertexArray, "glBindVertexArray");

    ok &= load_proc(GenBuffers, "glGenBuffers");
    ok &= load_proc(DeleteBuffers, "glDeleteBuffers");
    ok &= load_proc(BindBuffer, "glBindBuffer");
    ok &= load_proc(BufferData, "glBufferData");

    ok &= load_proc(EnableVertexAttribArray, "glEnableVertexAttribArray");
    ok &= load_proc(VertexAttribPointer, "glVertexAttribPointer");

    ok &= load_proc(CreateShader, "glCreateShader");
    ok &= load_proc(DeleteShader, "glDeleteShader");
    ok &= load_proc(ShaderSource, "glShaderSource");
    ok &= load_proc(CompileShader, "glCompileShader");
    ok &= load_proc(GetShaderiv, "glGetShaderiv");
    ok &= load_proc(GetShaderInfoLog, "glGetShaderInfoLog");

    ok &= load_proc(CreateProgram, "glCreateProgram");
    ok &= load_proc(DeleteProgram, "glDeleteProgram");
    ok &= load_proc(AttachShader, "glAttachShader");
    ok &= load_proc(LinkProgram, "glLinkProgram");
    ok &= load_proc(GetProgramiv, "glGetProgramiv");
    ok &= load_proc(GetProgramInfoLog, "glGetProgramInfoLog");
    ok &= load_proc(UseProgram, "glUseProgram");

    ok &= load_proc(GetUniformLocation, "glGetUniformLocation");
    ok &= load_proc(Uniform1i, "glUniform1i");
    ok &= load_proc(Uniform1f, "glUniform1f");
    ok &= load_proc(Uniform3f, "glUniform3f");
    ok &= load_proc(Uniform4f, "glUniform4f");
    ok &= load_proc(UniformMatrix4fv, "glUniformMatrix4fv");

    ok &= load_proc(ActiveTexture, "glActiveTexture");
    ok &= load_proc(GenerateMipmap, "glGenerateMipmap");

    g_gl_ready = ok;
    return ok;
}

bool is_gl_ready() noexcept {
    return g_gl_ready;
}

} // namespace imgstudio::render::gl
