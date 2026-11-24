#pragma once

#include "app/ArchiveController.hpp"
#include "app/AssetLoader.hpp"
#include "render/DffRenderer.hpp"
#include "ui/Theme.hpp"
#include "ui/UiState.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace imgstudio::ui {

class DffStudioView {
public:
    void render(
        app::ArchiveController& controller,
        UiState& ui_state,
        void* native_hwnd,
        const FontBundle& fonts);

    bool open_file_sync(const std::filesystem::path& path);
    bool open_from_document_sync(const app::ArchiveDocument& doc, std::uint64_t entry_id);
    void render_smoke_frame(int width = 800, int height = 600);
    void shutdown();

    [[nodiscard]] const std::optional<app::LoadedDffSession>& session() const noexcept { return session_; }
    [[nodiscard]] const render::DffRenderer& renderer() const noexcept { return renderer_; }

private:
    void handle_loader_completions(UiState& ui_state);

    app::AssetLoader loader_;
    render::DffRenderer renderer_;
    render::OrbitCamera camera_{};
    render::DffViewSettings view_settings_{};
    std::optional<app::LoadedDffSession> session_;
    std::string last_load_error_;
};

} // namespace imgstudio::ui
