#include "ui/DffStudioView.hpp"
#include "app/Localization.hpp"
#include "core/ImgFilename.hpp"
#include "core/rw/RwStream.hpp"
#include "platform/FileDialogs.hpp"
#include "platform/Platform.hpp"
#include "ui/Icons.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>

namespace imgstudio::ui {

void DffStudioView::shutdown() {
    renderer_.shutdown();
    session_.reset();
}

bool DffStudioView::open_file_sync(const std::filesystem::path& path) {
    auto res = app::AssetLoader::load_dff_from_file_sync(path);
    if (!res.ok()) {
        last_load_error_ = res.error->message;
        return false;
    }
    session_ = std::move(*res.value);
    last_load_error_.clear();
    renderer_.upload_session(*session_);
    renderer_.fit_camera(camera_, view_settings_);
    return true;
}

bool DffStudioView::open_from_document_sync(const app::ArchiveDocument& doc, std::uint64_t entry_id) {
    auto res = app::AssetLoader::load_dff_from_document_sync(doc, entry_id);
    if (!res.ok()) {
        last_load_error_ = res.error->message;
        return false;
    }
    session_ = std::move(*res.value);
    last_load_error_.clear();
    renderer_.upload_session(*session_);
    renderer_.fit_camera(camera_, view_settings_);
    return true;
}

void DffStudioView::render_smoke_frame(int width, int height) {
    renderer_.render(width, height, camera_, view_settings_);
}

void DffStudioView::handle_loader_completions(UiState& ui_state) {
    for (auto& comp : loader_.poll_completions()) {
        if (!comp.ok) {
            last_load_error_ = comp.message;
            ui_state.push_toast(comp.message, comp.detail, true);
            continue;
        }
        if (comp.kind == app::AssetLoadCompletion::Kind::DffLoaded && comp.dff_session.has_value()) {
            session_ = std::move(*comp.dff_session);
            last_load_error_.clear();
            renderer_.upload_session(*session_);
            renderer_.fit_camera(camera_, view_settings_);
            ui_state.push_toast(app::tr("toast.dff_loaded"), session_->model.source_name);
        } else if (comp.kind == app::AssetLoadCompletion::Kind::AdditionalTxdLoaded &&
                   comp.txd_dict && session_.has_value()) {
            session_->loaded_txds.insert(session_->loaded_txds.begin(), comp.txd_dict);
            app::AssetLoader::resolve_dff_textures(*session_);
            renderer_.refresh_textures(*session_);
            ui_state.push_toast(app::tr("toast.txd_attached"), comp.txd_dict->source_name);
        }
    }
}

void DffStudioView::render(
    app::ArchiveController& controller,
    UiState& ui_state,
    void* native_hwnd,
    const FontBundle& fonts) {
    if (ui_state.pending_preview_dff_entry_id.has_value()) {
        if (const auto* doc = controller.active_document()) {
            loader_.start_load_dff_from_document(*doc, *ui_state.pending_preview_dff_entry_id);
        }
        ui_state.pending_preview_dff_entry_id.reset();
    }

    handle_loader_completions(ui_state);

    const auto& pal = theme_palette();
    const bool busy = loader_.is_busy();

    // Top control strip
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_panel);
    ImGui::BeginChild("##DffToolbar", ImVec2(0.0f, 44.0f), true, ImGuiWindowFlags_NoScrollbar);
    {
        if (ImGui::Button((std::string(icons::kFolderOpen) + " " + app::tr("dff.open_btn") + "###DffOpen").c_str()) && !busy) {
            if (auto p = platform::open_file_dialog(
                    native_hwnd,
                    app::tr("dff.open_dialog"),
                    {{"RenderWare DFF (*.dff)", "*.dff"}, {"All Files (*.*)", "*.*"}})) {
                loader_.start_load_dff_from_file(*p);
            }
        }
        ImGui::SameLine();
        if (!session_.has_value() || busy) ImGui::BeginDisabled();
        if (ImGui::Button((std::string(icons::kImage) + " " + app::tr("dff.load_txd_btn") + "###DffLoadTxd").c_str())) {
            if (auto p = platform::open_file_dialog(
                    native_hwnd,
                    app::tr("dff.load_txd_dialog"),
                    {{"RenderWare TXD (*.txd)", "*.txd"}, {"All Files (*.*)", "*.*"}})) {
                loader_.start_load_txd_from_file(*p, true);
            }
        }
        if (!session_.has_value() || busy) ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        int mode_idx = static_cast<int>(view_settings_.mode);
        const char* mode_items[] = {
            app::tr_c("dff.mode.textured"),
            app::tr_c("dff.mode.solid"),
            app::tr_c("dff.mode.wireframe"),
            app::tr_c("dff.mode.materials")
        };
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::Combo("##RenderMode", &mode_idx, mode_items, 4)) {
            view_settings_.mode = static_cast<render::DffRenderMode>(mode_idx);
        }

        ImGui::SameLine();
        ImGui::Checkbox((app::tr("dff.grid") + "###DffGrid").c_str(), &view_settings_.show_grid);
        ImGui::SameLine();
        ImGui::Checkbox((app::tr("dff.axes") + "###DffAxes").c_str(), &view_settings_.show_axes);
        ImGui::SameLine();
        ImGui::Checkbox((app::tr("dff.cull") + "###DffCull").c_str(), &view_settings_.backface_cull);
        ImGui::SameLine();
        if (ImGui::Checkbox((app::tr("dff.center") + "###DffCenter").c_str(), &view_settings_.center_model)) {
            renderer_.fit_camera(camera_, view_settings_);
        }
        ImGui::SameLine();
        if (ImGui::Button((app::tr("dff.fit") + "###DffFit").c_str())) {
            renderer_.fit_camera(camera_, view_settings_);
        }
        ImGui::SameLine();
        if (ImGui::Button((app::tr("dff.reset_cam") + "###DffResetCam").c_str())) {
            camera_ = render::OrbitCamera{};
            renderer_.fit_camera(camera_, view_settings_);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::ColorEdit3("##BgCol", view_settings_.bg_color.data(), ImGuiColorEditFlags_NoInputs);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", app::tr_c("dff.bg_tip"));
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    const float info_w = 350.0f;
    const float body_h = std::max(220.0f, ImGui::GetContentRegionAvail().y);
    const float vp_w = std::max(240.0f, ImGui::GetContentRegionAvail().x - info_w - 8.0f);

    // 3D Viewport
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_root);
    ImGui::BeginChild("##DffViewportPanel", ImVec2(vp_w, body_h), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const int rw = std::max(64, static_cast<int>(avail.x));
        const int rh = std::max(64, static_cast<int>(avail.y));

        if (!renderer_.is_initialized() && !renderer_.initialize()) {
            ImGui::TextColored(pal.danger, "%s",
                app::trf("dff.gl_error", {{"error", renderer_.last_error()}}).c_str());
        } else {
            renderer_.render(rw, rh, camera_, view_settings_);
            const ImVec2 img_pos = ImGui::GetCursorScreenPos();
            ImGui::Image(
                static_cast<ImTextureID>(static_cast<std::uintptr_t>(renderer_.output_texture())),
                ImVec2(static_cast<float>(rw), static_cast<float>(rh)),
                ImVec2(0.0f, 1.0f),
                ImVec2(1.0f, 0.0f));

            if (ImGui::IsItemHovered()) {
                const ImGuiIO& io = ImGui::GetIO();
                if (io.MouseWheel != 0.0f) {
                    camera_.zoom(io.MouseWheel);
                }
                if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                    if (io.KeyShift) {
                        camera_.pan(io.MouseDelta.x, io.MouseDelta.y, rh);
                    } else {
                        camera_.orbit(-io.MouseDelta.x * 0.45f, io.MouseDelta.y * 0.45f);
                    }
                } else if (ImGui::IsMouseDragging(ImGuiMouseButton_Right) ||
                           ImGui::IsMouseDragging(ImGuiMouseButton_Middle)) {
                    camera_.pan(io.MouseDelta.x, io.MouseDelta.y, rh);
                }
            }

            if (busy) {
                ImGui::SetCursorScreenPos(ImVec2(img_pos.x + 14.0f, img_pos.y + 14.0f));
                ImGui::TextColored(pal.accent_hover, "%s %s", icons::kArrowsRotate, app::tr_c("dff.loading"));
            } else if (!session_.has_value()) {
                ImGui::SetCursorScreenPos(ImVec2(img_pos.x + 18.0f, img_pos.y + 18.0f));
                ImGui::TextDisabled("%s", app::tr_c("dff.empty_prompt"));
                if (!last_load_error_.empty()) {
                    ImGui::SetCursorScreenPos(ImVec2(img_pos.x + 18.0f, img_pos.y + 42.0f));
                    ImGui::TextColored(pal.danger, "%s",
                        app::trf("dff.last_error", {{"error", last_load_error_}}).c_str());
                }
            } else {
                ImGui::SetCursorScreenPos(ImVec2(img_pos.x + 12.0f, img_pos.y + static_cast<float>(rh) - 24.0f));
                ImGui::TextDisabled("%s", app::tr_c("dff.controls_hint"));
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::SameLine();

    // Right Model Inspector
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_panel);
    ImGui::BeginChild("##DffInfoPanel", ImVec2(0.0f, body_h), true);
    {
        if (fonts.semibold) ImGui::PushFont(fonts.semibold);
        ImGui::TextUnformatted(app::tr_c("dff.inspector_title"));
        if (fonts.semibold) ImGui::PopFont();
        ImGui::Separator();

        ImGui::TextDisabled("%s", app::tr_c("dff.lighting"));
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat((app::tr("dff.azimuth") + "###DffAzimuth").c_str(), &view_settings_.light_azimuth_deg, -180.0f, 180.0f, "%.0f deg");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat((app::tr("dff.elevation") + "###DffElevation").c_str(), &view_settings_.light_elevation_deg, -89.0f, 89.0f, "%.0f deg");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::SliderFloat((app::tr("dff.ambient") + "###DffAmbient").c_str(), &view_settings_.ambient_strength, 0.05f, 0.90f, "%.2f");
        ImGui::Separator();

        if (!session_.has_value()) {
            ImGui::TextDisabled("%s", app::tr_c("dff.no_model"));
        } else {
            const auto& m = session_->model;
            if (fonts.semibold) ImGui::PushFont(fonts.semibold);
            ImGui::TextWrapped("%s", m.source_name.c_str());
            if (fonts.semibold) ImGui::PopFont();
            ImGui::TextDisabled("%s", app::trf("dff.rw_meta", {
                {"ver", core::rw::format_rw_version(m.rw_version)},
                {"size", std::to_string(m.file_size_bytes)}
            }).c_str());
            ImGui::TextUnformatted(app::trf("dff.geom_mats", {
                {"geoms", std::to_string(m.geometries.size())},
                {"mats", std::to_string(m.total_materials)}
            }).c_str());
            ImGui::TextUnformatted(app::trf("dff.verts_tris", {
                {"verts", std::to_string(m.total_vertices)},
                {"tris", std::to_string(m.total_triangles)}
            }).c_str());

            if (m.bounds.valid) {
                char bbuf[128];
                std::snprintf(bbuf, sizeof(bbuf), "%.2f x %.2f x %.2f",
                    m.bounds.max.x - m.bounds.min.x,
                    m.bounds.max.y - m.bounds.min.y,
                    m.bounds.max.z - m.bounds.min.z);
                if (fonts.mono) ImGui::PushFont(fonts.mono);
                ImGui::TextUnformatted(app::trf("dff.bounds", {{"bounds", bbuf}}).c_str());
                if (fonts.mono) ImGui::PopFont();
            }

            ImGui::Spacing();
            if (fonts.semibold) ImGui::PushFont(fonts.semibold);
            ImGui::TextUnformatted(app::trf("dff.tex_refs", {
                {"count", std::to_string(m.referenced_textures.size())}
            }).c_str());
            if (fonts.semibold) ImGui::PopFont();
            ImGui::Separator();

            if (m.referenced_textures.empty()) {
                ImGui::TextDisabled("%s", app::tr_c("dff.no_tex_refs"));
            } else {
                for (const auto& tname : m.referenced_textures) {
                    const std::string key = core::to_ascii_lower(tname);
                    auto it = session_->resolved_textures.find(key);
                    if (it != session_->resolved_textures.end() && it->second.resolved) {
                        ImGui::TextColored(pal.success, "%s %s", icons::kCircleCheck, tname.c_str());
                        ImGui::SameLine();
                        ImGui::TextDisabled("(%s)", it->second.source_txd_name.c_str());
                    } else {
                        ImGui::TextColored(pal.warning, "%s %s (%s)", icons::kTriangleWarn, tname.c_str(), app::tr_c("dff.missing_tex"));
                    }
                }
            }

            if (!session_->loaded_txds.empty()) {
                ImGui::Spacing();
                ImGui::TextDisabled("%s", app::tr_c("dff.loaded_txds"));
                for (const auto& td : session_->loaded_txds) {
                    ImGui::BulletText("%s", app::trf("dff.txd_entry_count", {
                        {"name", td->source_name},
                        {"count", std::to_string(td->textures.size())}
                    }).c_str());
                }
            }

            if (!m.warnings.empty() || !session_->resolution_notes.empty()) {
                ImGui::Spacing();
                if (fonts.semibold) ImGui::PushFont(fonts.semibold);
                ImGui::TextColored(pal.warning, "%s", app::trf("dff.diagnostics", {
                    {"count", std::to_string(m.warnings.size() + session_->resolution_notes.size())}
                }).c_str());
                if (fonts.semibold) ImGui::PopFont();
                ImGui::Separator();
                for (const auto& w : m.warnings) {
                    ImGui::TextColored(w.is_error ? pal.danger : pal.warning, "• %s", w.message.c_str());
                }
                for (const auto& n : session_->resolution_notes) {
                    ImGui::TextColored(pal.info_cyan, "• %s", n.c_str());
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace imgstudio::ui
