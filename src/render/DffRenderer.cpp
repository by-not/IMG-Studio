#include "render/DffRenderer.hpp"
#include "core/ImgFilename.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace imgstudio::render {

namespace {

inline constexpr float kDegToRad = 0.017453292519943295f;

const char* kMeshVertexShader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aUv;
layout (location = 3) in vec4 aColor;

uniform mat4 uMvp;
uniform mat4 uModel;

out vec3 vNormal;
out vec2 vUv;
out vec4 vColor;

void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
    vNormal = normalize(mat3(uModel) * aNormal);
    vUv = aUv;
    vColor = aColor;
}
)";

const char* kMeshFragmentShader = R"(
#version 330 core
in vec3 vNormal;
in vec2 vUv;
in vec4 vColor;

uniform vec3 uLightDir;
uniform float uAmbient;
uniform vec4 uMatColor;
uniform int uUseTexture;
uniform int uShadeMode;
uniform sampler2D uTex;

out vec4 FragColor;

void main() {
    vec4 base = uMatColor * vColor;
    if (uUseTexture != 0 && uShadeMode == 0) {
        vec4 texel = texture(uTex, vUv);
        if (texel.a < 0.04) discard;
        base *= texel;
    }

    if (uShadeMode == 2) {
        FragColor = vec4(0.32, 0.58, 1.0, 1.0);
        return;
    }

    vec3 n = normalize(vNormal);
    if (!gl_FrontFacing) n = -n;
    float ndl = max(dot(n, normalize(uLightDir)), 0.0);
    float light = clamp(uAmbient + (1.0 - uAmbient) * ndl, 0.0, 1.2);
    FragColor = vec4(base.rgb * light, base.a);
}
)";

const char* kLineVertexShader = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aColor;

uniform mat4 uMvp;
out vec3 vColor;

void main() {
    gl_Position = uMvp * vec4(aPos, 1.0);
    vColor = aColor;
}
)";

const char* kLineFragmentShader = R"(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main() {
    FragColor = vec4(vColor, 1.0);
}
)";

GLuint compile_shader(GLenum type, const char* src, std::string& err) {
    GLuint sh = gl::CreateShader(type);
    gl::ShaderSource(sh, 1, &src, nullptr);
    gl::CompileShader(sh);
    GLint ok = 0;
    gl::GetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        gl::GetShaderiv(sh, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<std::size_t>(std::max(1, len)), '\0');
        gl::GetShaderInfoLog(sh, len, nullptr, log.data());
        err = log;
        gl::DeleteShader(sh);
        return 0U;
    }
    return sh;
}

GLuint link_program(const char* vs_src, const char* fs_src, std::string& err) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vs_src, err);
    if (vs == 0U) return 0U;
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fs_src, err);
    if (fs == 0U) {
        gl::DeleteShader(vs);
        return 0U;
    }
    GLuint prog = gl::CreateProgram();
    gl::AttachShader(prog, vs);
    gl::AttachShader(prog, fs);
    gl::LinkProgram(prog);
    gl::DeleteShader(vs);
    gl::DeleteShader(fs);

    GLint ok = 0;
    gl::GetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        GLint len = 0;
        gl::GetProgramiv(prog, GL_INFO_LOG_LENGTH, &len);
        std::string log(static_cast<std::size_t>(std::max(1, len)), '\0');
        gl::GetProgramInfoLog(prog, len, nullptr, log.data());
        err = log;
        gl::DeleteProgram(prog);
        return 0U;
    }
    return prog;
}

struct PackedVertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
    float r, g, b, a;
};

std::array<float, 4> material_palette_color(std::uint32_t idx) noexcept {
    static constexpr std::array<std::array<float, 4>, 8> kCols = {{
        {0.23f, 0.51f, 0.96f, 1.0f},
        {0.13f, 0.77f, 0.37f, 1.0f},
        {0.96f, 0.62f, 0.04f, 1.0f},
        {0.94f, 0.27f, 0.27f, 1.0f},
        {0.02f, 0.71f, 0.83f, 1.0f},
        {0.66f, 0.33f, 0.97f, 1.0f},
        {0.93f, 0.29f, 0.60f, 1.0f},
        {0.52f, 0.80f, 0.09f, 1.0f}
    }};
    return kCols[idx % kCols.size()];
}

} // namespace

void OrbitCamera::orbit(float delta_yaw_deg, float delta_pitch_deg) noexcept {
    yaw_deg += delta_yaw_deg;
    pitch_deg = std::clamp(pitch_deg + delta_pitch_deg, -89.0f, 89.0f);
}

void OrbitCamera::pan(float dx_pixels, float dy_pixels, int viewport_height) noexcept {
    const float yaw = yaw_deg * kDegToRad;
    const float pitch = pitch_deg * kDegToRad;
    const Vec3 eye = eye_position();
    const Vec3 forward = vec3_normalize(vec3_sub(target, eye));
    const Vec3 world_up{0.0f, 0.0f, 1.0f};
    const Vec3 right = vec3_normalize(vec3_cross(forward, world_up));
    const Vec3 up = vec3_cross(right, forward);
    (void)yaw;
    (void)pitch;

    const float scale = distance / static_cast<float>(std::max(100, viewport_height));
    target = vec3_sub(target, vec3_scale(right, dx_pixels * scale));
    target = vec3_add(target, vec3_scale(up, dy_pixels * scale));
}

void OrbitCamera::zoom(float wheel_delta) noexcept {
    const float factor = std::pow(0.88f, wheel_delta);
    distance = std::clamp(distance * factor, 0.1f, 2000.0f);
}

Vec3 OrbitCamera::eye_position() const noexcept {
    const float yaw = yaw_deg * kDegToRad;
    const float pitch = pitch_deg * kDegToRad;
    const float cp = std::cos(pitch);
    return Vec3{
        target.x + distance * cp * std::cos(yaw),
        target.y + distance * cp * std::sin(yaw),
        target.z + distance * std::sin(pitch)
    };
}

DffRenderer::~DffRenderer() {
    shutdown();
}

bool DffRenderer::initialize() {
    if (initialized_) {
        return true;
    }
    if (!gl::init_gl_functions()) {
        last_error_ = "Failed to load required OpenGL 3.3 Core functions.";
        return false;
    }

    mesh_program_ = link_program(kMeshVertexShader, kMeshFragmentShader, last_error_);
    if (mesh_program_ == 0U) {
        return false;
    }
    line_program_ = link_program(kLineVertexShader, kLineFragmentShader, last_error_);
    if (line_program_ == 0U) {
        gl::DeleteProgram(mesh_program_);
        mesh_program_ = 0U;
        return false;
    }

    u_mvp_ = gl::GetUniformLocation(mesh_program_, "uMvp");
    u_model_ = gl::GetUniformLocation(mesh_program_, "uModel");
    u_light_dir_ = gl::GetUniformLocation(mesh_program_, "uLightDir");
    u_ambient_ = gl::GetUniformLocation(mesh_program_, "uAmbient");
    u_mat_color_ = gl::GetUniformLocation(mesh_program_, "uMatColor");
    u_use_texture_ = gl::GetUniformLocation(mesh_program_, "uUseTexture");
    u_shade_mode_ = gl::GetUniformLocation(mesh_program_, "uShadeMode");
    u_tex_ = gl::GetUniformLocation(mesh_program_, "uTex");
    u_line_mvp_ = gl::GetUniformLocation(line_program_, "uMvp");

    build_grid_and_axes();
    initialized_ = true;
    return true;
}

void DffRenderer::build_grid_and_axes() {
    struct LineVert {
        float x, y, z;
        float r, g, b;
    };
    std::vector<LineVert> grid;
    const int half = 10;
    for (int i = -half; i <= half; ++i) {
        const float v = static_cast<float>(i);
        const bool major = (i % 5 == 0);
        const float c = major ? 0.28f : 0.19f;
        grid.push_back(LineVert{-static_cast<float>(half), v, 0.0f, c, c, c + 0.03f});
        grid.push_back(LineVert{ static_cast<float>(half), v, 0.0f, c, c, c + 0.03f});
        grid.push_back(LineVert{v, -static_cast<float>(half), 0.0f, c, c, c + 0.03f});
        grid.push_back(LineVert{v,  static_cast<float>(half), 0.0f, c, c, c + 0.03f});
    }
    grid_vertex_count_ = static_cast<GLsizei>(grid.size());

    gl::GenVertexArrays(1, &grid_vao_);
    gl::GenBuffers(1, &grid_vbo_);
    gl::BindVertexArray(grid_vao_);
    gl::BindBuffer(GL_ARRAY_BUFFER, grid_vbo_);
    gl::BufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(grid.size() * sizeof(LineVert)),
        grid.data(),
        GL_STATIC_DRAW);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVert), reinterpret_cast<void*>(0));
    gl::EnableVertexAttribArray(1);
    gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LineVert), reinterpret_cast<void*>(3 * sizeof(float)));

    std::vector<LineVert> axes = {
        {0.0f, 0.0f, 0.0f, 0.94f, 0.27f, 0.27f}, {2.0f, 0.0f, 0.0f, 0.94f, 0.27f, 0.27f}, // +X Red
        {0.0f, 0.0f, 0.0f, 0.13f, 0.77f, 0.37f}, {0.0f, 2.0f, 0.0f, 0.13f, 0.77f, 0.37f}, // +Y Green
        {0.0f, 0.0f, 0.0f, 0.23f, 0.51f, 0.96f}, {0.0f, 0.0f, 2.0f, 0.23f, 0.51f, 0.96f}, // +Z Blue
    };
    axes_vertex_count_ = static_cast<GLsizei>(axes.size());

    gl::GenVertexArrays(1, &axes_vao_);
    gl::GenBuffers(1, &axes_vbo_);
    gl::BindVertexArray(axes_vao_);
    gl::BindBuffer(GL_ARRAY_BUFFER, axes_vbo_);
    gl::BufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(axes.size() * sizeof(LineVert)),
        axes.data(),
        GL_STATIC_DRAW);
    gl::EnableVertexAttribArray(0);
    gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(LineVert), reinterpret_cast<void*>(0));
    gl::EnableVertexAttribArray(1);
    gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(LineVert), reinterpret_cast<void*>(3 * sizeof(float)));
    gl::BindVertexArray(0U);
}

void DffRenderer::release_model_gpu() noexcept {
    for (auto& batch : meshes_) {
        for (auto& sp : batch.splits) {
            if (sp.ebo != 0U && gl::DeleteBuffers != nullptr) {
                gl::DeleteBuffers(1, &sp.ebo);
            }
        }
        if (batch.vbo != 0U && gl::DeleteBuffers != nullptr) {
            gl::DeleteBuffers(1, &batch.vbo);
        }
        if (batch.vao != 0U && gl::DeleteVertexArrays != nullptr) {
            gl::DeleteVertexArrays(1, &batch.vao);
        }
    }
    meshes_.clear();
    textures_.clear();
}

void DffRenderer::clear_model() {
    release_model_gpu();
    model_bounds_ = core::rw::DffBounds{};
}

void DffRenderer::shutdown() {
    release_model_gpu();
    fbo_.release();
    if (grid_vbo_ != 0U && gl::DeleteBuffers != nullptr) gl::DeleteBuffers(1, &grid_vbo_);
    if (grid_vao_ != 0U && gl::DeleteVertexArrays != nullptr) gl::DeleteVertexArrays(1, &grid_vao_);
    if (axes_vbo_ != 0U && gl::DeleteBuffers != nullptr) gl::DeleteBuffers(1, &axes_vbo_);
    if (axes_vao_ != 0U && gl::DeleteVertexArrays != nullptr) gl::DeleteVertexArrays(1, &axes_vao_);
    if (mesh_program_ != 0U && gl::DeleteProgram != nullptr) gl::DeleteProgram(mesh_program_);
    if (line_program_ != 0U && gl::DeleteProgram != nullptr) gl::DeleteProgram(line_program_);
    grid_vao_ = grid_vbo_ = axes_vao_ = axes_vbo_ = mesh_program_ = line_program_ = 0U;
    initialized_ = false;
}

void DffRenderer::refresh_textures(const app::LoadedDffSession& session) {
    textures_.clear();
    for (const auto& [key, info] : session.resolved_textures) {
        if (info.resolved && info.texture != nullptr && !info.texture->mipmaps.empty()) {
            GpuTexture gtex;
            if (gtex.upload_mip(info.texture->mipmaps[0], TextureFilterMode::Linear, true)) {
                textures_.emplace(key, std::move(gtex));
            }
        }
    }
}

void DffRenderer::upload_session(const app::LoadedDffSession& session) {
    if (!initialized_ && !initialize()) {
        return;
    }
    release_model_gpu();
    model_bounds_ = session.model.bounds;

    const auto& model = session.model;
    auto upload_geom = [&](std::size_t gidx, const Mat4& xform) {
        if (gidx >= model.geometries.size()) return;
        const auto& geom = model.geometries[gidx];
        if (geom.is_native_unsupported || geom.positions.empty() || geom.splits.empty()) {
            return;
        }

        std::vector<PackedVertex> verts(geom.positions.size());
        for (std::size_t i = 0; i < geom.positions.size(); ++i) {
            PackedVertex pv{};
            pv.px = geom.positions[i].x;
            pv.py = geom.positions[i].y;
            pv.pz = geom.positions[i].z;
            if (i < geom.normals.size()) {
                pv.nx = geom.normals[i].x;
                pv.ny = geom.normals[i].y;
                pv.nz = geom.normals[i].z;
            } else {
                pv.nz = 1.0f;
            }
            if (i < geom.uvs.size()) {
                pv.u = geom.uvs[i].x;
                pv.v = geom.uvs[i].y;
            }
            if (i < geom.prelit_colors.size()) {
                pv.r = static_cast<float>(geom.prelit_colors[i].r) / 255.0f;
                pv.g = static_cast<float>(geom.prelit_colors[i].g) / 255.0f;
                pv.b = static_cast<float>(geom.prelit_colors[i].b) / 255.0f;
                pv.a = static_cast<float>(geom.prelit_colors[i].a) / 255.0f;
            } else {
                pv.r = pv.g = pv.b = pv.a = 1.0f;
            }
            verts[i] = pv;
        }

        GpuGeometryBatch batch;
        batch.model_matrix = xform;
        gl::GenVertexArrays(1, &batch.vao);
        gl::GenBuffers(1, &batch.vbo);
        gl::BindVertexArray(batch.vao);
        gl::BindBuffer(GL_ARRAY_BUFFER, batch.vbo);
        gl::BufferData(
            GL_ARRAY_BUFFER,
            static_cast<GLsizeiptr>(verts.size() * sizeof(PackedVertex)),
            verts.data(),
            GL_STATIC_DRAW);

        gl::EnableVertexAttribArray(0);
        gl::VertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PackedVertex), reinterpret_cast<void*>(0));
        gl::EnableVertexAttribArray(1);
        gl::VertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(PackedVertex), reinterpret_cast<void*>(3 * sizeof(float)));
        gl::EnableVertexAttribArray(2);
        gl::VertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(PackedVertex), reinterpret_cast<void*>(6 * sizeof(float)));
        gl::EnableVertexAttribArray(3);
        gl::VertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, sizeof(PackedVertex), reinterpret_cast<void*>(8 * sizeof(float)));

        for (const auto& s : geom.splits) {
            if (s.triangle_indices.empty()) continue;
            GpuSubMesh sub;
            sub.material_index = s.material_index;
            sub.index_count = static_cast<GLsizei>(s.triangle_indices.size());
            if (s.material_index < geom.materials.size()) {
                const auto& m = geom.materials[s.material_index];
                sub.base_color = {
                    static_cast<float>(m.color.r) / 255.0f,
                    static_cast<float>(m.color.g) / 255.0f,
                    static_cast<float>(m.color.b) / 255.0f,
                    static_cast<float>(m.color.a) / 255.0f
                };
                if (!m.texture_name.empty()) {
                    sub.texture_key = core::to_ascii_lower(m.texture_name);
                    sub.has_texture_ref = true;
                }
            }
            gl::GenBuffers(1, &sub.ebo);
            gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, sub.ebo);
            gl::BufferData(
                GL_ELEMENT_ARRAY_BUFFER,
                static_cast<GLsizeiptr>(s.triangle_indices.size() * sizeof(std::uint32_t)),
                s.triangle_indices.data(),
                GL_STATIC_DRAW);
            batch.splits.push_back(std::move(sub));
        }
        gl::BindVertexArray(0U);
        meshes_.push_back(std::move(batch));
    };

    if (!model.atomics.empty()) {
        for (const auto& a : model.atomics) {
            Mat4 xf = mat4_identity();
            if (a.frame_index >= 0 && static_cast<std::size_t>(a.frame_index) < model.frames.size()) {
                xf = model.frames[static_cast<std::size_t>(a.frame_index)].world_matrix;
            }
            if (a.geometry_index >= 0) {
                upload_geom(static_cast<std::size_t>(a.geometry_index), xf);
            }
        }
    } else {
        for (std::size_t i = 0; i < model.geometries.size(); ++i) {
            upload_geom(i, mat4_identity());
        }
    }

    refresh_textures(session);
}

void DffRenderer::fit_camera(OrbitCamera& cam, const DffViewSettings& settings) const noexcept {
    if (!model_bounds_.valid) {
        cam = OrbitCamera{};
        return;
    }
    cam.target = settings.center_model ? Vec3{0.0f, 0.0f, 0.0f} : model_bounds_.center;
    const float radius = std::max(0.25f, model_bounds_.radius);
    const float half_fov = (cam.fov_deg * 0.5f) * kDegToRad;
    cam.distance = std::clamp((radius / std::sin(half_fov)) * 1.15f, 0.5f, 1000.0f);
    cam.yaw_deg = -35.0f;
    cam.pitch_deg = 22.0f;
}

void DffRenderer::render(
    int width,
    int height,
    const OrbitCamera& cam,
    const DffViewSettings& settings) {
    if (!initialized_ && !initialize()) {
        return;
    }
    if (!fbo_.ensure_size(width, height)) {
        return;
    }

    fbo_.bind();
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    if (settings.backface_cull) {
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    } else {
        glDisable(GL_CULL_FACE);
    }

    glClearColor(settings.bg_color[0], settings.bg_color[1], settings.bg_color[2], 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float aspect = static_cast<float>(fbo_.width()) / static_cast<float>(std::max(1, fbo_.height()));
    const float znear = std::max(0.02f, cam.distance * 0.005f);
    const float zfar = std::max(200.0f, cam.distance * 25.0f);
    const Mat4 proj = mat4_perspective(cam.fov_deg * kDegToRad, aspect, znear, zfar);
    const Mat4 view = mat4_look_at(cam.eye_position(), cam.target, Vec3{0.0f, 0.0f, 1.0f});
    const Mat4 vp = mat4_mul(proj, view);

    // Draw Grid & Axes
    gl::UseProgram(line_program_);
    gl::UniformMatrix4fv(u_line_mvp_, 1, GL_FALSE, vp.m.data());
    if (settings.show_grid && grid_vao_ != 0U) {
        gl::BindVertexArray(grid_vao_);
        glDrawArrays(GL_LINES, 0, grid_vertex_count_);
    }
    if (settings.show_axes && axes_vao_ != 0U) {
        gl::BindVertexArray(axes_vao_);
        glDrawArrays(GL_LINES, 0, axes_vertex_count_);
    }
    gl::BindVertexArray(0U);

    // Draw Meshes
    if (!meshes_.empty()) {
        Mat4 center_offset = mat4_identity();
        if (settings.center_model && model_bounds_.valid) {
            center_offset.m[12] = -model_bounds_.center.x;
            center_offset.m[13] = -model_bounds_.center.y;
            center_offset.m[14] = -model_bounds_.center.z;
        }

        const float az = settings.light_azimuth_deg * kDegToRad;
        const float el = settings.light_elevation_deg * kDegToRad;
        const Vec3 light_dir{
            std::cos(el) * std::cos(az),
            std::cos(el) * std::sin(az),
            std::sin(el)
        };

        gl::UseProgram(mesh_program_);
        gl::Uniform3f(u_light_dir_, light_dir.x, light_dir.y, light_dir.z);
        gl::Uniform1f(u_ambient_, settings.ambient_strength);
        gl::Uniform1i(u_tex_, 0);
        gl::ActiveTexture(GL_TEXTURE0);

        if (settings.mode == DffRenderMode::Wireframe) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }

        const int shade_mode = (settings.mode == DffRenderMode::Wireframe) ? 2 : 0;
        gl::Uniform1i(u_shade_mode_, shade_mode);

        for (const auto& batch : meshes_) {
            const Mat4 model_mat = mat4_mul(center_offset, batch.model_matrix);
            const Mat4 mvp = mat4_mul(vp, model_mat);
            gl::UniformMatrix4fv(u_mvp_, 1, GL_FALSE, mvp.m.data());
            gl::UniformMatrix4fv(u_model_, 1, GL_FALSE, model_mat.m.data());
            gl::BindVertexArray(batch.vao);

            for (const auto& sp : batch.splits) {
                if (settings.mode == DffRenderMode::MaterialColors) {
                    const auto mc = material_palette_color(sp.material_index);
                    gl::Uniform4f(u_mat_color_, mc[0], mc[1], mc[2], mc[3]);
                    gl::Uniform1i(u_use_texture_, 0);
                } else {
                    gl::Uniform4f(
                        u_mat_color_,
                        sp.base_color[0],
                        sp.base_color[1],
                        sp.base_color[2],
                        sp.base_color[3]);
                    bool bound = false;
                    if (settings.mode == DffRenderMode::Textured && sp.has_texture_ref) {
                        auto it = textures_.find(sp.texture_key);
                        if (it != textures_.end() && it->second.valid()) {
                            glBindTexture(GL_TEXTURE_2D, it->second.id());
                            bound = true;
                        }
                    }
                    gl::Uniform1i(u_use_texture_, bound ? 1 : 0);
                }

                gl::BindBuffer(GL_ELEMENT_ARRAY_BUFFER, sp.ebo);
                glDrawElements(GL_TRIANGLES, sp.index_count, GL_UNSIGNED_INT, nullptr);
            }
        }

        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glBindTexture(GL_TEXTURE_2D, 0U);
        gl::BindVertexArray(0U);
    }

    gl::UseProgram(0U);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    fbo_.unbind();
}

} // namespace imgstudio::render
