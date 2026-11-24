#pragma once

#include "app/ArchiveController.hpp"
#include "app/BatchProcessor.hpp"
#include "ui/Theme.hpp"
#include "ui/UiState.hpp"

#include <filesystem>
#include <vector>

namespace imgstudio::ui {

class BatchView {
public:
    void render(
        app::ArchiveController& controller,
        UiState& ui_state,
        void* native_hwnd,
        const FontBundle& fonts);

private:
    app::BatchProcessor processor_;
    std::vector<std::filesystem::path> queued_archives_;
    std::vector<std::filesystem::path> input_files_;
    std::filesystem::path export_dir_;
    int op_idx_ = 0;
    int worker_count_ = 2;
    int dup_policy_idx_ = 1; // Replace
    bool auto_backup_ = true;
    bool verify_payloads_ = false;
    bool last_finished_toast_sent_ = false;
};

} // namespace imgstudio::ui
