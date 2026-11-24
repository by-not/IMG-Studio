#pragma once

#include <filesystem>
#include <optional>
#include <string>

namespace imgstudio::ui {

struct AppLaunchOptions {
    std::optional<std::filesystem::path> initial_archive;
    std::optional<std::filesystem::path> initial_dff;
    std::optional<std::filesystem::path> initial_txd;
    bool smoke_test = false;
    std::optional<std::filesystem::path> smoke_test_report_path;
};

int run_application(const AppLaunchOptions& options);

} // namespace imgstudio::ui
