#pragma once

#include "app/ArchiveController.hpp"
#include "app/AssetLoader.hpp"
#include "core/rw/TxdTypes.hpp"
#include "render/GpuTexture.hpp"
#include "ui/Theme.hpp"
#include "ui/UiState.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace imgstudio::ui {

class TxdStudioView {
public:
    void render(
        app::ArchiveController& controller,
        UiState& ui_state,
        void* native_hwnd,
        const FontBundle& fonts);

    bool open_file_sync(const std::filesystem::path& path);
    bool open_from_document_sync(const app::ArchiveDocument& doc, std::uint64_t entry_id);
    void shutdown();

    [[nodiscard]] const std::shared_ptr<const core::rw::TxdDictionary>& dictionary() const noexcept {
        return dict_;
    }

private:
    enum class ChannelView : int { Rgba = 0, Rgb, Alpha };
    enum class BgStyle : int { Checkerboard = 0, Dark, Light };
    enum class DimFilter : int { All = 0, Le64, Size128, Size256, Ge512 };
    enum class SortMode : int { Original = 0, NameAsc, SizeDesc };

    void set_dictionary(std::shared_ptr<const core::rw::TxdDictionary> dict);
    void rebuild_preview_gpu_texture();
    void handle_loader_completions(UiState& ui_state);
    void export_textures(bool export_all, void* native_hwnd, UiState& ui_state);

    app::AssetLoader loader_;
    std::shared_ptr<const core::rw::TxdDictionary> dict_;
    std::vector<render::GpuTexture> thumb_textures_;
    render::GpuTexture preview_gpu_tex_;

    std::unordered_set<std::size_t> selected_indices_;
    std::size_t primary_index_ = 0U;
    std::size_t selected_mip_ = 0U;

    std::string search_text_;
    DimFilter dim_filter_ = DimFilter::All;
    SortMode sort_mode_ = SortMode::Original;
    ChannelView channel_view_ = ChannelView::Rgba;
    BgStyle bg_style_ = BgStyle::Checkerboard;
    render::TextureFilterMode filter_mode_ = render::TextureFilterMode::Linear;
    bool fit_viewport_ = true;
    float zoom_factor_ = 1.0f;
    int export_fmt_idx_ = 0; // 0=PNG, 1=TGA, 2=BMP
    std::string last_error_;
};

} // namespace imgstudio::ui
