#include <catch2/catch_test_macros.hpp>

#include "RwTestHelpers.hpp"
#include "core/rw/DffParser.hpp"

using namespace imgstudio::core::rw;
using namespace imgstudio::tests;

TEST_CASE("DffParser parses valid Clump, Geometry, Materials, UVs, and BinMesh list/strip", "[dff]") {
    SyntheticDffSpec spec;
    spec.frame_name = "infernus_body";
    spec.texture_name = "infernus92body";
    spec.use_tristrip_binmesh = false;
    spec.include_normals = true;

    const auto bytes = build_synthetic_dff(spec);
    auto res = DffParser::parse_bytes(bytes, "infernus.dff");
    REQUIRE(res.ok());
    const auto& m = *res.value;
    REQUIRE(m.rw_version == 0x36003U);
    REQUIRE(m.frames.size() == 1);
    REQUIRE(m.frames[0].name == "infernus_body");
    REQUIRE(m.geometries.size() == 1);
    REQUIRE(m.total_vertices == 4);
    REQUIRE(m.total_triangles == 2);
    REQUIRE(m.total_materials == 1);
    REQUIRE(m.referenced_textures.size() == 1);
    REQUIRE(m.referenced_textures[0] == "infernus92body");
    REQUIRE(m.bounds.valid);
    REQUIRE(m.bounds.min.x == -1.0f);
    REQUIRE(m.bounds.max.x == 1.0f);

    // Strip variant
    spec.use_tristrip_binmesh = true;
    spec.include_normals = false;
    const auto strip_bytes = build_synthetic_dff(spec);
    auto strip_res = DffParser::parse_bytes(strip_bytes, "strip.dff");
    REQUIRE(strip_res.ok());
    REQUIRE(strip_res.value->total_triangles == 2);
    REQUIRE(strip_res.value->geometries[0].normals.size() == 4);
    REQUIRE(!strip_res.value->warnings.empty()); // smooth normals warning
}

TEST_CASE("DffParser handles truncated, invalid, and native geometry DFF safely", "[dff][faults]") {
    REQUIRE(!DffParser::parse_bytes({}, "empty.dff").ok());

    const std::vector<std::uint8_t> bad_root = wrap_rw_chunk(0x9999U, {1, 2, 3, 4});
    REQUIRE(!DffParser::parse_bytes(bad_root, "bad_root.dff").ok());

    // Truncated Clump chunk claiming 10000 bytes
    std::vector<std::uint8_t> trunc = {0x10, 0x00, 0x00, 0x00, 0x10, 0x27, 0x00, 0x00, 0xFF, 0xFF, 0x03, 0x18};
    REQUIRE(!DffParser::parse_bytes(trunc, "trunc.dff").ok());

    // Native geometry flagged as unsupported without crash
    SyntheticDffSpec nat;
    nat.native_geometry_flag = true;
    auto nat_res = DffParser::parse_bytes(build_synthetic_dff(nat), "native.dff");
    REQUIRE(nat_res.ok());
    REQUIRE(nat_res.value->geometries[0].is_native_unsupported);
    REQUIRE(!nat_res.value->warnings.empty());
}
