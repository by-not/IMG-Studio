#pragma once

#include <imgui.h>

#include <string>

namespace imgstudio::ui {

struct ThemePalette {
    ImVec4 bg_root        = ImVec4(0.071f, 0.078f, 0.094f, 1.0f); // #121418
    ImVec4 bg_panel       = ImVec4(0.094f, 0.106f, 0.129f, 1.0f); // #181B21
    ImVec4 bg_surface     = ImVec4(0.125f, 0.141f, 0.173f, 1.0f); // #20242C
    ImVec4 bg_header      = ImVec4(0.145f, 0.165f, 0.204f, 1.0f); // #252A34
    ImVec4 border_subtle  = ImVec4(0.176f, 0.200f, 0.247f, 1.0f); // #2D333F
    ImVec4 text_primary   = ImVec4(0.902f, 0.918f, 0.941f, 1.0f); // #E6EAF0
    ImVec4 text_secondary = ImVec4(0.596f, 0.635f, 0.702f, 1.0f); // #98A2B3
    ImVec4 text_muted     = ImVec4(0.420f, 0.459f, 0.525f, 1.0f); // #6B7586
    ImVec4 accent         = ImVec4(0.231f, 0.510f, 0.965f, 1.0f); // #3B82F6
    ImVec4 accent_hover   = ImVec4(0.322f, 0.580f, 1.000f, 1.0f); // #5294FF
    ImVec4 accent_active  = ImVec4(0.165f, 0.400f, 0.855f, 1.0f); // #2A66DA
    ImVec4 success        = ImVec4(0.133f, 0.773f, 0.369f, 1.0f); // #22C55E
    ImVec4 warning        = ImVec4(0.961f, 0.620f, 0.043f, 1.0f); // #F59E0B
    ImVec4 danger         = ImVec4(0.937f, 0.267f, 0.267f, 1.0f); // #EF4444
    ImVec4 info_cyan      = ImVec4(0.024f, 0.714f, 0.831f, 1.0f); // #06B6D4
};

struct FontBundle {
    ImFont* regular = nullptr;
    ImFont* semibold = nullptr;
    ImFont* mono = nullptr;
    std::string loaded_summary;
};

[[nodiscard]] const ThemePalette& theme_palette() noexcept;
void apply_modern_dark_theme(float ui_scale = 1.0f);
[[nodiscard]] FontBundle load_application_fonts(float dpi_scale, float user_scale);

} // namespace imgstudio::ui
