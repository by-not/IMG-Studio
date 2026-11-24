#pragma once

#include "app/ArchiveController.hpp"
#include "ui/Theme.hpp"
#include "ui/UiState.hpp"

namespace imgstudio::ui {

void render_modals_and_toasts(
    app::ArchiveController& controller,
    UiState& state,
    void* native_hwnd,
    bool& request_window_close);

} // namespace imgstudio::ui
