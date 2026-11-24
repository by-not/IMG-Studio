#include "ui/Theme.hpp"
#include "platform/Platform.hpp"

#include <filesystem>

namespace imgstudio::ui {

const ThemePalette& theme_palette() noexcept {
    static const ThemePalette kPalette{};
    return kPalette;
}

void apply_modern_dark_theme(float ui_scale) {
    ImGuiStyle& style = ImGui::GetStyle();
    style = ImGuiStyle();
    const auto& p = theme_palette();

    style.WindowPadding = ImVec2(10.0f, 10.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.CellPadding = ImVec2(8.0f, 5.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.IndentSpacing = 18.0f;
    style.ScrollbarSize = 11.0f;
    style.GrabMinSize = 10.0f;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;

    style.WindowRounding = 6.0f;
    style.ChildRounding = 6.0f;
    style.FrameRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding = 4.0f;
    style.TabRounding = 5.0f;
    style.WindowMenuButtonPosition = ImGuiDir_None;

    ImVec4* c = style.Colors;
    c[ImGuiCol_Text] = p.text_primary;
    c[ImGuiCol_TextDisabled] = p.text_muted;
    c[ImGuiCol_WindowBg] = p.bg_root;
    c[ImGuiCol_ChildBg] = p.bg_panel;
    c[ImGuiCol_PopupBg] = p.bg_surface;
    c[ImGuiCol_Border] = p.border_subtle;
    c[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_FrameBg] = p.bg_surface;
    c[ImGuiCol_FrameBgHovered] = p.bg_header;
    c[ImGuiCol_FrameBgActive] = ImVec4(0.184f, 0.212f, 0.267f, 1.0f);
    c[ImGuiCol_TitleBg] = p.bg_panel;
    c[ImGuiCol_TitleBgActive] = p.bg_header;
    c[ImGuiCol_TitleBgCollapsed] = p.bg_panel;
    c[ImGuiCol_MenuBarBg] = p.bg_panel;
    c[ImGuiCol_ScrollbarBg] = p.bg_root;
    c[ImGuiCol_ScrollbarGrab] = ImVec4(0.24f, 0.27f, 0.33f, 1.0f);
    c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.32f, 0.36f, 0.44f, 1.0f);
    c[ImGuiCol_ScrollbarGrabActive] = p.accent;
    c[ImGuiCol_CheckMark] = p.accent_hover;
    c[ImGuiCol_SliderGrab] = p.accent;
    c[ImGuiCol_SliderGrabActive] = p.accent_hover;
    c[ImGuiCol_Button] = p.bg_surface;
    c[ImGuiCol_ButtonHovered] = p.bg_header;
    c[ImGuiCol_ButtonActive] = ImVec4(0.188f, 0.235f, 0.322f, 1.0f);
    c[ImGuiCol_Header] = ImVec4(0.231f, 0.510f, 0.965f, 0.22f);
    c[ImGuiCol_HeaderHovered] = ImVec4(0.231f, 0.510f, 0.965f, 0.32f);
    c[ImGuiCol_HeaderActive] = ImVec4(0.231f, 0.510f, 0.965f, 0.45f);
    c[ImGuiCol_Separator] = p.border_subtle;
    c[ImGuiCol_SeparatorHovered] = p.accent;
    c[ImGuiCol_SeparatorActive] = p.accent_hover;
    c[ImGuiCol_ResizeGrip] = ImVec4(0.231f, 0.510f, 0.965f, 0.25f);
    c[ImGuiCol_ResizeGripHovered] = ImVec4(0.231f, 0.510f, 0.965f, 0.55f);
    c[ImGuiCol_ResizeGripActive] = p.accent;
    c[ImGuiCol_Tab] = p.bg_panel;
    c[ImGuiCol_TabHovered] = p.bg_header;
    c[ImGuiCol_TabSelected] = p.bg_surface;
    c[ImGuiCol_TabSelectedOverline] = p.accent;
    c[ImGuiCol_TabDimmed] = p.bg_root;
    c[ImGuiCol_TabDimmedSelected] = p.bg_panel;
    c[ImGuiCol_TableHeaderBg] = p.bg_header;
    c[ImGuiCol_TableBorderStrong] = p.border_subtle;
    c[ImGuiCol_TableBorderLight] = ImVec4(0.155f, 0.176f, 0.216f, 1.0f);
    c[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    c[ImGuiCol_TableRowBgAlt] = ImVec4(1.0f, 1.0f, 1.0f, 0.02f);
    c[ImGuiCol_TextSelectedBg] = ImVec4(0.231f, 0.510f, 0.965f, 0.35f);
    c[ImGuiCol_DragDropTarget] = p.accent_hover;
    c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.03f, 0.04f, 0.06f, 0.65f);

    if (ui_scale > 0.5f && ui_scale <= 2.5f) {
        style.ScaleAllSizes(ui_scale);
    }
}

namespace {

bool file_exists(const std::filesystem::path& p) {
    std::error_code ec;
    return !p.empty() && std::filesystem::exists(p, ec) && std::filesystem::is_regular_file(p, ec);
}

void merge_fontawesome(ImGuiIO& io, float icon_size, const std::filesystem::path& fa_path) {
    if (!file_exists(fa_path)) {
        return;
    }
    static const ImWchar kIconRanges[] = {0xE005, 0xF8FF, 0};
    ImFontConfig icon_cfg;
    icon_cfg.MergeMode = true;
    icon_cfg.PixelSnapH = true;
    icon_cfg.GlyphMinAdvanceX = icon_size * 1.15f;
    icon_cfg.GlyphOffset = ImVec2(0.0f, 0.5f);
    io.Fonts->AddFontFromFileTTF(
        platform::to_utf8(fa_path).c_str(),
        icon_size,
        &icon_cfg,
        kIconRanges);
}

} // namespace

FontBundle load_application_fonts(float dpi_scale, float user_scale) {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();

    const float scale = (dpi_scale > 0.5f ? dpi_scale : 1.0f) * (user_scale > 0.5f ? user_scale : 1.0f);
    const float base_px = 15.0f * scale;
    const float bold_px = 16.0f * scale;
    const float mono_px = 13.5f * scale;
    const float icon_px = 13.5f * scale;

    auto regular_path = platform::resolve_asset_path("fonts/Inter-Regular.ttf");
    auto semibold_path = platform::resolve_asset_path("fonts/Inter-SemiBold.ttf");
    auto mono_path = platform::resolve_asset_path("fonts/JetBrainsMono-Regular.ttf");
    const auto fa_path = platform::resolve_asset_path("fonts/fa-solid-900.ttf");

#ifdef _WIN32
    if (!file_exists(regular_path)) {
        regular_path = "C:\\Windows\\Fonts\\segoeui.ttf";
    }
    if (!file_exists(semibold_path)) {
        semibold_path = "C:\\Windows\\Fonts\\seguisb.ttf";
    }
    if (!file_exists(mono_path)) {
        mono_path = "C:\\Windows\\Fonts\\consola.ttf";
    }
#endif

    FontBundle bundle;
    if (file_exists(regular_path)) {
        bundle.regular = io.Fonts->AddFontFromFileTTF(platform::to_utf8(regular_path).c_str(), base_px);
        bundle.loaded_summary += "UI=" + regular_path.filename().string();
    } else {
        bundle.regular = io.Fonts->AddFontDefault();
        bundle.loaded_summary += "UI=Default";
    }
    merge_fontawesome(io, icon_px, fa_path);

    if (file_exists(semibold_path)) {
        bundle.semibold = io.Fonts->AddFontFromFileTTF(platform::to_utf8(semibold_path).c_str(), bold_px);
        merge_fontawesome(io, icon_px, fa_path);
        bundle.loaded_summary += ";SemiBold=" + semibold_path.filename().string();
    } else {
        bundle.semibold = bundle.regular;
    }

    if (file_exists(mono_path)) {
        bundle.mono = io.Fonts->AddFontFromFileTTF(platform::to_utf8(mono_path).c_str(), mono_px);
        bundle.loaded_summary += ";Mono=" + mono_path.filename().string();
    } else {
        bundle.mono = bundle.regular;
    }

    if (file_exists(fa_path)) {
        bundle.loaded_summary += ";Icons=FontAwesome6";
    }

    io.FontDefault = bundle.regular;
    io.Fonts->Build();
    return bundle;
}

} // namespace imgstudio::ui
