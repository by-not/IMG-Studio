#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.hpp"
#include "app/BatchProcessor.hpp"
#include "core/ImgArchive.hpp"

using namespace imgstudio::app;
using namespace imgstudio::core;
using namespace imgstudio::tests;

TEST_CASE("BatchProcessor validates, backs up, exports, imports, and isolates failures", "[batch]") {
    TempDirGuard tmp("batch");
    const auto img1 = tmp.path() / "one.img";
    const auto img2 = tmp.path() / "two.img";
    const auto bad_img = tmp.path() / "broken.img";
    const auto bkp_dir = tmp.path() / "backups";
    const auto export_dir = tmp.path() / "exports";

    write_synthetic_img(img1, {{"a.dff", make_pattern_bytes(2048, 1)}});
    write_synthetic_img(img2, {{"b.txd", make_pattern_bytes(2048, 2)}});
    write_raw_file(bad_img, {'B', 'A', 'D', '!', 0, 0, 0, 0});

    // 1. Validate batch with 2 valid + 1 broken archive (failure isolated)
    BatchJobSpec val_spec;
    val_spec.operation = BatchOperationKind::Validate;
    val_spec.archive_paths = {img1, bad_img, img2};
    val_spec.worker_threads = 2;
    auto val_res = BatchProcessor::run_sync(val_spec);
    REQUIRE(val_res.total == 3);
    REQUIRE(val_res.succeeded == 2);
    REQUIRE(val_res.failed == 1);

    // 2. Batch ExportAll
    BatchJobSpec exp_spec;
    exp_spec.operation = BatchOperationKind::ExportAll;
    exp_spec.archive_paths = {img1, img2};
    exp_spec.output_directory = export_dir;
    auto exp_res = BatchProcessor::run_sync(exp_spec);
    REQUIRE(exp_res.succeeded == 2);
    REQUIRE(std::filesystem::exists(export_dir / "one" / "a.dff"));
    REQUIRE(std::filesystem::exists(export_dir / "two" / "b.txd"));

    // 3. Batch ImportFiles with automatic backup
    const auto extra = write_raw_file(tmp.path() / "shared.col", make_pattern_bytes(2048, 44));
    BatchJobSpec imp_spec;
    imp_spec.operation = BatchOperationKind::ImportFiles;
    imp_spec.archive_paths = {img1, img2};
    imp_spec.input_files = {extra};
    imp_spec.backup_directory = bkp_dir;
    imp_spec.create_auto_backup = true;
    imp_spec.worker_threads = 2;
    auto imp_res = BatchProcessor::run_sync(imp_spec);
    REQUIRE(imp_res.succeeded == 2);
    REQUIRE(!imp_res.items[0].backup_id.empty());

    auto reopened1 = ImgArchive::open(img1);
    REQUIRE(reopened1.ok());
    REQUIRE(reopened1.value->entries().size() == 2);
    REQUIRE(BatchProcessor::export_summary_json(imp_res, tmp.path() / "batch.json").ok());

    // 4. Batch ReplaceMatching + Backup + Recovery
    const auto new_a_bytes = make_pattern_bytes(4096, 88);
    const auto rep_a = write_raw_file(tmp.path() / "a.dff", new_a_bytes);
    BatchJobSpec rep_spec;
    rep_spec.operation = BatchOperationKind::ReplaceMatching;
    rep_spec.archive_paths = {img1, img2};
    rep_spec.input_files = {rep_a};
    rep_spec.backup_directory = bkp_dir;
    rep_spec.create_auto_backup = true;
    auto rep_res = BatchProcessor::run_sync(rep_spec);
    REQUIRE(rep_res.succeeded == 2);
    REQUIRE(!rep_res.items[0].backup_id.empty());

    // Manual batch backup + cancellation
    BatchJobSpec bkp_spec;
    bkp_spec.operation = BatchOperationKind::Backup;
    bkp_spec.archive_paths = {img1, img2};
    bkp_spec.backup_directory = bkp_dir;
    auto bkp_res = BatchProcessor::run_sync(bkp_spec);
    REQUIRE(bkp_res.succeeded == 2);

    std::atomic<bool> cancel_flag{true};
    BatchJobSpec cancel_spec;
    cancel_spec.operation = BatchOperationKind::Validate;
    cancel_spec.archive_paths = {img1, img2};
    auto cancel_res = BatchProcessor::run_sync(cancel_spec, &cancel_flag);
    REQUIRE(cancel_res.canceled);
    REQUIRE(cancel_res.canceled_count == 2);
}
