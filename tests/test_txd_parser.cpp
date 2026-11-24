#include <catch2/catch_test_macros.hpp>

#include "RwTestHelpers.hpp"
#include "TestHelpers.hpp"
#include "core/rw/ImageExporter.hpp"
#include "core/rw/TxdParser.hpp"

using namespace imgstudio::core::rw;
using namespace imgstudio::tests;

TEST_CASE("TxdParser decodes BGRA8888, DXT1, DXT3, DXT5, PAL8, BGR565, and mipmaps", "[txd]") {
    const auto bytes = build_synthetic_txd({
        {"tex_8888", "", 9U, 4, 4, 2, 0, true},
        {"tex_dxt1", "", 9U, 4, 4, 2, 1, false},
        {"tex_dxt3", "tex_dxt3a", 9U, 4, 4, 1, 2, true},
        {"tex_dxt5", "", 9U, 4, 4, 1, 3, true},
        {"tex_pal8", "", 9U, 4, 4, 1, 4, true},
        {"tex_565",  "", 9U, 4, 4, 1, 5, false}
    });

    auto res = TxdParser::parse_bytes(bytes, "sample.txd");
    REQUIRE(res.ok());
    const auto& dict = *res.value;
    REQUIRE(dict.textures.size() == 6);

    // BGRA8888 -> RGBA (R=220, G=180, B=40, A=128) + 2 mipmaps (4x4, 2x2)
    REQUIRE(dict.textures[0].supported);
    REQUIRE(dict.textures[0].mipmaps.size() == 2);
    REQUIRE(dict.textures[0].mipmaps[0].rgba8[0] == 220);
    REQUIRE(dict.textures[0].mipmaps[0].rgba8[1] == 180);
    REQUIRE(dict.textures[0].mipmaps[0].rgba8[2] == 40);
    REQUIRE(dict.textures[0].mipmaps[0].rgba8[3] == 128);
    REQUIRE(dict.textures[0].mipmaps[1].width == 2);

    // DXT1 red
    REQUIRE(dict.textures[1].supported);
    REQUIRE(dict.textures[1].mipmaps.size() == 2);
    REQUIRE(dict.textures[1].mipmaps[0].rgba8[0] == 255);
    REQUIRE(dict.textures[1].mipmaps[0].rgba8[1] == 0);
    REQUIRE(dict.textures[1].mipmaps[0].rgba8[3] == 255);

    // DXT3 alpha preserved (0x88)
    REQUIRE(dict.textures[2].supported);
    REQUIRE(dict.textures[2].has_alpha);
    REQUIRE(dict.textures[2].mipmaps[0].rgba8[3] == 0x88);

    // DXT5 alpha preserved (200)
    REQUIRE(dict.textures[3].supported);
    REQUIRE(dict.textures[3].mipmaps[0].rgba8[3] == 200);

    // PAL8 pixel 0 has alpha 0, pixel 1 has alpha 255
    REQUIRE(dict.textures[4].supported);
    REQUIRE(dict.textures[4].mipmaps[0].rgba8[3] == 0);
    REQUIRE(dict.textures[4].mipmaps[0].rgba8[4 + 3] == 255);

    // Export PNG/TGA/BMP
    TempDirGuard tmp("txd_export");
    REQUIRE(ImageExporter::export_mip_to_file(
        dict.textures[0].mipmaps[0], tmp.path() / "out.png", ImageExportFormat::Png).ok());
    REQUIRE(ImageExporter::export_mip_to_file(
        dict.textures[0].mipmaps[0], tmp.path() / "out.tga", ImageExportFormat::Tga).ok());
    REQUIRE(ImageExporter::export_mip_to_file(
        dict.textures[0].mipmaps[0], tmp.path() / "out.bmp", ImageExportFormat::Bmp).ok());
    REQUIRE(std::filesystem::file_size(tmp.path() / "out.png") > 20U);
}

TEST_CASE("TxdParser reports unsupported platforms and handles truncated TXD safely", "[txd][faults]") {
    REQUIRE(!TxdParser::parse_bytes({}, "empty.txd").ok());

    const auto bad_plat = build_synthetic_txd({
        {"ps2_tex", "", 4U, 8, 8, 1, 6, false},
        {"ok_pc",   "", 9U, 4, 4, 1, 0, false}
    });
    auto res = TxdParser::parse_bytes(bad_plat, "mixed.txd");
    REQUIRE(res.ok());
    REQUIRE(res.value->textures.size() == 2);
    REQUIRE(!res.value->textures[0].supported);
    REQUIRE(res.value->textures[1].supported);

    // Truncated TXD payload
    auto full = build_synthetic_txd({{"a", "", 9U, 4, 4, 1, 0, false}});
    full.resize(full.size() / 2U);
    REQUIRE(!TxdParser::parse_bytes(full, "trunc.txd").ok());
}
