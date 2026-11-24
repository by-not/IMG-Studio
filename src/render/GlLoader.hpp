#pragma once

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <GL/gl.h>
#include <GL/glext.h>

namespace imgstudio::render::gl {

bool init_gl_functions();
bool is_gl_ready() noexcept;

inline PFNGLGENFRAMEBUFFERSPROC GenFramebuffers = nullptr;
inline PFNGLDELETEFRAMEBUFFERSPROC DeleteFramebuffers = nullptr;
inline PFNGLBINDFRAMEBUFFERPROC BindFramebuffer = nullptr;
inline PFNGLFRAMEBUFFERTEXTURE2DPROC FramebufferTexture2D = nullptr;
inline PFNGLCHECKFRAMEBUFFERSTATUSPROC CheckFramebufferStatus = nullptr;

inline PFNGLGENRENDERBUFFERSPROC GenRenderbuffers = nullptr;
inline PFNGLDELETERENDERBUFFERSPROC DeleteRenderbuffers = nullptr;
inline PFNGLBINDRENDERBUFFERPROC BindRenderbuffer = nullptr;
inline PFNGLRENDERBUFFERSTORAGEPROC RenderbufferStorage = nullptr;
inline PFNGLFRAMEBUFFERRENDERBUFFERPROC FramebufferRenderbuffer = nullptr;

inline PFNGLGENVERTEXARRAYSPROC GenVertexArrays = nullptr;
inline PFNGLDELETEVERTEXARRAYSPROC DeleteVertexArrays = nullptr;
inline PFNGLBINDVERTEXARRAYPROC BindVertexArray = nullptr;

inline PFNGLGENBUFFERSPROC GenBuffers = nullptr;
inline PFNGLDELETEBUFFERSPROC DeleteBuffers = nullptr;
inline PFNGLBINDBUFFERPROC BindBuffer = nullptr;
inline PFNGLBUFFERDATAPROC BufferData = nullptr;

inline PFNGLENABLEVERTEXATTRIBARRAYPROC EnableVertexAttribArray = nullptr;
inline PFNGLVERTEXATTRIBPOINTERPROC VertexAttribPointer = nullptr;

inline PFNGLCREATESHADERPROC CreateShader = nullptr;
inline PFNGLDELETESHADERPROC DeleteShader = nullptr;
inline PFNGLSHADERSOURCEPROC ShaderSource = nullptr;
inline PFNGLCOMPILESHADERPROC CompileShader = nullptr;
inline PFNGLGETSHADERIVPROC GetShaderiv = nullptr;
inline PFNGLGETSHADERINFOLOGPROC GetShaderInfoLog = nullptr;

inline PFNGLCREATEPROGRAMPROC CreateProgram = nullptr;
inline PFNGLDELETEPROGRAMPROC DeleteProgram = nullptr;
inline PFNGLATTACHSHADERPROC AttachShader = nullptr;
inline PFNGLLINKPROGRAMPROC LinkProgram = nullptr;
inline PFNGLGETPROGRAMIVPROC GetProgramiv = nullptr;
inline PFNGLGETPROGRAMINFOLOGPROC GetProgramInfoLog = nullptr;
inline PFNGLUSEPROGRAMPROC UseProgram = nullptr;

inline PFNGLGETUNIFORMLOCATIONPROC GetUniformLocation = nullptr;
inline PFNGLUNIFORM1IPROC Uniform1i = nullptr;
inline PFNGLUNIFORM1FPROC Uniform1f = nullptr;
inline PFNGLUNIFORM3FPROC Uniform3f = nullptr;
inline PFNGLUNIFORM4FPROC Uniform4f = nullptr;
inline PFNGLUNIFORMMATRIX4FVPROC UniformMatrix4fv = nullptr;

inline PFNGLACTIVETEXTUREPROC ActiveTexture = nullptr;
inline PFNGLGENERATEMIPMAPPROC GenerateMipmap = nullptr;

} // namespace imgstudio::render::gl
