#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.hpp"
#include "app/BackupManager.hpp"
#include "core/ImgArchive.hpp"

using namespace imgstudio::app;
using namespace imgstudio::core;
using namespace imgstudio::tests;

TEST_CASE("BackupManager creates, lists, verifies, and restores backups", "[app][backup]") {
    TempDirGuard tmp("backup_mgr");
    const auto img_path = tmp.path() / "gta3.img";
    const auto bkp_dir = tmp.path() / "backups";

    const auto orig_bytes = make_pattern_bytes(4096, 15);
    write_synthetic_img(img_path, {{"player.dff", orig_bytes}});
    const auto expected_file_bytes = read_raw_file(img_path);

    BackupManager bm(bkp_dir);
    auto b1 = bm.create_backup(img_path, BackupKind::Manual, "Before mod install");
    REQUIRE(b1.ok());
    REQUIRE(std::filesystem::exists(b1.value->backup_file_path));
    REQUIRE(!b1.value->sha256_hex.empty());

    const auto listed = bm.list_backups(img_path);
    REQUIRE(listed.size() == 1);
    REQUIRE(listed[0].id == b1.value->id);

    // Corrupt original archive on disk
    write_raw_file(img_path, {'C', 'O', 'R', 'R', 'U', 'P', 'T'});
    REQUIRE(ImgArchive::open(img_path).value->validation_report().has_errors());

    // Restore from backup
    REQUIRE(bm.restore_backup(listed[0]).ok());
    REQUIRE(read_raw_file(img_path) == expected_file_bytes);
    REQUIRE(ImgArchive::open(img_path).value->validation_report().is_valid_structure());

    const auto instructions = BackupManager::recovery_instructions(listed[0]);
    REQUIRE(instructions.find(listed[0].sha256_hex) != std::string::npos);
}
