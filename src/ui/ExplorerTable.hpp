#pragma once

#include "app/ArchiveController.hpp"
#include "ui/UiState.hpp"

#include <vector>

namespace imgstudio::ui {

void render_explorer_table(
    app::ArchiveController& controller,
    UiState& state,
    void* native_hwnd,
    const FontBundle& fonts);

void trigger_import_files(
    app::ArchiveController& controller,
    UiState& state,
    const std::vector<std::filesystem::path>& files);

void trigger_export_entries(
    app::ArchiveController& controller,
    UiState& state,
    void* native_hwnd,
    bool export_all);

void trigger_replace_selected(
    app::ArchiveController& controller,
    UiState& state,
    void* native_hwnd);

void trigger_rename_selected(
    app::ArchiveController& controller,
    UiState& state);

void trigger_delete_selected(
    app::ArchiveController& controller,
    UiState& state);

} // namespace imgstudio::ui
