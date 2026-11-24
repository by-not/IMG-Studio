#pragma once

#include "app/ArchiveController.hpp"
#include "ui/Theme.hpp"
#include "ui/UiState.hpp"

namespace imgstudio::ui {

void render_main_view(
    app::ArchiveController& controller,
    UiState& state,
    void* native_hwnd,
    const FontBundle& fonts,
    bool& request_window_close);

} // namespace imgstudio::ui
