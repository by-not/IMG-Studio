#pragma once

#include "app/ArchiveController.hpp"
#include "ui/BatchView.hpp"
#include "ui/DffStudioView.hpp"
#include "ui/ImgCompareView.hpp"
#include "ui/Theme.hpp"
#include "ui/TxdStudioView.hpp"
#include "ui/UiState.hpp"

namespace imgstudio::ui {

struct StudioModules {
    DffStudioView dff_studio;
    TxdStudioView txd_studio;
    ImgCompareView img_compare;
    BatchView batch_view;

    void shutdown() {
        dff_studio.shutdown();
        txd_studio.shutdown();
    }
};

void render_main_view(
    app::ArchiveController& controller,
    UiState& state,
    StudioModules& modules,
    void* native_hwnd,
    const FontBundle& fonts,
    bool& request_window_close);

} // namespace imgstudio::ui
