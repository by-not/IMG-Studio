#pragma once

#include "core/ImgTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace imgstudio::platform {

struct SystemPaths {
    std::filesystem::path executable_dir;
    std::filesystem::path app_data_dir;
    std::filesystem::path default_backup_dir;
    std::filesystem::path settings_file;
    std::filesystem::path assets_dir;
};

[[nodiscard]] SystemPaths get_system_paths();
[[nodiscard]] std::string to_utf8(const std::filesystem::path& p);
[[nodiscard]] std::filesystem::path from_utf8(const std::string& utf8);
[[nodiscard]] std::filesystem::path resolve_asset_path(const std::string& relative_path);
[[nodiscard]] bool reveal_in_file_manager(const std::filesystem::path& path);
[[nodiscard]] bool open_directory_in_file_manager(const std::filesystem::path& dir);
[[nodiscard]] std::string detect_system_language();

} // namespace imgstudio::platform
