#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace imgstudio::platform {

struct FileFilterSpec {
    std::string label;
    std::string pattern;
};

[[nodiscard]] std::optional<std::filesystem::path> open_file_dialog(
    void* native_window_handle,
    const std::string& title,
    const std::vector<FileFilterSpec>& filters);

[[nodiscard]] std::vector<std::filesystem::path> open_files_dialog(
    void* native_window_handle,
    const std::string& title,
    const std::vector<FileFilterSpec>& filters);

[[nodiscard]] std::optional<std::filesystem::path> save_file_dialog(
    void* native_window_handle,
    const std::string& title,
    const std::string& default_name,
    const std::vector<FileFilterSpec>& filters);

[[nodiscard]] std::optional<std::filesystem::path> pick_folder_dialog(
    void* native_window_handle,
    const std::string& title);

} // namespace imgstudio::platform
