#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.hpp"
#include "app/ArchiveDocument.hpp"

using namespace imgstudio::app;
using namespace imgstudio::core;
using namespace imgstudio::tests;

TEST_CASE("ArchiveDocument import duplicate policies and rejection handling", "[app][document]") {
    TempDirGuard tmp("doc_import");
    const auto img_path = tmp.path() / "base.img";
    write_synthetic_img(img_path, {
        {"sultan.dff", make_pattern_bytes(2048, 1)},
        {"sultan.txd", make_pattern_bytes(2048, 2)}
    });

    const auto f_dup = write_raw_file(tmp.path() / "SULTAN.DFF", make_pattern_bytes(4096, 9));
    const auto f_new = write_raw_file(tmp.path() / "elegy.dff", make_pattern_bytes(3000, 10));
    const auto f_bad = write_raw_file(tmp.path() / "way_too_long_filename_for_ver2.dff", make_pattern_bytes(512, 3));

    auto doc_res = ArchiveDocument::open(img_path);
    REQUIRE(doc_res.ok());
    auto& doc = *doc_res.value;

    // Ask policy reports unresolved duplicate without discarding
    auto ask_rep = doc.stage_import({f_dup, f_new, f_bad}, DuplicateImportPolicy::Ask);
    REQUIRE(ask_rep.ok());
    REQUIRE(ask_rep.value->imported_count == 1);
    REQUIRE(ask_rep.value->unresolved_duplicates.size() == 1);
    REQUIRE(ask_rep.value->rejected.size() == 1);
    REQUIRE(doc.active_entry_count() == 3);

    // Skip policy
    auto skip_rep = doc.stage_import({f_dup}, DuplicateImportPolicy::Skip);
    REQUIRE(skip_rep.ok());
    REQUIRE(skip_rep.value->skipped_count == 1);

    // Replace policy
    auto rep_rep = doc.stage_import({f_dup}, DuplicateImportPolicy::Replace);
    REQUIRE(rep_rep.ok());
    REQUIRE(rep_rep.value->replaced_count == 1);
    REQUIRE(doc.entries()[0].change_state == EntryChangeState::Replaced);
    REQUIRE(doc.entries()[0].streaming_size == 2);

    // AutoRename policy
    auto ren_rep = doc.stage_import({f_dup}, DuplicateImportPolicy::AutoRename);
    REQUIRE(ren_rep.ok());
    REQUIRE(ren_rep.value->renamed_count == 1);
    REQUIRE(doc.active_entry_count() == 4);
}

TEST_CASE("ArchiveDocument undo/redo, rename, delete, commit, and backup", "[app][document]") {
    TempDirGuard tmp("doc_ops");
    const auto img_path = tmp.path() / "work.img";
    const auto bkp_dir = tmp.path() / "backups";

    const auto orig_b1 = make_pattern_bytes(2048, 4);
    const auto orig_b2 = make_pattern_bytes(2048, 8);
    write_synthetic_img(img_path, {
        {"alpha.dff", orig_b1},
        {"beta.txd", orig_b2}
    });

    auto doc_res = ArchiveDocument::open(img_path);
    REQUIRE(doc_res.ok());
    auto& doc = *doc_res.value;

    // Rename alpha.dff -> alpha2.dff
    REQUIRE(doc.stage_rename(doc.entries()[0].id, "alpha2.dff").ok());
    REQUIRE(doc.entries()[0].name == "alpha2.dff");
    REQUIRE(doc.has_pending_changes());

    // Duplicate rename must fail
    REQUIRE(!doc.stage_rename(doc.entries()[0].id, "BETA.TXD").ok());

    // Delete beta.txd
    REQUIRE(doc.stage_delete({doc.entries()[1].id}).ok());
    REQUIRE(doc.active_entry_count() == 1);

    // Undo delete -> beta.txd restored
    REQUIRE(doc.undo());
    REQUIRE(doc.active_entry_count() == 2);
    REQUIRE(doc.entries()[1].change_state == EntryChangeState::Unchanged);

    // Redo delete -> beta.txd deleted again
    REQUIRE(doc.redo());
    REQUIRE(doc.active_entry_count() == 1);

    // Import new entry
    const auto f_add = write_raw_file(tmp.path() / "gamma.col", make_pattern_bytes(4096, 77));
    REQUIRE(doc.stage_import({f_add}, DuplicateImportPolicy::Replace).ok());
    REQUIRE(doc.active_entry_count() == 2);

    // Commit rebuild with auto backup
    BackupManager bm(bkp_dir);
    auto commit = doc.commit_rebuild(&bm, true);
    REQUIRE(commit.ok());
    REQUIRE(commit.value->auto_backup.has_value());
    REQUIRE(commit.value->build_result.verified);
    REQUIRE(!doc.has_pending_changes());
    REQUIRE(!doc.history().can_undo());
    REQUIRE(doc.entries().size() == 2);
    REQUIRE(doc.entries()[0].name == "alpha2.dff");
    REQUIRE(doc.entries()[1].name == "gamma.col");

    // Verify reopened bytes for alpha2.dff
    const auto out_alpha = tmp.path() / "out_alpha2.dff";
    ImgEntry alpha_entry;
    alpha_entry.index = 0;
    alpha_entry.offset_sectors = doc.entries()[0].offset_sectors;
    alpha_entry.streaming_size = doc.entries()[0].streaming_size;
    alpha_entry.archive_size = doc.entries()[0].archive_size;
    alpha_entry.raw_name = doc.entries()[0].raw_name;
    alpha_entry.name = doc.entries()[0].name;
    alpha_entry.extension = doc.entries()[0].extension;
    alpha_entry.category = doc.entries()[0].category;
    REQUIRE(ImgExtractor::extract_entry_to_file(img_path, alpha_entry, out_alpha).ok());
    REQUIRE(read_raw_file(out_alpha) == orig_b1);
}
