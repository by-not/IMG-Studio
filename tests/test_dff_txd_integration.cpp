#include <catch2/catch_test_macros.hpp>

#include "RwTestHelpers.hpp"
#include "TestHelpers.hpp"
#include "app/ArchiveDocument.hpp"
#include "app/AssetLoader.hpp"

using namespace imgstudio::app;
using namespace imgstudio::tests;

TEST_CASE("AssetLoader loads DFF and resolves matching TXD directly from IMG archive", "[integration][dff][txd]") {
    TempDirGuard tmp("dff_txd_int");
    const auto img_path = tmp.path() / "vehicles.img";

    const auto dff_bytes = build_synthetic_dff(SyntheticDffSpec{
        "chassis", "sultan92body", "", false, true, false
    });
    const auto txd_bytes = build_synthetic_txd({
        {"sultan92body", "", 9U, 8, 8, 2, 1, false}
    });
    const auto extra_dff = build_synthetic_dff(SyntheticDffSpec{
        "chassis", "missing_tex", "", false, true, false
    });

    write_synthetic_img(img_path, {
        {"sultan.dff", dff_bytes},
        {"sultan.txd", txd_bytes},
        {"orphan.dff", extra_dff}
    });

    auto doc_res = ArchiveDocument::open(img_path);
    REQUIRE(doc_res.ok());
    const auto& doc = *doc_res.value;

    auto loaded = AssetLoader::load_dff_from_document_sync(doc, doc.entries()[0].id);
    REQUIRE(loaded.ok());
    REQUIRE(loaded.value->loaded_txds.size() == 1);
    const auto it = loaded.value->resolved_textures.find("sultan92body");
    REQUIRE(it != loaded.value->resolved_textures.end());
    REQUIRE(it->second.resolved);
    REQUIRE(it->second.source_txd_name == "sultan.txd");
    REQUIRE(it->second.texture != nullptr);
    REQUIRE(it->second.texture->width == 8);

    // Missing texture reference reported cleanly
    auto orphan_loaded = AssetLoader::load_dff_from_document_sync(doc, doc.entries()[2].id);
    REQUIRE(orphan_loaded.ok());
    const auto mit = orphan_loaded.value->resolved_textures.find("missing_tex");
    REQUIRE(mit != orphan_loaded.value->resolved_textures.end());
    REQUIRE(!mit->second.resolved);
}

TEST_CASE("AssetLoader resolves across multiple TXDs and handles large/repeated loads", "[integration][dff][txd]") {
    TempDirGuard tmp("dff_txd_multi");
    const auto img_path = tmp.path() / "multi.img";
    const auto extra_txd_path = tmp.path() / "custom_extra.txd";

    const auto dff_bytes = build_synthetic_dff(SyntheticDffSpec{
        "chassis", "custom_chrome", "", false, true, false
    });
    std::vector<SyntheticTxdTextureSpec> many_specs;
    for (int i = 0; i < 32; ++i) {
        many_specs.push_back(SyntheticTxdTextureSpec{
            "tex_" + std::to_string(i), "", 9U, 16, 16, 2, 1, false
        });
    }
    const auto txd_primary = build_synthetic_txd(many_specs);
    const auto txd_extra = build_synthetic_txd({
        {"custom_chrome", "", 9U, 32, 32, 2, 3, true}
    });

    write_raw_file(extra_txd_path, txd_extra);
    write_synthetic_img(img_path, {
        {"car.dff", dff_bytes},
        {"car.txd", txd_primary}
    });

    auto extra_dict_res = AssetLoader::load_txd_from_file_sync(extra_txd_path);
    REQUIRE(extra_dict_res.ok());
    REQUIRE((*extra_dict_res.value)->textures.size() == 1);

    auto doc_res = ArchiveDocument::open(img_path);
    REQUIRE(doc_res.ok());
    const auto& doc = *doc_res.value;

    for (int iter = 0; iter < 10; ++iter) {
        auto loaded = AssetLoader::load_dff_from_document_sync(
            doc,
            doc.entries()[0].id,
            {*extra_dict_res.value});
        REQUIRE(loaded.ok());
        REQUIRE(loaded.value->loaded_txds.size() == 2);
        const auto it = loaded.value->resolved_textures.find("custom_chrome");
        REQUIRE(it != loaded.value->resolved_textures.end());
        REQUIRE(it->second.resolved);
        REQUIRE(it->second.source_txd_name == "custom_extra.txd");
        REQUIRE(it->second.texture->width == 32);
    }
}
