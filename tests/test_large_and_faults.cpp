#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.hpp"
#include "core/ImgArchive.hpp"
#include "core/ImgBuilder.hpp"

using namespace imgstudio::core;
using namespace imgstudio::tests;

TEST_CASE("Handle large archive with 2500 entries and multi-sector directory", "[large][builder]") {
    TempDirGuard tmp("large_archive");
    const auto orig_path = tmp.path() / "large.img";
    const auto rebuilt_path = tmp.path() / "large_rebuilt.img";

    std::vector<SyntheticEntrySpec> specs;
    specs.reserve(2500);
    for (int i = 0; i < 2500; ++i) {
        specs.push_back(SyntheticEntrySpec{
            "asset_" + std::to_string(i) + ".dff",
            make_pattern_bytes(1024 + static_cast<std::size_t>((i % 5) * 512), static_cast<std::uint8_t>(i & 0xFF)),
            0, 0, false
        });
    }
    write_synthetic_img(orig_path, specs);

    ValidationOptions vopts;
    vopts.verify_payload_readability = true;
    auto arch_res = ImgArchive::open(orig_path, vopts);
    REQUIRE(arch_res.ok());
    REQUIRE(arch_res.value->entries().size() == 2500);
    REQUIRE(arch_res.value->validation_report().is_valid_structure());
    // 8 + 2500 * 32 = 80008 bytes => ceil(80008 / 2048) = 40 sectors
    REQUIRE(arch_res.value->entries()[0].offset_sectors == 40);

    RebuildPlan plan;
    plan.original_archive_path = orig_path;
    for (const auto& e : arch_res.value->entries()) {
        BuildEntrySpec s;
        s.name = e.name;
        s.source_kind = BuildSourceKind::OriginalArchiveRange;
        s.original_offset_bytes = e.offset_bytes();
        s.original_size_bytes = e.effective_bytes();
        s.preserved_streaming_size = e.streaming_size;
        s.preserved_archive_size = e.archive_size;
        s.preserve_size_metadata = true;
        plan.entries.push_back(std::move(s));
    }

    auto rebuilt = ImgBuilder::build_to_file(plan, rebuilt_path);
    REQUIRE(rebuilt.ok());
    REQUIRE(rebuilt.value->entry_count == 2500);
}

TEST_CASE("Builder fails cleanly without touching original on missing external file or bad destination", "[faults]") {
    TempDirGuard tmp("faults");
    const auto orig_path = tmp.path() / "orig.img";
    write_synthetic_img(orig_path, {
        {"one.dff", make_pattern_bytes(2048, 1)}
    });
    const auto orig_bytes = read_raw_file(orig_path);

    RebuildPlan bad_ext;
    bad_ext.original_archive_path = orig_path;
    bad_ext.entries.push_back(BuildEntrySpec{
        "missing.dff", {}, false, BuildSourceKind::ExternalFile, 0, 0, tmp.path() / "does_not_exist.dff", 0, 0, false
    });

    const auto temp_out = tmp.path() / "temp_out.img";
    REQUIRE(!ImgBuilder::build_to_file(bad_ext, temp_out).ok());
    REQUIRE(!std::filesystem::exists(temp_out));
    REQUIRE(read_raw_file(orig_path) == orig_bytes);

    RebuildPlan bad_dest_plan;
    bad_dest_plan.original_archive_path = orig_path;
    bad_dest_plan.entries.push_back(BuildEntrySpec{
        "one.dff", {}, false, BuildSourceKind::OriginalArchiveRange, 2048, 2048, {}, 1, 0, true
    });
    REQUIRE(!ImgBuilder::build_to_file(bad_dest_plan, tmp.path() / "nonexistent_dir" / "out.img").ok());
    REQUIRE(read_raw_file(orig_path) == orig_bytes);
}
