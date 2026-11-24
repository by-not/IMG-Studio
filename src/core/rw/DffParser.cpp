#include "core/rw/DffParser.hpp"
#include "core/ImgFilename.hpp"
#include "core/rw/RwStream.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <unordered_map>
#include <unordered_set>

namespace imgstudio::core::rw {

std::size_t DffGeometry::renderable_triangle_count() const noexcept {
    std::size_t sum = 0;
    for (const auto& s : splits) {
        sum += s.triangle_indices.size() / 3U;
    }
    return sum;
}

namespace {

inline constexpr std::uint32_t kGeomTriStrip   = 0x00000001U;
inline constexpr std::uint32_t kGeomPositions  = 0x00000002U;
inline constexpr std::uint32_t kGeomTextured   = 0x00000004U;
inline constexpr std::uint32_t kGeomPrelit     = 0x00000008U;
inline constexpr std::uint32_t kGeomNormals    = 0x00000010U;
inline constexpr std::uint32_t kGeomTextured2  = 0x00000080U;
inline constexpr std::uint32_t kGeomNative     = 0x01000000U;

Mat4f make_mat4_from_frame(const DffFrame& f) noexcept {
    Mat4f mat;
    // RW rotation is 3 rows x 3 cols: right(0..2), up(3..5), at(6..8)
    mat.m[0] = f.rotation[0]; mat.m[1] = f.rotation[1]; mat.m[2]  = f.rotation[2];  mat.m[3]  = 0.0f;
    mat.m[4] = f.rotation[3]; mat.m[5] = f.rotation[4]; mat.m[6]  = f.rotation[5];  mat.m[7]  = 0.0f;
    mat.m[8] = f.rotation[6]; mat.m[9] = f.rotation[7]; mat.m[10] = f.rotation[8];  mat.m[11] = 0.0f;
    mat.m[12] = f.position.x; mat.m[13] = f.position.y; mat.m[14] = f.position.z;   mat.m[15] = 1.0f;
    return mat;
}

Mat4f mul_mat4(const Mat4f& a, const Mat4f& b) noexcept {
    Mat4f r{};
    for (int c = 0; c < 4; ++c) {
        for (int row = 0; row < 4; ++row) {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                sum += a.m[static_cast<std::size_t>(k * 4 + row)] *
                       b.m[static_cast<std::size_t>(c * 4 + k)];
            }
            r.m[static_cast<std::size_t>(c * 4 + row)] = sum;
        }
    }
    return r;
}

Vec3f transform_point(const Mat4f& m, const Vec3f& v) noexcept {
    return Vec3f{
        m.m[0] * v.x + m.m[4] * v.y + m.m[8]  * v.z + m.m[12],
        m.m[1] * v.x + m.m[5] * v.y + m.m[9]  * v.z + m.m[13],
        m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14]
    };
}

void compute_frame_world_matrices(std::vector<DffFrame>& frames, std::vector<DffWarning>& warnings) {
    for (auto& f : frames) {
        f.local_matrix = make_mat4_from_frame(f);
        f.world_matrix = f.local_matrix;
    }

    std::vector<int> state(frames.size(), 0); // 0=unvisited, 1=visiting, 2=done
    auto resolve = [&](auto& self, std::size_t idx) -> void {
        if (state[idx] == 2) return;
        if (state[idx] == 1) {
            warnings.push_back(DffWarning{false, "Cycle detected in DFF FrameList hierarchy."});
            state[idx] = 2;
            return;
        }
        state[idx] = 1;
        const std::int32_t parent = frames[idx].parent_index;
        if (parent >= 0 && static_cast<std::size_t>(parent) < frames.size() &&
            static_cast<std::size_t>(parent) != idx) {
            self(self, static_cast<std::size_t>(parent));
            frames[idx].world_matrix = mul_mat4(
                frames[static_cast<std::size_t>(parent)].world_matrix,
                frames[idx].local_matrix);
        }
        state[idx] = 2;
    };

    for (std::size_t i = 0; i < frames.size(); ++i) {
        resolve(resolve, i);
    }
}

void generate_smooth_normals(DffGeometry& geom) {
    if (geom.positions.empty()) {
        return;
    }
    geom.normals.assign(geom.positions.size(), Vec3f{0.0f, 0.0f, 0.0f});
    for (const auto& split : geom.splits) {
        for (std::size_t i = 0; i + 2U < split.triangle_indices.size(); i += 3U) {
            const std::uint32_t i0 = split.triangle_indices[i];
            const std::uint32_t i1 = split.triangle_indices[i + 1U];
            const std::uint32_t i2 = split.triangle_indices[i + 2U];
            if (i0 >= geom.positions.size() || i1 >= geom.positions.size() || i2 >= geom.positions.size()) {
                continue;
            }
            const Vec3f& p0 = geom.positions[i0];
            const Vec3f& p1 = geom.positions[i1];
            const Vec3f& p2 = geom.positions[i2];
            const Vec3f e1{p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
            const Vec3f e2{p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
            const Vec3f fn{
                e1.y * e2.z - e1.z * e2.y,
                e1.z * e2.x - e1.x * e2.z,
                e1.x * e2.y - e1.y * e2.x
            };
            geom.normals[i0].x += fn.x; geom.normals[i0].y += fn.y; geom.normals[i0].z += fn.z;
            geom.normals[i1].x += fn.x; geom.normals[i1].y += fn.y; geom.normals[i1].z += fn.z;
            geom.normals[i2].x += fn.x; geom.normals[i2].y += fn.y; geom.normals[i2].z += fn.z;
        }
    }
    for (auto& n : geom.normals) {
        const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
        if (len > 1e-6f) {
            n.x /= len;
            n.y /= len;
            n.z /= len;
        } else {
            n = Vec3f{0.0f, 0.0f, 1.0f};
        }
    }
}

bool parse_string_chunk_text(RwCursor& cur, std::string& out) {
    out.assign(reinterpret_cast<const char*>(cur.read_span(cur.remaining())->data()), cur.remaining());
    while (!out.empty() && out.back() == '\0') {
        out.pop_back();
    }
    return true;
}

bool parse_texture_chunk(RwCursor tex_cur, DffMaterial& mat, std::vector<DffWarning>& warnings) {
    RwChunkHeader struct_hdr{};
    auto struct_cur = tex_cur.read_chunk_payload(struct_hdr);
    if (!struct_cur.has_value() || struct_hdr.type != kRwStruct) {
        warnings.push_back(DffWarning{false, "Texture chunk missing Struct header."});
        return false;
    }
    if (!struct_cur->read_u32(mat.filter_flags)) {
        return false;
    }

    RwChunkHeader name_hdr{};
    if (auto name_cur = tex_cur.read_chunk_payload(name_hdr)) {
        if (name_hdr.type == kRwString) {
            parse_string_chunk_text(*name_cur, mat.texture_name);
        }
    }
    RwChunkHeader mask_hdr{};
    if (auto mask_cur = tex_cur.read_chunk_payload(mask_hdr)) {
        if (mask_hdr.type == kRwString) {
            parse_string_chunk_text(*mask_cur, mat.mask_name);
        }
    }
    return true;
}

bool parse_material_chunk(RwCursor mat_cur, DffMaterial& out_mat, std::vector<DffWarning>& warnings) {
    RwChunkHeader shdr{};
    auto scur = mat_cur.read_chunk_payload(shdr);
    if (!scur.has_value() || shdr.type != kRwStruct) {
        warnings.push_back(DffWarning{false, "Material chunk missing Struct."});
        return false;
    }

    std::uint32_t unused_flags = 0;
    std::uint32_t unused_val = 0;
    std::uint32_t is_tex = 0;
    if (!scur->read_u32(unused_flags) ||
        !scur->read_u8(out_mat.color.r) ||
        !scur->read_u8(out_mat.color.g) ||
        !scur->read_u8(out_mat.color.b) ||
        !scur->read_u8(out_mat.color.a) ||
        !scur->read_u32(unused_val) ||
        !scur->read_u32(is_tex)) {
        warnings.push_back(DffWarning{false, "Truncated Material Struct."});
        return false;
    }
    out_mat.is_textured = (is_tex != 0U);
    if (scur->remaining() >= 12U) {
        (void)scur->read_f32(out_mat.ambient);
        (void)scur->read_f32(out_mat.specular);
        (void)scur->read_f32(out_mat.diffuse);
    }

    while (!mat_cur.at_end()) {
        RwChunkHeader ch{};
        auto child = mat_cur.read_chunk_payload(ch);
        if (!child.has_value()) {
            warnings.push_back(DffWarning{false, "Truncated child chunk inside Material."});
            break;
        }
        if (ch.type == kRwTexture) {
            parse_texture_chunk(*child, out_mat, warnings);
        }
    }
    return true;
}

bool parse_material_list(RwCursor ml_cur, std::vector<DffMaterial>& materials, std::vector<DffWarning>& warnings) {
    RwChunkHeader shdr{};
    auto scur = ml_cur.read_chunk_payload(shdr);
    if (!scur.has_value() || shdr.type != kRwStruct) {
        return false;
    }

    std::uint32_t mat_count = 0;
    if (!scur->read_u32(mat_count)) {
        return false;
    }
    if (mat_count > 4096U || scur->remaining() < static_cast<std::size_t>(mat_count) * 4U) {
        warnings.push_back(DffWarning{true, "Invalid or truncated MaterialList count."});
        return false;
    }

    std::vector<std::int32_t> refs(mat_count, -1);
    for (std::uint32_t i = 0; i < mat_count; ++i) {
        (void)scur->read_i32(refs[i]);
    }

    materials.resize(mat_count);
    for (std::uint32_t i = 0; i < mat_count; ++i) {
        if (refs[i] == -1) {
            RwChunkHeader mhdr{};
            auto mcur = ml_cur.read_chunk_payload(mhdr);
            if (!mcur.has_value() || mhdr.type != kRwMaterial) {
                warnings.push_back(DffWarning{false, "Missing Material chunk in MaterialList."});
                break;
            }
            parse_material_chunk(*mcur, materials[i], warnings);
        } else if (refs[i] >= 0 && static_cast<std::size_t>(refs[i]) < i) {
            materials[i] = materials[static_cast<std::size_t>(refs[i])];
        }
    }
    return true;
}

void parse_binmesh_plg(
    RwCursor bcur,
    DffGeometry& geom,
    std::vector<DffWarning>& warnings) {
    std::uint32_t flags = 0;
    std::uint32_t num_splits = 0;
    std::uint32_t total_indices = 0;
    if (!bcur.read_u32(flags) || !bcur.read_u32(num_splits) || !bcur.read_u32(total_indices)) {
        warnings.push_back(DffWarning{false, "Truncated BinMesh PLG header."});
        return;
    }
    const bool is_strip = (flags & 1U) != 0U;
    if (num_splits > 4096U) {
        warnings.push_back(DffWarning{false, "Unreasonable BinMesh split count."});
        return;
    }

    geom.splits.clear();
    geom.splits.reserve(num_splits);

    for (std::uint32_t s = 0; s < num_splits; ++s) {
        std::uint32_t idx_count = 0;
        std::uint32_t mat_idx = 0;
        if (!bcur.read_u32(idx_count) || !bcur.read_u32(mat_idx)) {
            warnings.push_back(DffWarning{false, "Truncated BinMesh split header."});
            break;
        }
        if (bcur.remaining() < static_cast<std::size_t>(idx_count) * 4U) {
            warnings.push_back(DffWarning{false, "Truncated BinMesh index stream."});
            break;
        }

        std::vector<std::uint32_t> raw_indices(idx_count);
        for (std::uint32_t k = 0; k < idx_count; ++k) {
            (void)bcur.read_u32(raw_indices[k]);
        }

        DffMeshSplit split;
        split.material_index = mat_idx;

        if (!is_strip) {
            const std::size_t tri_indices = (raw_indices.size() / 3U) * 3U;
            split.triangle_indices.reserve(tri_indices);
            for (std::size_t k = 0; k + 2U < raw_indices.size(); k += 3U) {
                const std::uint32_t a = raw_indices[k];
                const std::uint32_t b = raw_indices[k + 1U];
                const std::uint32_t c = raw_indices[k + 2U];
                if (a < geom.num_vertices && b < geom.num_vertices && c < geom.num_vertices) {
                    split.triangle_indices.push_back(a);
                    split.triangle_indices.push_back(b);
                    split.triangle_indices.push_back(c);
                }
            }
        } else {
            split.triangle_indices.reserve(idx_count * 3U);
            for (std::size_t k = 0; k + 2U < raw_indices.size(); ++k) {
                const std::uint32_t a = raw_indices[k];
                const std::uint32_t b = raw_indices[k + 1U];
                const std::uint32_t c = raw_indices[k + 2U];
                if (a == b || b == c || a == c) {
                    continue; // Skip degenerate strip triangle
                }
                if (a >= geom.num_vertices || b >= geom.num_vertices || c >= geom.num_vertices) {
                    continue;
                }
                if ((k & 1U) == 0U) {
                    split.triangle_indices.push_back(a);
                    split.triangle_indices.push_back(b);
                    split.triangle_indices.push_back(c);
                } else {
                    split.triangle_indices.push_back(a);
                    split.triangle_indices.push_back(c);
                    split.triangle_indices.push_back(b);
                }
            }
        }

        if (!split.triangle_indices.empty()) {
            geom.splits.push_back(std::move(split));
        }
    }
}

bool parse_geometry_chunk(
    RwCursor gcur,
    const RwChunkHeader& ghdr,
    DffGeometry& geom,
    std::vector<DffWarning>& warnings) {
    RwChunkHeader shdr{};
    auto scur = gcur.read_chunk_payload(shdr);
    if (!scur.has_value() || shdr.type != kRwStruct) {
        warnings.push_back(DffWarning{true, "Geometry chunk missing Struct."});
        return false;
    }

    if (!scur->read_u32(geom.flags) ||
        !scur->read_u32(geom.num_triangles_declared) ||
        !scur->read_u32(geom.num_vertices) ||
        !scur->read_u32(geom.num_morph_targets)) {
        warnings.push_back(DffWarning{true, "Truncated Geometry Struct header."});
        return false;
    }

    if (ghdr.rw_version < 0x34000U) {
        if (!scur->skip(12U)) { // ambient, specular, diffuse
            warnings.push_back(DffWarning{true, "Truncated legacy Geometry surface properties."});
            return false;
        }
    }

    geom.has_positions = (geom.flags & kGeomPositions) != 0U;
    geom.has_normals = (geom.flags & kGeomNormals) != 0U;
    geom.has_prelit = (geom.flags & kGeomPrelit) != 0U;
    geom.is_tristrip = (geom.flags & kGeomTriStrip) != 0U;
    geom.is_native_unsupported = (geom.flags & kGeomNative) != 0U;

    std::uint32_t tex_sets = (geom.flags & 0x00FF0000U) >> 16U;
    if (tex_sets == 0U) {
        if ((geom.flags & kGeomTextured2) != 0U) {
            tex_sets = 2U;
        } else if ((geom.flags & kGeomTextured) != 0U) {
            tex_sets = 1U;
        }
    }
    geom.uv_channel_count = tex_sets;
    geom.has_uvs = (tex_sets > 0U);

    if (geom.num_vertices > 2000000U || geom.num_triangles_declared > 4000000U) {
        warnings.push_back(DffWarning{true, "Geometry vertex or triangle count exceeds safety bounds."});
        return false;
    }

    struct RawTri {
        std::uint16_t v1 = 0;
        std::uint16_t v2 = 0;
        std::uint16_t mat_id = 0;
        std::uint16_t v3 = 0;
    };
    std::vector<RawTri> fallback_tris;

    if (geom.is_native_unsupported) {
        warnings.push_back(DffWarning{
            true,
            "Geometry uses rpGEOMETRYNATIVE (platform-specific compiled buffer), which is not supported for vertex extraction."
        });
    } else {
        if (geom.has_prelit) {
            if (scur->remaining() < static_cast<std::size_t>(geom.num_vertices) * 4U) {
                warnings.push_back(DffWarning{true, "Truncated Geometry prelit vertex colors."});
                return false;
            }
            geom.prelit_colors.resize(geom.num_vertices);
            for (std::uint32_t i = 0; i < geom.num_vertices; ++i) {
                (void)scur->read_u8(geom.prelit_colors[i].r);
                (void)scur->read_u8(geom.prelit_colors[i].g);
                (void)scur->read_u8(geom.prelit_colors[i].b);
                (void)scur->read_u8(geom.prelit_colors[i].a);
            }
        }

        if (tex_sets > 0U) {
            const std::size_t uv_bytes = static_cast<std::size_t>(geom.num_vertices) * 8U;
            if (scur->remaining() < uv_bytes * static_cast<std::size_t>(tex_sets)) {
                warnings.push_back(DffWarning{true, "Truncated Geometry UV coordinates."});
                return false;
            }
            geom.uvs.resize(geom.num_vertices);
            for (std::uint32_t i = 0; i < geom.num_vertices; ++i) {
                (void)scur->read_f32(geom.uvs[i].x);
                (void)scur->read_f32(geom.uvs[i].y);
            }
            if (tex_sets > 1U) {
                (void)scur->skip(uv_bytes * static_cast<std::size_t>(tex_sets - 1U));
            }
        }

        if (geom.num_triangles_declared > 0U) {
            if (scur->remaining() < static_cast<std::size_t>(geom.num_triangles_declared) * 8U) {
                warnings.push_back(DffWarning{true, "Truncated Geometry triangle table."});
                return false;
            }
            fallback_tris.resize(geom.num_triangles_declared);
            for (std::uint32_t i = 0; i < geom.num_triangles_declared; ++i) {
                std::uint16_t a = 0, b = 0, m = 0, c = 0;
                (void)scur->read_u16(a);
                (void)scur->read_u16(b);
                (void)scur->read_u16(m);
                (void)scur->read_u16(c);
                // RW binary order: vert2, vert1, matId, vert3
                fallback_tris[i] = RawTri{b, a, m, c};
            }
        }
    }

    // Morph targets
    for (std::uint32_t mt = 0; mt < geom.num_morph_targets; ++mt) {
        Vec3f bs_center{};
        float bs_radius = 0.0f;
        std::uint32_t mt_has_verts = 0;
        std::uint32_t mt_has_norms = 0;
        if (!scur->read_f32(bs_center.x) || !scur->read_f32(bs_center.y) || !scur->read_f32(bs_center.z) ||
            !scur->read_f32(bs_radius) || !scur->read_u32(mt_has_verts) || !scur->read_u32(mt_has_norms)) {
            warnings.push_back(DffWarning{true, "Truncated Geometry morph target header."});
            return false;
        }
        if (mt == 0U) {
            geom.bounding_sphere_center = bs_center;
            geom.bounding_sphere_radius = bs_radius;
        }

        const std::size_t vec3_bytes = static_cast<std::size_t>(geom.num_vertices) * 12U;
        if (mt_has_verts != 0U) {
            if (scur->remaining() < vec3_bytes) {
                warnings.push_back(DffWarning{true, "Truncated Geometry vertex positions."});
                return false;
            }
            if (mt == 0U) {
                geom.positions.resize(geom.num_vertices);
                for (std::uint32_t i = 0; i < geom.num_vertices; ++i) {
                    (void)scur->read_f32(geom.positions[i].x);
                    (void)scur->read_f32(geom.positions[i].y);
                    (void)scur->read_f32(geom.positions[i].z);
                }
            } else {
                (void)scur->skip(vec3_bytes);
            }
        }
        if (mt_has_norms != 0U) {
            if (scur->remaining() < vec3_bytes) {
                warnings.push_back(DffWarning{true, "Truncated Geometry vertex normals."});
                return false;
            }
            if (mt == 0U) {
                geom.normals.resize(geom.num_vertices);
                for (std::uint32_t i = 0; i < geom.num_vertices; ++i) {
                    (void)scur->read_f32(geom.normals[i].x);
                    (void)scur->read_f32(geom.normals[i].y);
                    (void)scur->read_f32(geom.normals[i].z);
                }
                geom.has_normals = true;
            } else {
                (void)scur->skip(vec3_bytes);
            }
        }
    }

    // Read MaterialList and Extension inside Geometry
    while (!gcur.at_end()) {
        RwChunkHeader ch{};
        auto child = gcur.read_chunk_payload(ch);
        if (!child.has_value()) {
            warnings.push_back(DffWarning{false, "Truncated sub-chunk inside Geometry."});
            break;
        }
        if (ch.type == kRwMatList) {
            parse_material_list(*child, geom.materials, warnings);
        } else if (ch.type == kRwExtension) {
            while (!child->at_end()) {
                RwChunkHeader eh{};
                auto ext = child->read_chunk_payload(eh);
                if (!ext.has_value()) {
                    warnings.push_back(DffWarning{false, "Truncated plugin chunk inside Geometry Extension."});
                    break;
                }
                if (eh.type == kRwBinMeshPlg && !geom.is_native_unsupported) {
                    parse_binmesh_plg(*ext, geom, warnings);
                }
            }
        }
    }

    // Fallback to RpTriangle list if BinMesh PLG yielded no splits
    if (geom.splits.empty() && !fallback_tris.empty() && !geom.is_native_unsupported) {
        std::unordered_map<std::uint16_t, std::vector<std::uint32_t>> grouped;
        for (const auto& t : fallback_tris) {
            if (t.v1 < geom.num_vertices && t.v2 < geom.num_vertices && t.v3 < geom.num_vertices) {
                auto& dst = grouped[t.mat_id];
                dst.push_back(t.v1);
                dst.push_back(t.v2);
                dst.push_back(t.v3);
            }
        }
        for (auto& [mat_id, idxs] : grouped) {
            geom.splits.push_back(DffMeshSplit{mat_id, std::move(idxs)});
        }
    }

    if (!geom.is_native_unsupported && !geom.positions.empty()) {
        if (geom.normals.size() != geom.positions.size()) {
            generate_smooth_normals(geom);
            warnings.push_back(DffWarning{false, "Geometry lacked vertex normals; smooth normals generated for shading."});
        }
        if (geom.uvs.size() != geom.positions.size()) {
            geom.uvs.assign(geom.positions.size(), Vec2f{0.0f, 0.0f});
        }
    }

    return true;
}

void parse_frame_list(RwCursor fl_cur, DffModel& model) {
    RwChunkHeader shdr{};
    auto scur = fl_cur.read_chunk_payload(shdr);
    if (!scur.has_value() || shdr.type != kRwStruct) {
        model.warnings.push_back(DffWarning{false, "FrameList missing Struct."});
        return;
    }

    std::uint32_t frame_count = 0;
    if (!scur->read_u32(frame_count) || frame_count > 4096U) {
        model.warnings.push_back(DffWarning{false, "Invalid or truncated FrameList count."});
        return;
    }

    model.frames.resize(frame_count);
    for (std::uint32_t i = 0; i < frame_count; ++i) {
        auto& f = model.frames[i];
        bool ok = true;
        for (std::size_t r = 0; r < 9U; ++r) {
            ok = ok && scur->read_f32(f.rotation[r]);
        }
        ok = ok && scur->read_f32(f.position.x) && scur->read_f32(f.position.y) && scur->read_f32(f.position.z);
        ok = ok && scur->read_i32(f.parent_index) && scur->read_u32(f.flags);
        if (!ok) {
            model.warnings.push_back(DffWarning{false, "Truncated FrameList entry."});
            model.frames.resize(i);
            break;
        }
    }

    std::size_t ext_idx = 0;
    while (!fl_cur.at_end() && ext_idx < model.frames.size()) {
        RwChunkHeader eh{};
        auto ext = fl_cur.read_chunk_payload(eh);
        if (!ext.has_value()) {
            break;
        }
        if (eh.type == kRwExtension) {
            while (!ext->at_end()) {
                RwChunkHeader ph{};
                auto pcur = ext->read_chunk_payload(ph);
                if (!pcur.has_value()) break;
                if (ph.type == kRwFrameName) {
                    parse_string_chunk_text(*pcur, model.frames[ext_idx].name);
                }
            }
            ++ext_idx;
        }
    }

    compute_frame_world_matrices(model.frames, model.warnings);
}

void parse_geometry_list(RwCursor gl_cur, DffModel& model) {
    RwChunkHeader shdr{};
    auto scur = gl_cur.read_chunk_payload(shdr);
    if (!scur.has_value() || shdr.type != kRwStruct) {
        model.warnings.push_back(DffWarning{true, "GeometryList missing Struct."});
        return;
    }
    std::uint32_t count = 0;
    (void)scur->read_u32(count);

    while (!gl_cur.at_end()) {
        RwChunkHeader ghdr{};
        auto gcur = gl_cur.read_chunk_payload(ghdr);
        if (!gcur.has_value()) {
            model.warnings.push_back(DffWarning{true, "Truncated Geometry inside GeometryList."});
            break;
        }
        if (ghdr.type == kRwGeometry) {
            DffGeometry geom;
            if (parse_geometry_chunk(*gcur, ghdr, geom, model.warnings)) {
                model.geometries.push_back(std::move(geom));
            }
        }
    }
    if (count != 0U && model.geometries.size() != count) {
        model.warnings.push_back(DffWarning{false, "GeometryList count mismatch against parsed geometries."});
    }
}

void parse_atomic(RwCursor acur, DffModel& model) {
    RwChunkHeader shdr{};
    auto scur = acur.read_chunk_payload(shdr);
    if (!scur.has_value() || shdr.type != kRwStruct) {
        return;
    }
    DffAtomic a;
    if (scur->read_i32(a.frame_index) &&
        scur->read_i32(a.geometry_index) &&
        scur->read_u32(a.flags)) {
        model.atomics.push_back(a);
    }
}

void finalize_bounds_and_stats(DffModel& model) {
    bool any_pt = false;
    Vec3f g_min{
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()
    };
    Vec3f g_max{
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()
    };

    auto expand_bounds = [](DffBounds& b, const Vec3f& pt) {
        if (!b.valid) {
            b.min = pt;
            b.max = pt;
            b.valid = true;
        } else {
            b.min.x = std::min(b.min.x, pt.x);
            b.min.y = std::min(b.min.y, pt.y);
            b.min.z = std::min(b.min.z, pt.z);
            b.max.x = std::max(b.max.x, pt.x);
            b.max.y = std::max(b.max.y, pt.y);
            b.max.z = std::max(b.max.z, pt.z);
        }
    };

    std::unordered_set<std::string> seen_tex;
    for (auto& geom : model.geometries) {
        model.total_vertices += geom.positions.size();
        model.total_triangles += geom.renderable_triangle_count();
        model.total_materials += geom.materials.size();
        for (const auto& p : geom.positions) {
            expand_bounds(geom.bounds, p);
        }
        if (geom.bounds.valid) {
            geom.bounds.center = Vec3f{
                0.5f * (geom.bounds.min.x + geom.bounds.max.x),
                0.5f * (geom.bounds.min.y + geom.bounds.max.y),
                0.5f * (geom.bounds.min.z + geom.bounds.max.z)
            };
            const float dx = geom.bounds.max.x - geom.bounds.center.x;
            const float dy = geom.bounds.max.y - geom.bounds.center.y;
            const float dz = geom.bounds.max.z - geom.bounds.center.z;
            geom.bounds.radius = std::sqrt(dx * dx + dy * dy + dz * dz);
        }
        for (const auto& mat : geom.materials) {
            if (!mat.texture_name.empty()) {
                const std::string low = to_ascii_lower(mat.texture_name);
                if (seen_tex.insert(low).second) {
                    model.referenced_textures.push_back(mat.texture_name);
                }
            }
        }
    }

    auto apply_geom_with_mat = [&](std::size_t gidx, const Mat4f& xform) {
        if (gidx >= model.geometries.size()) return;
        for (const auto& p : model.geometries[gidx].positions) {
            const Vec3f wp = transform_point(xform, p);
            any_pt = true;
            g_min.x = std::min(g_min.x, wp.x);
            g_min.y = std::min(g_min.y, wp.y);
            g_min.z = std::min(g_min.z, wp.z);
            g_max.x = std::max(g_max.x, wp.x);
            g_max.y = std::max(g_max.y, wp.y);
            g_max.z = std::max(g_max.z, wp.z);
        }
    };

    if (!model.atomics.empty()) {
        for (const auto& a : model.atomics) {
            Mat4f xf{};
            if (a.frame_index >= 0 && static_cast<std::size_t>(a.frame_index) < model.frames.size()) {
                xf = model.frames[static_cast<std::size_t>(a.frame_index)].world_matrix;
            }
            if (a.geometry_index >= 0) {
                apply_geom_with_mat(static_cast<std::size_t>(a.geometry_index), xf);
            }
        }
    } else {
        for (std::size_t i = 0; i < model.geometries.size(); ++i) {
            apply_geom_with_mat(i, Mat4f{});
        }
    }

    if (any_pt) {
        model.bounds.min = g_min;
        model.bounds.max = g_max;
        model.bounds.center = Vec3f{
            0.5f * (g_min.x + g_max.x),
            0.5f * (g_min.y + g_max.y),
            0.5f * (g_min.z + g_max.z)
        };
        const float dx = g_max.x - model.bounds.center.x;
        const float dy = g_max.y - model.bounds.center.y;
        const float dz = g_max.z - model.bounds.center.z;
        model.bounds.radius = std::sqrt(dx * dx + dy * dy + dz * dz);
        model.bounds.valid = true;
    }
}

} // namespace

Result<DffModel> DffParser::parse_bytes(
    std::span<const std::uint8_t> bytes,
    std::string source_name) {
    if (bytes.size() < kRwChunkHeaderSize) {
        return Result<DffModel>::failure("File is too small to contain a RenderWare chunk header.");
    }

    RwCursor root(bytes);
    RwChunkHeader top_hdr{};
    auto top_cur = root.read_chunk_payload(top_hdr);
    if (!top_cur.has_value()) {
        return Result<DffModel>::failure(
            "Truncated or malformed top-level RenderWare chunk in DFF.");
    }

    DffModel model;
    model.source_name = std::move(source_name);
    model.file_size_bytes = bytes.size();
    model.rw_version = top_hdr.rw_version;

    if (top_hdr.type == kRwClump) {
        while (!top_cur->at_end()) {
            RwChunkHeader ch{};
            auto child = top_cur->read_chunk_payload(ch);
            if (!child.has_value()) {
                model.warnings.push_back(DffWarning{true, "Truncated child chunk inside Clump."});
                break;
            }
            if (ch.type == kRwFrameList) {
                parse_frame_list(*child, model);
            } else if (ch.type == kRwGeometryList) {
                parse_geometry_list(*child, model);
            } else if (ch.type == kRwAtomic) {
                parse_atomic(*child, model);
            }
        }
    } else if (top_hdr.type == kRwGeometry) {
        DffGeometry geom;
        if (parse_geometry_chunk(*top_cur, top_hdr, geom, model.warnings)) {
            model.geometries.push_back(std::move(geom));
        }
    } else {
        return Result<DffModel>::failure(
            "Unsupported top-level DFF chunk (expected Clump 0x10 or Geometry 0x0F).",
            "Chunk type: 0x" + std::to_string(top_hdr.type));
    }

    finalize_bounds_and_stats(model);
    if (model.geometries.empty()) {
        return Result<DffModel>::failure("DFF archive contains no parseable Geometry chunks.");
    }

    return Result<DffModel>::success(std::move(model));
}

Result<DffModel> DffParser::parse_file(const std::filesystem::path& path) {
    std::error_code ec;
    const std::uint64_t fsize = std::filesystem::file_size(path, ec);
    if (ec) {
        return Result<DffModel>::failure("Failed to stat DFF file.", ec.message());
    }
    if (fsize > 256ULL * 1024ULL * 1024ULL) {
        return Result<DffModel>::failure("DFF file exceeds 256 MiB safety limit.");
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return Result<DffModel>::failure("Failed to open DFF file.", path.string());
    }

    std::vector<std::uint8_t> buf(static_cast<std::size_t>(fsize));
    if (fsize > 0ULL) {
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(fsize));
        if (!in || static_cast<std::uint64_t>(in.gcount()) != fsize) {
            return Result<DffModel>::failure("Failed to read complete DFF file.");
        }
    }

    return parse_bytes(buf, path.filename().string());
}

} // namespace imgstudio::core::rw
