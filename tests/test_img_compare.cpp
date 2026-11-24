#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.hpp"
#include "app/CompareService.hpp"
#include "core/ImgCompare.hpp"

using namespace imgstudio::core;
using namespace imgstudio::app;
using namespace imgstudio::tests;

TEST_CASE("ImgCompare classifies identical, added, removed, modified, metadata_diff, and ambiguous", "[compare]") {
    TempDirGuard tmp("compare");
    const auto img_a = tmp.path() / "a.img";
    const auto img_b = tmp.path() / "b.img";

    const auto p_same = make_pattern_bytes(2048, 1);
    const auto p_meta = make_pattern_bytes(2048, 2);

    write_synthetic_img(img_a, {
        {"same.dff", p_same, 1, 0, true},
        {"removed.txd", make_pattern_bytes(2048, 3)},
        {"changed.col", make_pattern_bytes(2048, 4)},
        {"meta.ifp", p_meta, 1, 0, true},
        {"dup.dff", make_pattern_bytes(2048, 5)},
        {"DUP.DFF", make_pattern_bytes(2048, 6)}
    });

    write_synthetic_img(img_b, {
        {"same.dff", p_same, 1, 0, true},
        {"added.txd", make_pattern_bytes(2048, 7)},
        {"changed.col", make_pattern_bytes(2048, 99)},
        {"META.IFP", p_meta, 1, 1, true},
        {"dup.dff", make_pattern_bytes(2048, 5)}
    });

    auto cmp = ImgCompare::compare(img_a, img_b);
    REQUIRE(cmp.ok());
    const auto& r = *cmp.value;
    REQUIRE(r.counts.unchanged == 1);
    REQUIRE(r.counts.added == 1);
    REQUIRE(r.counts.removed == 1);
    REQUIRE(r.counts.modified == 1);
    REQUIRE(r.counts.metadata_diff == 1);
    REQUIRE(r.counts.ambiguous == 1);

    REQUIRE(CompareService::export_report_json(r, tmp.path() / "cmp.json").ok());
    REQUIRE(CompareService::export_report_text(r, tmp.path() / "cmp.txt").ok());
}
