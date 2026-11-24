#pragma once

#include "core/ImgTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace imgstudio::app {

enum class DuplicateImportPolicy : std::uint8_t {
    Ask = 0,
    Replace,
    Skip,
    AutoRename
};

struct AppSettings {
    std::filesystem::path backup_directory;
    bool auto_backup_before_commit = true;
    DuplicateImportPolicy duplicate_policy = DuplicateImportPolicy::Ask;
    bool show_deleted_entries = true;
    bool verify_payloads_on_open = false;
    float ui_scale = 1.0f;
    float sidebar_width = 270.0f;
    float inspector_width = 340.0f;
    bool show_sidebar = true;
    bool show_inspector = true;
    std::vector<std::filesystem::path> recent_files;
    std::size_t max_recent_files = 15U;
    std::string language = "auto";

    void add_recent_file(const std::filesystem::path& path);
};

class AppSettingsStore {
public:
    [[nodiscard]] static AppSettings load_or_default(const std::filesystem::path& settings_file);
    [[nodiscard]] static core::Result<void> save(
        const AppSettings& settings,
        const std::filesystem::path& settings_file);
};

[[nodiscard]] const char* duplicate_policy_label(DuplicateImportPolicy policy) noexcept;

} // namespace imgstudio::app
