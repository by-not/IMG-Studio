#pragma once

#include "app/ArchiveController.hpp"
#include "ui/Theme.hpp"
#include "ui/UiState.hpp"

namespace imgstudio::ui {

void render_inspector_panel(
    app::ArchiveController& controller,
    UiState& state,
    void* native_hwnd,
    const FontBundle& fonts);

} // namespace imgstudio::ui
