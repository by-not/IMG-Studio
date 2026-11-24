#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace imgstudio::ui {

struct AppLaunchOptions {
    std::optional<std::filesystem::path> initial_archive;
    std::optional<std::filesystem::path> initial_dff;
    std::optional<std::filesystem::path> initial_txd;
    std::optional<std::filesystem::path> settings_file_override;
    std::optional<std::string> initial_language;
    bool save_language_override = false;
    bool smoke_test = false;
    std::optional<std::filesystem::path> smoke_test_report_path;
};

int run_application(const AppLaunchOptions& options);

} // namespace imgstudio::ui
