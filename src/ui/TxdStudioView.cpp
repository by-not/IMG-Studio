#include "ui/TxdStudioView.hpp"
#include "core/ImgFilename.hpp"
#include "core/rw/ImageExporter.hpp"
#include "core/rw/RwStream.hpp"
#include "platform/FileDialogs.hpp"
#include "platform/Platform.hpp"
#include "ui/Icons.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <vector>

namespace imgstudio::ui {

void TxdStudioView::shutdown() {
    thumb_textures_.clear();
    preview_gpu_tex_.release();
    dict_.reset();
}

void TxdStudioView::set_dictionary(std::shared_ptr<const core::rw::TxdDictionary> dict) {
    dict_ = std::move(dict);
    thumb_textures_.clear();
    selected_indices_.clear();
    primary_index_ = 0U;
    selected_mip_ = 0U;

    if (dict_) {
        thumb_textures_.resize(dict_->textures.size());
        for (std::size_t i = 0; i < dict_->textures.size(); ++i) {
            const auto& tex = dict_->textures[i];
            if (tex.supported && !tex.mipmaps.empty()) {
                thumb_textures_[i].upload_mip(tex.mipmaps[0], render::TextureFilterMode::Linear, false);
            }
        }
        if (!dict_->textures.empty()) {
            selected_indices_.insert(0U);
        }
    }
    rebuild_preview_gpu_texture();
}

void TxdStudioView::rebuild_preview_gpu_texture() {
    preview_gpu_tex_.release();
    if (!dict_ || primary_index_ >= dict_->textures.size()) {
        return;
    }
    const auto& tex = dict_->textures[primary_index_];
    if (!tex.supported || tex.mipmaps.empty()) {
        return;
    }
    if (selected_mip_ >= tex.mipmaps.size()) {
        selected_mip_ = 0U;
    }
    const auto& mip = tex.mipmaps[selected_mip_];
    if (channel_view_ == ChannelView::Rgba) {
        preview_gpu_tex_.upload_mip(mip, filter_mode_, false);
        return;
    }

    std::vector<std::uint8_t> swizzled = mip.rgba8;
    for (std::size_t i = 0; i + 3U < swizzled.size(); i += 4U) {
        if (channel_view_ == ChannelView::Rgb) {
            swizzled[i + 3U] = 255U;
        } else {
            const std::uint8_t a = swizzled[i + 3U];
            swizzled[i + 0U] = a;
            swizzled[i + 1U] = a;
            swizzled[i + 2U] = a;
            swizzled[i + 3U] = 255U;
        }
    }
    preview_gpu_tex_.upload_rgba8(mip.width, mip.height, swizzled.data(), filter_mode_, false);
}

bool TxdStudioView::open_file_sync(const std::filesystem::path& path) {
    auto res = app::AssetLoader::load_txd_from_file_sync(path);
    if (!res.ok()) {
        last_error_ = res.error->message;
        return false;
    }
    last_error_.clear();
    set_dictionary(std::move(*res.value));
    return true;
}

bool TxdStudioView::open_from_document_sync(const app::ArchiveDocument& doc, std::uint64_t entry_id) {
    auto res = app::AssetLoader::load_txd_from_document_sync(doc, entry_id);
    if (!res.ok()) {
        last_error_ = res.error->message;
        return false;
    }
    last_error_.clear();
    set_dictionary(std::move(*res.value));
    return true;
}

void TxdStudioView::handle_loader_completions(UiState& ui_state) {
    for (auto& comp : loader_.poll_completions()) {
        if (!comp.ok) {
            last_error_ = comp.message;
            ui_state.push_toast(comp.message, comp.detail, true);
            continue;
        }
        if (comp.kind == app::AssetLoadCompletion::Kind::TxdLoaded && comp.txd_dict) {
            last_error_.clear();
            set_dictionary(comp.txd_dict);
            ui_state.push_toast("TXD Dictionary Loaded", comp.txd_dict->source_name);
        }
    }
}

void TxdStudioView::export_textures(bool export_all, void* native_hwnd, UiState& ui_state) {
    if (!dict_) return;
    auto folder = platform::pick_folder_dialog(
        native_hwnd,
        export_all ? "Export All TXD Textures To Folder" : "Export Selected TXD Textures To Folder");
    if (!folder.has_value()) {
        return;
    }

    const auto fmt = static_cast<core::rw::ImageExportFormat>(export_fmt_idx_);
    const char* ext = core::rw::image_export_extension(fmt);
    std::size_t exported = 0;
    std::size_t failed = 0;
    std::string first_err;

    for (std::size_t i = 0; i < dict_->textures.size(); ++i) {
        if (!export_all && selected_indices_.count(i) == 0U) {
            continue;
        }
        const auto& tex = dict_->textures[i];
        if (!tex.supported || tex.mipmaps.empty()) {
            ++failed;
            first_err = tex.name + ": " + tex.unsupported_reason;
            continue;
        }
        const std::string base = tex.name.empty() ? ("texture_" + std::to_string(i)) : tex.name;
        auto safe = core::build_safe_export_path(*folder, base + ext, true);
        if (!safe.ok()) {
            ++failed;
            first_err = safe.error->message;
            continue;
        }
        const std::size_t mip_idx = (!export_all && i == primary_index_ && selected_mip_ < tex.mipmaps.size())
            ? selected_mip_ : 0U;
        auto ex = core::rw::ImageExporter::export_mip_to_file(tex.mipmaps[mip_idx], *safe.value, fmt);
        if (ex.ok()) {
            ++exported;
        } else {
            ++failed;
            first_err = ex.error->message;
        }
    }

    if (failed > 0U) {
        ui_state.push_toast(
            "Exported " + std::to_string(exported) + ", failed " + std::to_string(failed),
            first_err,
            true);
    } else {
        ui_state.push_toast("Textures Exported", "Saved " + std::to_string(exported) + " file(s) as " + ext);
    }
}

void TxdStudioView::render(
    app::ArchiveController& controller,
    UiState& ui_state,
    void* native_hwnd,
    const FontBundle& fonts) {
    if (ui_state.pending_preview_txd_entry_id.has_value()) {
        if (const auto* doc = controller.active_document()) {
            loader_.start_load_txd_from_document(*doc, *ui_state.pending_preview_txd_entry_id, false);
        }
        ui_state.pending_preview_txd_entry_id.reset();
    }

    handle_loader_completions(ui_state);

    const auto& pal = theme_palette();
    const bool busy = loader_.is_busy();

    // Top TXD toolbar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_panel);
    ImGui::BeginChild("##TxdToolbar", ImVec2(0.0f, 44.0f), true, ImGuiWindowFlags_NoScrollbar);
    {
        if (ImGui::Button((std::string(icons::kFolderOpen) + " Open TXD...").c_str()) && !busy) {
            if (auto p = platform::open_file_dialog(
                    native_hwnd,
                    "Open GTA San Andreas TXD Dictionary",
                    {{"RenderWare TXD (*.txd)", "*.txd"}, {"All Files (*.*)", "*.*"}})) {
                loader_.start_load_txd_from_file(*p, false);
            }
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(190.0f);
        ImGui::InputTextWithHint("##TxdSearch", "Search texture...", &search_text_);
        ImGui::SameLine();

        int dim_idx = static_cast<int>(dim_filter_);
        const char* dim_items[] = {"All Sizes", "<= 64 px", "128 px", "256 px", ">= 512 px"};
        ImGui::SetNextItemWidth(110.0f);
        if (ImGui::Combo("##DimFilter", &dim_idx, dim_items, 5)) {
            dim_filter_ = static_cast<DimFilter>(dim_idx);
        }

        ImGui::SameLine();
        int sort_idx = static_cast<int>(sort_mode_);
        const char* sort_items[] = {"Order: Disk", "Sort: Name", "Sort: Size"};
        ImGui::SetNextItemWidth(115.0f);
        if (ImGui::Combo("##TxdSort", &sort_idx, sort_items, 3)) {
            sort_mode_ = static_cast<SortMode>(sort_idx);
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        const char* fmt_items[] = {"PNG", "TGA", "BMP"};
        ImGui::SetNextItemWidth(75.0f);
        ImGui::Combo("##ExpFmt", &export_fmt_idx_, fmt_items, 3);
        ImGui::SameLine();

        const bool has_dict = (dict_ != nullptr && !dict_->textures.empty());
        if (!has_dict || selected_indices_.empty()) ImGui::BeginDisabled();
        if (ImGui::Button("Export Selected")) {
            export_textures(false, native_hwnd, ui_state);
        }
        if (!has_dict || selected_indices_.empty()) ImGui::EndDisabled();
        ImGui::SameLine();
        if (!has_dict) ImGui::BeginDisabled();
        if (ImGui::Button("Export All")) {
            export_textures(true, native_hwnd, ui_state);
        }
        if (!has_dict) ImGui::EndDisabled();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    const float body_h = std::max(220.0f, ImGui::GetContentRegionAvail().y);
    const float grid_w = 360.0f;
    const float meta_w = 310.0f;
    const float preview_w = std::max(240.0f, ImGui::GetContentRegionAvail().x - grid_w - meta_w - 16.0f);

    // 1. Left: Thumbnail grid
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_panel);
    ImGui::BeginChild("##TxdThumbGrid", ImVec2(grid_w, body_h), true);
    {
        if (busy) {
            ImGui::TextColored(pal.accent_hover, "%s Loading TXD...", icons::kArrowsRotate);
        } else if (!dict_) {
            ImGui::TextDisabled("Open a .txd file or double-click/right-click a .txd entry in Archive Explorer.");
            if (!last_error_.empty()) {
                ImGui::TextColored(pal.danger, "%s", last_error_.c_str());
            }
        } else {
            const std::string needle = core::to_ascii_lower(search_text_);
            std::vector<std::size_t> visible;
            visible.reserve(dict_->textures.size());
            for (std::size_t i = 0; i < dict_->textures.size(); ++i) {
                const auto& t = dict_->textures[i];
                if (!needle.empty() && core::to_ascii_lower(t.name).find(needle) == std::string::npos) {
                    continue;
                }
                const std::uint16_t max_d = std::max(t.width, t.height);
                if (dim_filter_ == DimFilter::Le64 && max_d > 64U) continue;
                if (dim_filter_ == DimFilter::Size128 && max_d != 128U) continue;
                if (dim_filter_ == DimFilter::Size256 && max_d != 256U) continue;
                if (dim_filter_ == DimFilter::Ge512 && max_d < 512U) continue;
                visible.push_back(i);
            }

            if (sort_mode_ == SortMode::NameAsc) {
                std::sort(visible.begin(), visible.end(), [&](std::size_t a, std::size_t b) {
                    return core::to_ascii_lower(dict_->textures[a].name) <
                           core::to_ascii_lower(dict_->textures[b].name);
                });
            } else if (sort_mode_ == SortMode::SizeDesc) {
                std::sort(visible.begin(), visible.end(), [&](std::size_t a, std::size_t b) {
                    const std::uint32_t sa = static_cast<std::uint32_t>(dict_->textures[a].width) * dict_->textures[a].height;
                    const std::uint32_t sb = static_cast<std::uint32_t>(dict_->textures[b].width) * dict_->textures[b].height;
                    return sa > sb;
                });
            }

            ImGui::TextDisabled("Showing %zu / %zu textures", visible.size(), dict_->textures.size());
            ImGui::Separator();

            const int cols = 2;
            const float cell_w = (grid_w - 34.0f) / static_cast<float>(cols);
            for (std::size_t k = 0; k < visible.size(); ++k) {
                const std::size_t idx = visible[k];
                const auto& tex = dict_->textures[idx];
                const bool sel = selected_indices_.count(idx) > 0U;

                if (k % static_cast<std::size_t>(cols) != 0U) {
                    ImGui::SameLine();
                }

                ImGui::PushID(static_cast<int>(idx));
                ImGui::PushStyleColor(ImGuiCol_ChildBg, sel ? ImVec4(0.18f, 0.27f, 0.42f, 1.0f) : pal.bg_surface);
                ImGui::BeginChild("##ThumbCard", ImVec2(cell_w, 138.0f), true, ImGuiWindowFlags_NoScrollbar);
                {
                    if (idx < thumb_textures_.size() && thumb_textures_[idx].valid()) {
                        ImGui::Image(
                            static_cast<ImTextureID>(static_cast<std::uintptr_t>(thumb_textures_[idx].id())),
                            ImVec2(cell_w - 18.0f, 82.0f));
                    } else {
                        ImGui::Dummy(ImVec2(cell_w - 18.0f, 82.0f));
                        ImGui::SetCursorPos(ImVec2(12.0f, 38.0f));
                        ImGui::TextColored(pal.warning, "Unsupported");
                    }
                    ImGui::Text("%s", tex.name.empty() ? "(unnamed)" : tex.name.c_str());
                    ImGui::TextDisabled("%ux%u  %s", tex.width, tex.height, tex.format_name.c_str());
                }
                ImGui::EndChild();
                if (ImGui::IsItemClicked()) {
                    const ImGuiIO& io = ImGui::GetIO();
                    if (io.KeyCtrl) {
                        if (sel) selected_indices_.erase(idx);
                        else selected_indices_.insert(idx);
                    } else {
                        selected_indices_.clear();
                        selected_indices_.insert(idx);
                    }
                    primary_index_ = idx;
                    selected_mip_ = 0U;
                    rebuild_preview_gpu_texture();
                }
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine();

    // 2. Center: Texture preview
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_root);
    ImGui::BeginChild("##TxdPreviewPane", ImVec2(preview_w, body_h), true);
    {
        int ch = static_cast<int>(channel_view_);
        const char* ch_items[] = {"RGBA", "RGB", "Alpha"};
        ImGui::SetNextItemWidth(85.0f);
        if (ImGui::Combo("##Chan", &ch, ch_items, 3)) {
            channel_view_ = static_cast<ChannelView>(ch);
            rebuild_preview_gpu_texture();
        }
        ImGui::SameLine();

        int bg = static_cast<int>(bg_style_);
        const char* bg_items[] = {"Checker", "Dark", "Light"};
        ImGui::SetNextItemWidth(95.0f);
        if (ImGui::Combo("##BgStyle", &bg, bg_items, 3)) {
            bg_style_ = static_cast<BgStyle>(bg);
        }
        ImGui::SameLine();

        int flt = static_cast<int>(filter_mode_);
        const char* flt_items[] = {"Linear", "Nearest"};
        ImGui::SetNextItemWidth(90.0f);
        if (ImGui::Combo("##Filter", &flt, flt_items, 2)) {
            filter_mode_ = static_cast<render::TextureFilterMode>(flt);
            preview_gpu_tex_.set_filter(filter_mode_);
        }
        ImGui::SameLine();
        ImGui::Checkbox("Fit", &fit_viewport_);
        if (!fit_viewport_) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110.0f);
            ImGui::SliderFloat("##Zoom", &zoom_factor_, 0.25f, 8.0f, "%.2fx");
        }

        if (dict_ && primary_index_ < dict_->textures.size() && !dict_->textures[primary_index_].mipmaps.empty()) {
            const auto& tex = dict_->textures[primary_index_];
            ImGui::SameLine();
            ImGui::SetNextItemWidth(135.0f);
            std::string cur_mip_lbl = "Mip " + std::to_string(selected_mip_) + " (" +
                std::to_string(tex.mipmaps[selected_mip_].width) + "x" +
                std::to_string(tex.mipmaps[selected_mip_].height) + ")";
            if (ImGui::BeginCombo("##MipSel", cur_mip_lbl.c_str())) {
                for (std::size_t m = 0; m < tex.mipmaps.size(); ++m) {
                    std::string ml = "Mip " + std::to_string(m) + " (" +
                        std::to_string(tex.mipmaps[m].width) + "x" +
                        std::to_string(tex.mipmaps[m].height) + ")";
                    if (ImGui::Selectable(ml.c_str(), m == selected_mip_)) {
                        selected_mip_ = m;
                        rebuild_preview_gpu_texture();
                    }
                }
                ImGui::EndCombo();
            }
        }

        ImGui::Separator();

        const ImVec2 canvas_avail = ImGui::GetContentRegionAvail();
        const ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();

        if (bg_style_ == BgStyle::Dark) {
            dl->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_avail.x, canvas_pos.y + canvas_avail.y), IM_COL32(20, 22, 27, 255));
        } else if (bg_style_ == BgStyle::Light) {
            dl->AddRectFilled(canvas_pos, ImVec2(canvas_pos.x + canvas_avail.x, canvas_pos.y + canvas_avail.y), IM_COL32(230, 234, 240, 255));
        } else {
            const float step = 16.0f;
            dl->PushClipRect(canvas_pos, ImVec2(canvas_pos.x + canvas_avail.x, canvas_pos.y + canvas_avail.y), true);
            for (float y = 0.0f; y < canvas_avail.y; y += step) {
                for (float x = 0.0f; x < canvas_avail.x; x += step) {
                    const bool odd = (static_cast<int>(x / step) + static_cast<int>(y / step)) & 1;
                    const ImU32 col = odd ? IM_COL32(52, 56, 66, 255) : IM_COL32(36, 39, 46, 255);
                    dl->AddRectFilled(
                        ImVec2(canvas_pos.x + x, canvas_pos.y + y),
                        ImVec2(canvas_pos.x + x + step, canvas_pos.y + y + step),
                        col);
                }
            }
            dl->PopClipRect();
        }

        if (preview_gpu_tex_.valid()) {
            const float tw = static_cast<float>(preview_gpu_tex_.width());
            const float th = static_cast<float>(preview_gpu_tex_.height());
            float disp_w = tw * zoom_factor_;
            float disp_h = th * zoom_factor_;
            if (fit_viewport_ && canvas_avail.x > 16.0f && canvas_avail.y > 16.0f) {
                const float scale = std::min((canvas_avail.x - 24.0f) / tw, (canvas_avail.y - 24.0f) / th);
                disp_w = tw * scale;
                disp_h = th * scale;
            }
            const float ox = std::max(0.0f, (canvas_avail.x - disp_w) * 0.5f);
            const float oy = std::max(0.0f, (canvas_avail.y - disp_h) * 0.5f);
            ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x + ox, canvas_pos.y + oy));
            ImGui::Image(
                static_cast<ImTextureID>(static_cast<std::uintptr_t>(preview_gpu_tex_.id())),
                ImVec2(disp_w, disp_h));
        } else if (dict_ && primary_index_ < dict_->textures.size()) {
            const auto& tex = dict_->textures[primary_index_];
            ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x + 16.0f, canvas_pos.y + 16.0f));
            ImGui::TextColored(pal.warning, "Texture cannot be rendered: %s", tex.unsupported_reason.c_str());
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine();

    // 3. Right: Texture metadata inspector
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_panel);
    ImGui::BeginChild("##TxdInspector", ImVec2(0.0f, body_h), true);
    {
        if (fonts.semibold) ImGui::PushFont(fonts.semibold);
        ImGui::TextUnformatted("TEXTURE INSPECTOR");
        if (fonts.semibold) ImGui::PopFont();
        ImGui::Separator();

        if (!dict_ || primary_index_ >= dict_->textures.size()) {
            ImGui::TextDisabled("Select a texture to inspect metadata.");
        } else {
            const auto& t = dict_->textures[primary_index_];
            if (fonts.semibold) ImGui::PushFont(fonts.semibold);
            ImGui::TextWrapped("%s", t.name.c_str());
            if (fonts.semibold) ImGui::PopFont();
            ImGui::TextDisabled("Dictionary: %s (RW %s)",
                dict_->source_name.c_str(),
                core::rw::format_rw_version(dict_->rw_version).c_str());
            ImGui::Separator();
            ImGui::Text("Dimensions:  %u x %u", t.width, t.height);
            ImGui::Text("Format:      %s", t.format_name.c_str());
            ImGui::Text("Platform ID: %u", t.platform_id);
            ImGui::Text("Depth:       %u bpp", t.depth);
            ImGui::Text("Mipmaps:     %zu decoded (%u declared)", t.mipmaps.size(), t.mipmap_count_declared);
            ImGui::Text("Has Alpha:   %s", t.has_alpha ? "Yes" : "No");
            ImGui::Text("Compressed:  %s", t.is_compressed ? "Yes" : "No");
            ImGui::Text("Paletted:    %s", t.is_paletted ? "Yes" : "No");
            ImGui::Text("Cubemap:     %s", t.is_cubemap ? "Yes" : "No");
            if (!t.mask_name.empty()) {
                ImGui::Text("Alpha Mask:  %s", t.mask_name.c_str());
            }
            if (!t.supported) {
                ImGui::Spacing();
                ImGui::TextColored(pal.warning, "Unsupported: %s", t.unsupported_reason.c_str());
            }

            if (!dict_->warnings.empty()) {
                ImGui::Spacing();
                ImGui::TextColored(pal.warning, "Dictionary Warnings (%zu)", dict_->warnings.size());
                ImGui::Separator();
                for (const auto& w : dict_->warnings) {
                    ImGui::TextWrapped("• %s%s%s",
                        w.texture_name.empty() ? "" : w.texture_name.c_str(),
                        w.texture_name.empty() ? "" : ": ",
                        w.message.c_str());
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace imgstudio::ui
