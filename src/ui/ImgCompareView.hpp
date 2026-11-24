#pragma once

#include "app/ArchiveController.hpp"
#include "app/CompareService.hpp"
#include "ui/Theme.hpp"
#include "ui/UiState.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace imgstudio::ui {

class ImgCompareView {
public:
    void render(
        app::ArchiveController& controller,
        UiState& ui_state,
        void* native_hwnd,
        const FontBundle& fonts);

private:
    app::CompareService service_;
    std::filesystem::path archive_a_;
    std::filesystem::path archive_b_;
    std::optional<core::ArchiveComparisonResult> result_;
    std::optional<std::size_t> selected_row_;
    std::string search_text_;
    int diff_filter_ = 0; // 0=All, 1=Differences Only, 2=Added, 3=Removed, 4=Modified, 5=Metadata, 6=Ambiguous
};

} // namespace imgstudio::ui
