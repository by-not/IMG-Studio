#pragma once

#include "app/AssetLoader.hpp"
#include "render/GlLoader.hpp"
#include "render/GpuTexture.hpp"
#include "render/Math3D.hpp"
#include "render/ViewportFbo.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace imgstudio::render {

enum class DffRenderMode : std::uint8_t {
    Textured = 0,
    Solid,
    Wireframe,
    MaterialColors
};

struct DffViewSettings {
    DffRenderMode mode = DffRenderMode::Textured;
    bool show_grid = true;
    bool show_axes = true;
    bool backface_cull = false;
    bool center_model = true;
    float light_azimuth_deg = 45.0f;
    float light_elevation_deg = 55.0f;
    float ambient_strength = 0.32f;
    std::array<float, 3> bg_color{0.09f, 0.105f, 0.135f};
};

struct OrbitCamera {
    Vec3 target{0.0f, 0.0f, 0.0f};
    float distance = 5.0f;
    float yaw_deg = -35.0f;
    float pitch_deg = 22.0f;
    float fov_deg = 45.0f;

    void orbit(float delta_yaw_deg, float delta_pitch_deg) noexcept;
    void pan(float dx_pixels, float dy_pixels, int viewport_height) noexcept;
    void zoom(float wheel_delta) noexcept;
    [[nodiscard]] Vec3 eye_position() const noexcept;
};

class DffRenderer {
public:
    DffRenderer() = default;
    ~DffRenderer();

    DffRenderer(const DffRenderer&) = delete;
    DffRenderer& operator=(const DffRenderer&) = delete;

    bool initialize();
    void shutdown();

    void upload_session(const app::LoadedDffSession& session);
    void refresh_textures(const app::LoadedDffSession& session);
    void clear_model();

    void fit_camera(OrbitCamera& cam, const DffViewSettings& settings) const noexcept;
    void render(
        int width,
        int height,
        const OrbitCamera& cam,
        const DffViewSettings& settings);

    [[nodiscard]] GLuint output_texture() const noexcept { return fbo_.color_texture(); }
    [[nodiscard]] bool is_initialized() const noexcept { return initialized_; }
    [[nodiscard]] bool fbo_valid() const noexcept { return fbo_.valid(); }
    [[nodiscard]] bool has_model() const noexcept { return !meshes_.empty(); }
    [[nodiscard]] const std::string& last_error() const noexcept { return last_error_; }

private:
    struct GpuSubMesh {
        GLuint ebo = 0U;
        GLsizei index_count = 0;
        std::uint32_t material_index = 0U;
        std::array<float, 4> base_color{1.0f, 1.0f, 1.0f, 1.0f};
        std::string texture_key;
        bool has_texture_ref = false;
    };

    struct GpuGeometryBatch {
        GLuint vao = 0U;
        GLuint vbo = 0U;
        Mat4 model_matrix = mat4_identity();
        std::vector<GpuSubMesh> splits;
    };

    void release_model_gpu() noexcept;
    void build_grid_and_axes();

    bool initialized_ = false;
    std::string last_error_;

    GLuint mesh_program_ = 0U;
    GLuint line_program_ = 0U;

    GLint u_mvp_ = -1;
    GLint u_model_ = -1;
    GLint u_light_dir_ = -1;
    GLint u_ambient_ = -1;
    GLint u_mat_color_ = -1;
    GLint u_use_texture_ = -1;
    GLint u_shade_mode_ = -1;
    GLint u_tex_ = -1;
    GLint u_line_mvp_ = -1;

    GLuint grid_vao_ = 0U;
    GLuint grid_vbo_ = 0U;
    GLsizei grid_vertex_count_ = 0;

    GLuint axes_vao_ = 0U;
    GLuint axes_vbo_ = 0U;
    GLsizei axes_vertex_count_ = 0;

    ViewportFbo fbo_;
    std::vector<GpuGeometryBatch> meshes_;
    std::unordered_map<std::string, GpuTexture> textures_;
    core::rw::DffBounds model_bounds_{};
};

} // namespace imgstudio::render
