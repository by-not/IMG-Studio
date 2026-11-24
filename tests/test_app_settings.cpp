#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.hpp"
#include "app/AppSettings.hpp"

using namespace imgstudio::app;
using namespace imgstudio::tests;

TEST_CASE("AppSettings save and load round-trip and corrupt fallback", "[app][settings]") {
    TempDirGuard tmp("settings");
    const auto cfg_path = tmp.path() / "settings.json";

    AppSettings s;
    s.backup_directory = tmp.path() / "my_backups";
    s.auto_backup_before_commit = false;
    s.duplicate_policy = DuplicateImportPolicy::AutoRename;
    s.ui_scale = 1.25f;
    s.add_recent_file(tmp.path() / "a.img");
    s.add_recent_file(tmp.path() / "b.img");
    s.add_recent_file(tmp.path() / "a.img"); // moves to front without duplicating

    REQUIRE(AppSettingsStore::save(s, cfg_path).ok());

    const auto loaded = AppSettingsStore::load_or_default(cfg_path);
    REQUIRE(loaded.backup_directory == s.backup_directory);
    REQUIRE(loaded.auto_backup_before_commit == false);
    REQUIRE(loaded.duplicate_policy == DuplicateImportPolicy::AutoRename);
    REQUIRE(loaded.ui_scale == 1.25f);
    REQUIRE(loaded.recent_files.size() == 2);
    REQUIRE(loaded.recent_files[0].filename() == "a.img");

    // Corrupt JSON falls back cleanly
    write_raw_file(cfg_path, {'{', 'b', 'a', 'd'});
    const auto fallback = AppSettingsStore::load_or_default(cfg_path);
    REQUIRE(fallback.auto_backup_before_commit == true);
    REQUIRE(fallback.duplicate_policy == DuplicateImportPolicy::Ask);
}
