#pragma once

#include "app/ArchiveController.hpp"
#include "app/EntryFilter.hpp"
#include "ui/Theme.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

namespace imgstudio::ui {

enum class WorkspaceTab : std::uint8_t {
    ArchiveExplorer = 0,
    DffStudio,
    TxdStudio,
    ImgCompare,
    BatchProcessing
};

struct UiState {
    WorkspaceTab active_workspace = WorkspaceTab::ArchiveExplorer;
    std::optional<std::uint64_t> pending_preview_dff_entry_id;
    std::optional<std::uint64_t> pending_preview_txd_entry_id;
    std::optional<std::filesystem::path> pending_compare_archive_a;

    // Filtering & sorting
    app::EntryFilterCriteria filter{};
    std::vector<std::size_t> visible_indices;
    bool filter_dirty = true;
    std::uint64_t last_seen_tab_ptr = 0;

    // Multi-selection (stores DocumentEntry::id)
    std::unordered_set<std::uint64_t> selected_ids;
    std::optional<std::uint64_t> primary_selected_id;
    std::optional<std::size_t> anchor_visible_row;

    // Inspector tab: 0=Properties, 1=Pending, 2=Validation, 3=Backups
    int inspector_tab = 0;
    bool show_val_errors = true;
    bool show_val_warnings = true;
    bool show_val_info = true;

    // Modal state
    bool open_rename_modal = false;
    std::uint64_t rename_target_id = 0;
    std::string rename_buffer;
    std::string rename_error;

    bool open_delete_modal = false;
    std::vector<std::uint64_t> delete_target_ids;

    bool open_commit_modal = false;
    bool commit_auto_backup_override = true;

    bool open_duplicate_modal = false;
    std::vector<std::filesystem::path> pending_duplicate_files;

    bool open_settings_modal = false;
    app::AppSettings settings_draft{};

    bool open_about_modal = false;
    bool open_close_confirm_modal = false;
    std::optional<std::size_t> close_tab_pending;
    bool close_app_after_confirm = false;

    // Toast notification
    struct Toast {
        std::string message;
        std::string detail;
        bool is_error = false;
        double expires_at = 0.0;
    };
    std::vector<Toast> toasts;

    // Drag & drop overlay
    bool drag_hovering = false;
    double drag_hover_until = 0.0;
    bool focus_search_requested = false;

    void push_toast(std::string msg, std::string det = {}, bool is_err = false, double duration = 5.0);
};

} // namespace imgstudio::ui
