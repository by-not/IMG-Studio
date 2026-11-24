#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace imgstudio::core::rw {

struct Vec2f {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3f {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Rgba8 {
    std::uint8_t r = 255;
    std::uint8_t g = 255;
    std::uint8_t b = 255;
    std::uint8_t a = 255;
};

struct Mat4f {
    // Column-major 4x4 matrix
    std::array<float, 16> m{
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
};

struct DffBounds {
    Vec3f min{};
    Vec3f max{};
    Vec3f center{};
    float radius = 0.0f;
    bool valid = false;
};

struct DffFrame {
    std::array<float, 9> rotation{1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    Vec3f position{};
    std::int32_t parent_index = -1;
    std::uint32_t flags = 0;
    std::string name;
    Mat4f local_matrix{};
    Mat4f world_matrix{};
};

struct DffMaterial {
    Rgba8 color{255, 255, 255, 255};
    float ambient = 1.0f;
    float specular = 1.0f;
    float diffuse = 1.0f;
    bool is_textured = false;
    std::string texture_name;
    std::string mask_name;
    std::uint32_t filter_flags = 0;
};

struct DffMeshSplit {
    std::uint32_t material_index = 0;
    std::vector<std::uint32_t> triangle_indices; // Always unpacked to triangle list (3 indices per tri)
};

struct DffGeometry {
    std::uint32_t flags = 0;
    std::uint32_t num_triangles_declared = 0;
    std::uint32_t num_vertices = 0;
    std::uint32_t num_morph_targets = 0;
    std::uint32_t uv_channel_count = 0;

    bool has_positions = false;
    bool has_normals = false;
    bool has_uvs = false;
    bool has_prelit = false;
    bool is_tristrip = false;
    bool is_native_unsupported = false;

    std::vector<Vec3f> positions;
    std::vector<Vec3f> normals;
    std::vector<Vec2f> uvs;
    std::vector<Rgba8> prelit_colors;
    std::vector<DffMaterial> materials;
    std::vector<DffMeshSplit> splits;

    Vec3f bounding_sphere_center{};
    float bounding_sphere_radius = 0.0f;
    DffBounds bounds{};

    [[nodiscard]] std::size_t renderable_triangle_count() const noexcept;
};

struct DffAtomic {
    std::int32_t frame_index = -1;
    std::int32_t geometry_index = -1;
    std::uint32_t flags = 0;
};

struct DffWarning {
    bool is_error = false;
    std::string message;
};

struct DffModel {
    std::string source_name;
    std::uint64_t file_size_bytes = 0;
    std::uint32_t rw_version = 0;

    std::vector<DffFrame> frames;
    std::vector<DffGeometry> geometries;
    std::vector<DffAtomic> atomics;
    std::vector<std::string> referenced_textures;
    std::vector<DffWarning> warnings;

    std::size_t total_vertices = 0;
    std::size_t total_triangles = 0;
    std::size_t total_materials = 0;
    DffBounds bounds{};
};

} // namespace imgstudio::core::rw
