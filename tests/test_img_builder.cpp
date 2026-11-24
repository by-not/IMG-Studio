#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.hpp"
#include "core/BinaryIO.hpp"
#include "core/ImgArchive.hpp"
#include "core/ImgBuilder.hpp"

using namespace imgstudio::core;
using namespace imgstudio::tests;

TEST_CASE("Round-trip rebuild of unchanged archive preserves exact payload bytes and metadata", "[builder]") {
    TempDirGuard tmp("builder_roundtrip");
    const auto orig_path = tmp.path() / "orig.img";
    const auto rebuilt_path = tmp.path() / "rebuilt.img";

    const auto p1 = make_pattern_bytes(4096, 5);
    const auto p2 = make_pattern_bytes(6144, 9);
    write_synthetic_img(orig_path, {
        {"sultan.dff", p1, 2, 0, true},
        {"sultan.txd", p2, 2, 3, true} // archive_size=3 differs from streaming_size=2
    });

    auto arch_res = ImgArchive::open(orig_path);
    REQUIRE(arch_res.ok());

    RebuildPlan plan;
    plan.original_archive_path = orig_path;
    for (const auto& e : arch_res.value->entries()) {
        BuildEntrySpec spec;
        spec.name = e.name;
        spec.raw_name = e.raw_name;
        spec.preserve_raw_name = true;
        spec.source_kind = BuildSourceKind::OriginalArchiveRange;
        spec.original_offset_bytes = e.offset_bytes();
        spec.original_size_bytes = e.effective_bytes();
        spec.preserved_streaming_size = e.streaming_size;
        spec.preserved_archive_size = e.archive_size;
        spec.preserve_size_metadata = true;
        plan.entries.push_back(std::move(spec));
    }

    auto build_res = ImgBuilder::build_to_file(plan, rebuilt_path);
    REQUIRE(build_res.ok());
    REQUIRE(build_res.value->verified);
    REQUIRE(read_raw_file(orig_path) == read_raw_file(rebuilt_path));
}

TEST_CASE("Rebuild after add, replace, rename, and delete produces valid sector-aligned archive", "[builder]") {
    TempDirGuard tmp("builder_mods");
    const auto orig_path = tmp.path() / "orig.img";
    const auto rebuilt_path = tmp.path() / "rebuilt.img";

    const auto p_keep = make_pattern_bytes(2048, 10);
    const auto p_del = make_pattern_bytes(4096, 20);
    const auto p_rep_new = make_pattern_bytes(5500, 30);
    const auto p_added = make_pattern_bytes(100, 40);

    write_synthetic_img(orig_path, {
        {"keep.dff", p_keep},
        {"delete_me.txd", p_del},
        {"replace_me.col", make_pattern_bytes(2048, 99)}
    });

    const auto rep_file = write_raw_file(tmp.path() / "new_col.col", p_rep_new);
    const auto add_file = write_raw_file(tmp.path() / "added.ipl", p_added);

    auto arch_res = ImgArchive::open(orig_path);
    REQUIRE(arch_res.ok());
    const auto& orig_entries = arch_res.value->entries();

    RebuildPlan plan;
    plan.original_archive_path = orig_path;

    // Rename keep.dff -> kept.dff
    {
        BuildEntrySpec s;
        s.name = "kept.dff";
        s.source_kind = BuildSourceKind::OriginalArchiveRange;
        s.original_offset_bytes = orig_entries[0].offset_bytes();
        s.original_size_bytes = orig_entries[0].effective_bytes();
        s.preserved_streaming_size = orig_entries[0].streaming_size;
        s.preserved_archive_size = orig_entries[0].archive_size;
        s.preserve_size_metadata = true;
        plan.entries.push_back(std::move(s));
    }
    // delete_me.txd omitted
    // replace_me.col replaced with larger external file
    {
        BuildEntrySpec s;
        s.name = "replace_me.col";
        s.source_kind = BuildSourceKind::ExternalFile;
        s.external_path = rep_file;
        plan.entries.push_back(std::move(s));
    }
    // added.ipl imported
    {
        BuildEntrySpec s;
        s.name = "added.ipl";
        s.source_kind = BuildSourceKind::ExternalFile;
        s.external_path = add_file;
        plan.entries.push_back(std::move(s));
    }

    auto res = ImgBuilder::build_to_file(plan, rebuilt_path);
    REQUIRE(res.ok());
    REQUIRE(res.value->entry_count == 3);

    ValidationOptions vopts;
    vopts.verify_payload_readability = true;
    auto reopened = ImgArchive::open(rebuilt_path, vopts);
    REQUIRE(reopened.ok());
    REQUIRE(reopened.value->validation_report().is_valid_structure());
    REQUIRE(!reopened.value->validation_report().metrics.is_fragmented);
    REQUIRE(reopened.value->entries()[0].name == "kept.dff");
    REQUIRE(reopened.value->entries()[0].offset_sectors == 1);
    REQUIRE(reopened.value->entries()[1].name == "replace_me.col");
    REQUIRE(reopened.value->entries()[1].offset_sectors == 2);
    REQUIRE(reopened.value->entries()[1].streaming_size == 3); // ceil(5500/2048)
    REQUIRE(reopened.value->entries()[2].name == "added.ipl");
    REQUIRE(reopened.value->entries()[2].offset_sectors == 5);
    REQUIRE(reopened.value->entries()[2].streaming_size == 1);
}

TEST_CASE("Rebuild empty archive produces 2048-byte valid VER2 file", "[builder]") {
    TempDirGuard tmp("builder_empty");
    const auto rebuilt_path = tmp.path() / "empty.img";
    RebuildPlan plan;
    auto res = ImgBuilder::build_to_file(plan, rebuilt_path);
    REQUIRE(res.ok());
    REQUIRE(res.value->total_output_bytes == 2048);

    auto reopened = ImgArchive::open(rebuilt_path);
    REQUIRE(reopened.ok());
    REQUIRE(reopened.value->entries().empty());
    REQUIRE(reopened.value->validation_report().is_valid_structure());
}
