#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.hpp"
#include "core/ImgArchive.hpp"
#include "core/ImgExtractor.hpp"

using namespace imgstudio::core;
using namespace imgstudio::tests;

TEST_CASE("Extract individual, batch, and external staged items", "[extractor]") {
    TempDirGuard tmp("extract");
    const auto img_path = tmp.path() / "pack.img";
    const auto p1 = make_pattern_bytes(3000, 11);
    const auto p2 = make_pattern_bytes(2048, 22);
    const auto ext_bytes = make_pattern_bytes(1500, 33);
    const auto ext_file = write_raw_file(tmp.path() / "staged.col", ext_bytes);

    write_synthetic_img(img_path, {
        {"alpha.dff", p1},
        {"beta.txd", p2}
    });

    auto arch_res = ImgArchive::open(img_path);
    REQUIRE(arch_res.ok());

    const auto single_out = tmp.path() / "single_alpha.dff";
    REQUIRE(ImgExtractor::extract_entry_to_file(img_path, arch_res.value->entries()[0], single_out).ok());
    const auto single_read = read_raw_file(single_out);
    REQUIRE(single_read.size() == 4096); // sector-aligned size in IMG
    REQUIRE(std::equal(p1.begin(), p1.end(), single_read.begin()));

    const auto out_dir = tmp.path() / "out";
    std::filesystem::create_directories(out_dir);

    std::vector<ExtractionItem> items;
    for (const auto& e : arch_res.value->entries()) {
        items.push_back(ExtractionItem{
            e.name, false, {}, e.offset_bytes(), e.effective_bytes()
        });
    }
    items.push_back(ExtractionItem{
        "staged.col", true, ext_file, 0, ext_bytes.size()
    });
    items.push_back(ExtractionItem{
        "../unsafe.dff", false, {}, 0, 2048
    });

    auto batch = ImgExtractor::extract_items(img_path, items, out_dir);
    REQUIRE(batch.ok());
    REQUIRE(batch.value->requested == 4);
    REQUIRE(batch.value->succeeded == 3);
    REQUIRE(batch.value->failures.size() == 1);
    REQUIRE(read_raw_file(out_dir / "staged.col") == ext_bytes);
    REQUIRE(read_raw_file(out_dir / "beta.txd") == p2);
}
