#include "RwTestHelpers.hpp"
#include "core/BinaryIO.hpp"
#include "core/rw/RwStream.hpp"

#include <algorithm>
#include <cstring>

namespace imgstudio::tests {

namespace {

void append_u8(std::vector<std::uint8_t>& dst, std::uint8_t v) {
    dst.push_back(v);
}

void append_u16(std::vector<std::uint8_t>& dst, std::uint16_t v) {
    std::uint8_t b[2]{};
    core::write_u16_le(b, v);
    dst.insert(dst.end(), b, b + 2);
}

void append_u32(std::vector<std::uint8_t>& dst, std::uint32_t v) {
    std::uint8_t b[4]{};
    core::write_u32_le(b, v);
    dst.insert(dst.end(), b, b + 4);
}

void append_i32(std::vector<std::uint8_t>& dst, std::int32_t v) {
    std::uint32_t raw = 0;
    std::memcpy(&raw, &v, sizeof(raw));
    append_u32(dst, raw);
}

void append_f32(std::vector<std::uint8_t>& dst, float v) {
    std::uint32_t raw = 0;
    std::memcpy(&raw, &v, sizeof(raw));
    append_u32(dst, raw);
}

void append_fixed_str(std::vector<std::uint8_t>& dst, const std::string& s, std::size_t len) {
    std::vector<std::uint8_t> buf(len, 0U);
    const std::size_t n = std::min(s.size(), len - 1U);
    for (std::size_t i = 0; i < n; ++i) {
        buf[i] = static_cast<std::uint8_t>(s[i]);
    }
    dst.insert(dst.end(), buf.begin(), buf.end());
}

void append_bytes(std::vector<std::uint8_t>& dst, const std::vector<std::uint8_t>& src) {
    dst.insert(dst.end(), src.begin(), src.end());
}

std::vector<std::uint8_t> make_rw_string(const std::string& s) {
    std::vector<std::uint8_t> p(s.begin(), s.end());
    p.push_back(0U);
    while ((p.size() % 4U) != 0U) {
        p.push_back(0U);
    }
    return wrap_rw_chunk(core::rw::kRwString, p);
}

} // namespace

std::vector<std::uint8_t> wrap_rw_chunk(
    std::uint32_t type,
    const std::vector<std::uint8_t>& payload,
    std::uint32_t library_id) {
    std::vector<std::uint8_t> out;
    out.reserve(12U + payload.size());
    append_u32(out, type);
    append_u32(out, static_cast<std::uint32_t>(payload.size()));
    append_u32(out, library_id);
    out.insert(out.end(), payload.begin(), payload.end());
    return out;
}

std::vector<std::uint8_t> build_synthetic_dff(const SyntheticDffSpec& spec) {
    // 1. Clump Struct: numAtomics=1, numLights=0, numCameras=0
    std::vector<std::uint8_t> clump_struct;
    append_u32(clump_struct, 1U);
    append_u32(clump_struct, 0U);
    append_u32(clump_struct, 0U);
    const auto clump_struct_chunk = wrap_rw_chunk(core::rw::kRwStruct, clump_struct);

    // 2. FrameList: 1 identity frame + FrameName extension
    std::vector<std::uint8_t> fl_struct;
    append_u32(fl_struct, 1U);
    const float rot[9] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    for (float r : rot) append_f32(fl_struct, r);
    append_f32(fl_struct, 0.0f); append_f32(fl_struct, 0.0f); append_f32(fl_struct, 0.0f);
    append_i32(fl_struct, -1);
    append_u32(fl_struct, 0U);

    std::vector<std::uint8_t> fname_bytes(spec.frame_name.begin(), spec.frame_name.end());
    const auto fname_chunk = wrap_rw_chunk(core::rw::kRwFrameName, fname_bytes);
    const auto frame_ext = wrap_rw_chunk(core::rw::kRwExtension, fname_chunk);

    std::vector<std::uint8_t> fl_payload;
    append_bytes(fl_payload, wrap_rw_chunk(core::rw::kRwStruct, fl_struct));
    append_bytes(fl_payload, frame_ext);
    const auto frame_list_chunk = wrap_rw_chunk(core::rw::kRwFrameList, fl_payload);

    // 3. Geometry: simple quad (4 vertices, 2 triangles)
    std::uint32_t geom_flags = 0x0002U | 0x0004U | 0x0020U | 0x0040U | (1U << 16U);
    if (spec.include_normals) geom_flags |= 0x0010U;
    if (spec.use_tristrip_binmesh) geom_flags |= 0x0001U;
    if (spec.native_geometry_flag) geom_flags |= 0x01000000U;

    std::vector<std::uint8_t> g_struct;
    append_u32(g_struct, geom_flags);
    append_u32(g_struct, 2U); // numTriangles
    append_u32(g_struct, 4U); // numVertices
    append_u32(g_struct, 1U); // numMorphTargets

    if (!spec.native_geometry_flag) {
        // UVs
        const float uvs[8] = {0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f};
        for (float u : uvs) append_f32(g_struct, u);
        // 2 RpTriangles (v2, v1, matId, v3)
        append_u16(g_struct, 1); append_u16(g_struct, 0); append_u16(g_struct, 0); append_u16(g_struct, 2);
        append_u16(g_struct, 2); append_u16(g_struct, 0); append_u16(g_struct, 0); append_u16(g_struct, 3);
    }

    // Morph target
    append_f32(g_struct, 0.0f); append_f32(g_struct, 0.0f); append_f32(g_struct, 0.0f);
    append_f32(g_struct, 2.0f);
    append_u32(g_struct, spec.native_geometry_flag ? 0U : 1U);
    append_u32(g_struct, (!spec.native_geometry_flag && spec.include_normals) ? 1U : 0U);
    if (!spec.native_geometry_flag) {
        const float pos[12] = {
            -1.0f, -1.0f, 0.0f,
             1.0f, -1.0f, 0.0f,
             1.0f,  1.0f, 0.0f,
            -1.0f,  1.0f, 0.0f
        };
        for (float p : pos) append_f32(g_struct, p);
        if (spec.include_normals) {
            for (int i = 0; i < 4; ++i) {
                append_f32(g_struct, 0.0f);
                append_f32(g_struct, 0.0f);
                append_f32(g_struct, 1.0f);
            }
        }
    }

    // MaterialList
    std::vector<std::uint8_t> ml_struct;
    append_u32(ml_struct, 1U);
    append_i32(ml_struct, -1);

    std::vector<std::uint8_t> mat_struct;
    append_u32(mat_struct, 0U);
    append_u8(mat_struct, 240); append_u8(mat_struct, 220); append_u8(mat_struct, 200); append_u8(mat_struct, 255);
    append_u32(mat_struct, 0U);
    append_u32(mat_struct, spec.texture_name.empty() ? 0U : 1U);
    append_f32(mat_struct, 1.0f); append_f32(mat_struct, 0.5f); append_f32(mat_struct, 1.0f);

    std::vector<std::uint8_t> mat_payload;
    append_bytes(mat_payload, wrap_rw_chunk(core::rw::kRwStruct, mat_struct));
    if (!spec.texture_name.empty()) {
        std::vector<std::uint8_t> tex_struct;
        append_u32(tex_struct, 0x1102U);
        std::vector<std::uint8_t> tex_payload;
        append_bytes(tex_payload, wrap_rw_chunk(core::rw::kRwStruct, tex_struct));
        append_bytes(tex_payload, make_rw_string(spec.texture_name));
        append_bytes(tex_payload, make_rw_string(spec.mask_name));
        append_bytes(mat_payload, wrap_rw_chunk(core::rw::kRwTexture, tex_payload));
    }
    const auto mat_chunk = wrap_rw_chunk(core::rw::kRwMaterial, mat_payload);

    std::vector<std::uint8_t> ml_payload;
    append_bytes(ml_payload, wrap_rw_chunk(core::rw::kRwStruct, ml_struct));
    append_bytes(ml_payload, mat_chunk);
    const auto mat_list_chunk = wrap_rw_chunk(core::rw::kRwMatList, ml_payload);

    // Geometry Extension with BinMesh PLG
    std::vector<std::uint8_t> binmesh;
    if (spec.use_tristrip_binmesh) {
        append_u32(binmesh, 1U); // strip
        append_u32(binmesh, 1U);
        append_u32(binmesh, 4U);
        append_u32(binmesh, 4U);
        append_u32(binmesh, 0U);
        append_u32(binmesh, 0U); append_u32(binmesh, 1U); append_u32(binmesh, 3U); append_u32(binmesh, 2U);
    } else {
        append_u32(binmesh, 0U); // list
        append_u32(binmesh, 1U);
        append_u32(binmesh, 6U);
        append_u32(binmesh, 6U);
        append_u32(binmesh, 0U);
        append_u32(binmesh, 0U); append_u32(binmesh, 1U); append_u32(binmesh, 2U);
        append_u32(binmesh, 0U); append_u32(binmesh, 2U); append_u32(binmesh, 3U);
    }
    const auto geom_ext_chunk = wrap_rw_chunk(
        core::rw::kRwExtension,
        wrap_rw_chunk(core::rw::kRwBinMeshPlg, binmesh));

    std::vector<std::uint8_t> geom_payload;
    append_bytes(geom_payload, wrap_rw_chunk(core::rw::kRwStruct, g_struct));
    append_bytes(geom_payload, mat_list_chunk);
    append_bytes(geom_payload, geom_ext_chunk);
    const auto geom_chunk = wrap_rw_chunk(core::rw::kRwGeometry, geom_payload);

    // GeometryList
    std::vector<std::uint8_t> gl_struct;
    append_u32(gl_struct, 1U);
    std::vector<std::uint8_t> gl_payload;
    append_bytes(gl_payload, wrap_rw_chunk(core::rw::kRwStruct, gl_struct));
    append_bytes(gl_payload, geom_chunk);
    const auto geom_list_chunk = wrap_rw_chunk(core::rw::kRwGeometryList, gl_payload);

    // Atomic
    std::vector<std::uint8_t> at_struct;
    append_i32(at_struct, 0);
    append_i32(at_struct, 0);
    append_u32(at_struct, 5U);
    append_u32(at_struct, 0U);
    const auto atomic_chunk = wrap_rw_chunk(core::rw::kRwAtomic, wrap_rw_chunk(core::rw::kRwStruct, at_struct));

    std::vector<std::uint8_t> clump_payload;
    append_bytes(clump_payload, clump_struct_chunk);
    append_bytes(clump_payload, frame_list_chunk);
    append_bytes(clump_payload, geom_list_chunk);
    append_bytes(clump_payload, atomic_chunk);
    return wrap_rw_chunk(core::rw::kRwClump, clump_payload);
}

std::vector<std::uint8_t> build_synthetic_txd(const std::vector<SyntheticTxdTextureSpec>& textures) {
    std::vector<std::uint8_t> dict_struct;
    append_u16(dict_struct, static_cast<std::uint16_t>(textures.size()));
    append_u16(dict_struct, 2U); // D3D9

    std::vector<std::uint8_t> dict_payload;
    append_bytes(dict_payload, wrap_rw_chunk(core::rw::kRwStruct, dict_struct));

    for (const auto& t : textures) {
        std::vector<std::uint8_t> ts;
        append_u32(ts, t.platform_id);
        append_u32(ts, 0x1102U);
        append_fixed_str(ts, t.name, 32U);
        append_fixed_str(ts, t.mask_name, 32U);

        std::uint32_t raster_fmt = 0x0500U; // 8888
        std::uint32_t d3d_fmt = 21U;        // D3DFMT_A8R8G8B8
        std::uint8_t depth = 32U;
        std::uint8_t flags = t.has_alpha ? 0x01U : 0x00U;

        if (t.format_kind == 1) {
            raster_fmt = 0x0100U | (t.mipmap_count > 1U ? 0x8000U : 0U);
            d3d_fmt = 0x31545844U; // DXT1
            depth = 16U;
            flags |= 0x08U;
        } else if (t.format_kind == 2) {
            raster_fmt = 0x0300U | (t.mipmap_count > 1U ? 0x8000U : 0U);
            d3d_fmt = 0x33545844U; // DXT3
            depth = 16U;
            flags = 0x09U;
        } else if (t.format_kind == 3) {
            raster_fmt = 0x0300U | (t.mipmap_count > 1U ? 0x8000U : 0U);
            d3d_fmt = 0x35545844U; // DXT5
            depth = 16U;
            flags = 0x09U;
        } else if (t.format_kind == 4) {
            raster_fmt = 0x2500U; // PAL8 + 8888
            d3d_fmt = 41U;
            depth = 8U;
        } else if (t.format_kind == 5) {
            raster_fmt = 0x0200U; // 565
            d3d_fmt = 23U;
            depth = 16U;
            flags = 0U;
        }

        append_u32(ts, raster_fmt);
        append_u32(ts, d3d_fmt);
        append_u16(ts, t.width);
        append_u16(ts, t.height);
        append_u8(ts, depth);
        append_u8(ts, t.mipmap_count);
        append_u8(ts, 4U); // rasterType
        append_u8(ts, flags);

        if (t.format_kind == 4) {
            // 256-entry RGBA palette
            for (int i = 0; i < 256; ++i) {
                append_u8(ts, static_cast<std::uint8_t>(i));
                append_u8(ts, static_cast<std::uint8_t>(255 - i));
                append_u8(ts, 128U);
                append_u8(ts, static_cast<std::uint8_t>((i == 0) ? 0 : 255));
            }
        }

        if (t.format_kind != 6) {
            std::uint16_t mw = t.width;
            std::uint16_t mh = t.height;
            for (std::uint8_t lvl = 0; lvl < t.mipmap_count; ++lvl) {
                const std::uint32_t bw = std::max<std::uint32_t>(1U, (mw + 3U) / 4U);
                const std::uint32_t bh = std::max<std::uint32_t>(1U, (mh + 3U) / 4U);
                if (t.format_kind == 0) {
                    const std::uint32_t sz = static_cast<std::uint32_t>(mw) * mh * 4U;
                    append_u32(ts, sz);
                    for (std::uint32_t p = 0; p < static_cast<std::uint32_t>(mw) * mh; ++p) {
                        append_u8(ts, 40U);  // B
                        append_u8(ts, 180U); // G
                        append_u8(ts, 220U); // R
                        append_u8(ts, t.has_alpha ? 128U : 255U); // A
                    }
                } else if (t.format_kind == 1) {
                    const std::uint32_t sz = bw * bh * 8U;
                    append_u32(ts, sz);
                    for (std::uint32_t b = 0; b < bw * bh; ++b) {
                        // c0=0xF800 (pure red), c1=0x07E0 (pure green), code=0 everywhere
                        append_u16(ts, 0xF800U);
                        append_u16(ts, 0x07E0U);
                        append_u32(ts, 0x00000000U);
                    }
                } else if (t.format_kind == 2) {
                    const std::uint32_t sz = bw * bh * 16U;
                    append_u32(ts, sz);
                    for (std::uint32_t b = 0; b < bw * bh; ++b) {
                        // DXT3 explicit 4-bit alpha = 0x8 (136) for every pixel
                        for (int k = 0; k < 8; ++k) append_u8(ts, 0x88U);
                        append_u16(ts, 0xFFFFU);
                        append_u16(ts, 0x0000U);
                        append_u32(ts, 0x00000000U);
                    }
                } else if (t.format_kind == 3) {
                    const std::uint32_t sz = bw * bh * 16U;
                    append_u32(ts, sz);
                    for (std::uint32_t b = 0; b < bw * bh; ++b) {
                        // DXT5 a0=200, a1=100, index=0 everywhere
                        append_u8(ts, 200U);
                        append_u8(ts, 100U);
                        for (int k = 0; k < 6; ++k) append_u8(ts, 0x00U);
                        append_u16(ts, 0x001FU); // blue
                        append_u16(ts, 0x0000U);
                        append_u32(ts, 0x00000000U);
                    }
                } else if (t.format_kind == 4) {
                    const std::uint32_t sz = static_cast<std::uint32_t>(mw) * mh;
                    append_u32(ts, sz);
                    for (std::uint32_t p = 0; p < sz; ++p) {
                        append_u8(ts, static_cast<std::uint8_t>(p % 256U));
                    }
                } else if (t.format_kind == 5) {
                    const std::uint32_t sz = static_cast<std::uint32_t>(mw) * mh * 2U;
                    append_u32(ts, sz);
                    for (std::uint32_t p = 0; p < static_cast<std::uint32_t>(mw) * mh; ++p) {
                        append_u16(ts, 0x07E0U); // green
                    }
                }
                mw = static_cast<std::uint16_t>(std::max<int>(1, static_cast<int>(mw) / 2));
                mh = static_cast<std::uint16_t>(std::max<int>(1, static_cast<int>(mh) / 2));
            }
        }

        std::vector<std::uint8_t> tn_payload;
        append_bytes(tn_payload, wrap_rw_chunk(core::rw::kRwStruct, ts));
        append_bytes(tn_payload, wrap_rw_chunk(core::rw::kRwExtension, {}));
        append_bytes(dict_payload, wrap_rw_chunk(core::rw::kRwTextureNative, tn_payload));
    }

    return wrap_rw_chunk(core::rw::kRwTexDictionary, dict_payload);
}

} // namespace imgstudio::tests
