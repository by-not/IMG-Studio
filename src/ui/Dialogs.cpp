#include "ui/Dialogs.hpp"
#include "app/Localization.hpp"
#include "core/ImgFilename.hpp"
#include "platform/FileDialogs.hpp"
#include "platform/Platform.hpp"
#include "ui/ExplorerTable.hpp"
#include "ui/Icons.hpp"
#include "ui/Theme.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <string>

namespace imgstudio::ui {

namespace {

std::string modal_label(std::string_view key, const char* id_suffix) {
    std::string out = app::tr(key);
    out += id_suffix;
    return out;
}

} // namespace

void UiState::push_toast(std::string msg, std::string det, bool is_err, double duration) {
    toasts.push_back(Toast{
        std::move(msg),
        std::move(det),
        is_err,
        ImGui::GetTime() + duration
    });
    if (toasts.size() > 5U) {
        toasts.erase(toasts.begin());
    }
}

void render_modals_and_toasts(
    app::ArchiveController& controller,
    UiState& state,
    void* native_hwnd,
    bool& request_window_close) {
    const auto& pal = theme_palette();
    auto* doc = controller.active_document();

    // Trigger popups from boolean flags using stable ### IDs so live language switches preserve open popups
    if (state.open_rename_modal) {
        ImGui::OpenPopup(modal_label("modal.rename.title", "###RenameModal").c_str());
        state.open_rename_modal = false;
    }
    if (state.open_delete_modal) {
        ImGui::OpenPopup(modal_label("modal.delete.title", "###DeleteModal").c_str());
        state.open_delete_modal = false;
    }
    if (state.open_commit_modal) {
        ImGui::OpenPopup(modal_label("modal.commit.title", "###CommitModal").c_str());
        state.open_commit_modal = false;
    }
    if (state.open_duplicate_modal) {
        ImGui::OpenPopup(modal_label("modal.dup.title", "###DupModal").c_str());
        state.open_duplicate_modal = false;
    }
    if (state.open_settings_modal) {
        state.settings_initial_language = state.settings_draft.language;
        ImGui::OpenPopup(modal_label("modal.settings.title", "###SettingsModal").c_str());
        state.open_settings_modal = false;
    }
    if (state.open_about_modal) {
        ImGui::OpenPopup(modal_label("modal.about.title", "###AboutModal").c_str());
        state.open_about_modal = false;
    }
    if (state.open_close_confirm_modal) {
        ImGui::OpenPopup(modal_label("modal.unsaved.title", "###UnsavedModal").c_str());
        state.open_close_confirm_modal = false;
    }

    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();

    // 1. Rename modal
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(430.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(
            modal_label("modal.rename.title", "###RenameModal").c_str(),
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(app::tr_c("modal.rename.prompt"));
        ImGui::SetNextItemWidth(-1.0f);
        const bool enter_pressed = ImGui::InputText(
            "##RenameInput",
            &state.rename_buffer,
            ImGuiInputTextFlags_EnterReturnsTrue);

        const auto fn_val = core::validate_entry_filename(state.rename_buffer);
        if (!fn_val.valid) {
            ImGui::TextColored(pal.warning, "%s", fn_val.reason.c_str());
        } else if (!state.rename_error.empty()) {
            ImGui::TextColored(pal.danger, "%s", state.rename_error.c_str());
        } else {
            const std::string len_txt = app::trf("modal.rename.length", {
                {"len", std::to_string(state.rename_buffer.size())},
                {"max", std::to_string(core::kMaxFilenameLength)}
            });
            ImGui::TextDisabled("%s", len_txt.c_str());
        }

        ImGui::Spacing();
        const bool can_ok = doc != nullptr && fn_val.valid;
        if (!can_ok) ImGui::BeginDisabled();
        if (ImGui::Button(app::tr_c("modal.rename.apply"), ImVec2(130, 0)) || (enter_pressed && can_ok)) {
            auto res = doc->stage_rename(state.rename_target_id, state.rename_buffer);
            if (!res.ok()) {
                state.rename_error = res.error->message;
            } else {
                state.filter_dirty = true;
                state.push_toast(app::tr("toast.rename_ok"), state.rename_buffer);
                ImGui::CloseCurrentPopup();
            }
        }
        if (!can_ok) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button(app::tr_c("common.cancel"), ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 2. Delete confirmation modal
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(
            modal_label("modal.delete.title", "###DeleteModal").c_str(),
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize)) {
        const std::string prompt = app::trf("modal.delete.prompt", {
            {"count", std::to_string(state.delete_target_ids.size())}
        });
        ImGui::TextUnformatted(prompt.c_str());
        ImGui::TextDisabled("%s", app::tr_c("modal.delete.note"));
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.16f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.danger);
        if (ImGui::Button(app::tr_c("modal.delete.confirm"), ImVec2(140, 0)) && doc != nullptr) {
            auto res = doc->stage_delete(state.delete_target_ids);
            if (res.ok()) {
                state.selected_ids.clear();
                state.primary_selected_id.reset();
                state.filter_dirty = true;
                state.push_toast(
                    app::tr("toast.delete_ok"),
                    app::trf("toast.delete_ok_detail", {{"count", std::to_string(*res.value)}}));
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        if (ImGui::Button(app::tr_c("common.cancel"), ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 3. Rebuild & Save confirmation modal
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(
            modal_label("modal.commit.title", "###CommitModal").c_str(),
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize)) {
        if (doc != nullptr) {
            const std::string arch_line = app::trf("modal.commit.archive", {
                {"path", platform::to_utf8(doc->path())}
            });
            const std::string act_line = app::trf("modal.commit.active_entries", {
                {"count", std::to_string(doc->active_entry_count())}
            });
            const std::string mod_line = app::trf("modal.commit.pending_mods", {
                {"count", std::to_string(doc->pending_change_count())}
            });
            ImGui::TextWrapped("%s", arch_line.c_str());
            ImGui::TextUnformatted(act_line.c_str());
            ImGui::TextUnformatted(mod_line.c_str());
            ImGui::Separator();
            ImGui::Checkbox(app::tr_c("modal.commit.auto_backup"), &state.commit_auto_backup_override);
            ImGui::TextDisabled("%s", app::tr_c("modal.commit.note"));
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, pal.accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.accent_hover);
            if (ImGui::Button(app::tr_c("modal.commit.confirm"), ImVec2(165, 0)) && !controller.is_busy()) {
                controller.start_commit_active(state.commit_auto_backup_override);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            if (ImGui::Button(app::tr_c("common.cancel"), ImVec2(100, 0))) {
                ImGui::CloseCurrentPopup();
            }
        } else {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 4. Duplicate conflict modal
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(
            modal_label("modal.dup.title", "###DupModal").c_str(),
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize)) {
        const std::string prompt = app::trf("modal.dup.prompt", {
            {"count", std::to_string(state.pending_duplicate_files.size())}
        });
        ImGui::TextUnformatted(prompt.c_str());
        const std::size_t preview_max = std::min<std::size_t>(6U, state.pending_duplicate_files.size());
        for (std::size_t i = 0; i < preview_max; ++i) {
            ImGui::BulletText("%s", platform::to_utf8(state.pending_duplicate_files[i].filename()).c_str());
        }
        if (state.pending_duplicate_files.size() > preview_max) {
            const std::string more_txt = app::trf("modal.dup.more", {
                {"count", std::to_string(state.pending_duplicate_files.size() - preview_max)}
            });
            ImGui::TextDisabled("%s", more_txt.c_str());
        }
        ImGui::Spacing();
        auto apply_policy = [&](app::DuplicateImportPolicy pol) {
            if (doc != nullptr) {
                auto res = doc->stage_import(state.pending_duplicate_files, pol);
                if (res.ok()) {
                    state.filter_dirty = true;
                    state.push_toast(
                        app::tr("toast.dup_resolved"),
                        app::trf("toast.dup_resolved_detail", {
                            {"replaced", std::to_string(res.value->replaced_count)},
                            {"renamed", std::to_string(res.value->renamed_count)},
                            {"skipped", std::to_string(res.value->skipped_count)}
                        }));
                }
            }
            state.pending_duplicate_files.clear();
            ImGui::CloseCurrentPopup();
        };
        if (ImGui::Button(app::tr_c("modal.dup.replace"), ImVec2(140, 0))) apply_policy(app::DuplicateImportPolicy::Replace);
        ImGui::SameLine();
        if (ImGui::Button(app::tr_c("modal.dup.rename"), ImVec2(125, 0))) apply_policy(app::DuplicateImportPolicy::AutoRename);
        ImGui::SameLine();
        if (ImGui::Button(app::tr_c("modal.dup.skip"), ImVec2(105, 0))) apply_policy(app::DuplicateImportPolicy::Skip);
        ImGui::SameLine();
        if (ImGui::Button(app::tr_c("common.cancel"), ImVec2(85, 0))) {
            state.pending_duplicate_files.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 5. Settings modal
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(560.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(
            modal_label("modal.settings.title", "###SettingsModal").c_str(),
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize)) {
        auto& s = state.settings_draft;
        auto& loc = app::Localization::instance();
        const auto locales = loc.available_locales();

        const std::string sys_lang = platform::detect_system_language();
        std::string sys_native = "English";
        for (const auto& lm : locales) {
            if (lm.code == sys_lang) {
                sys_native = lm.native_name;
                break;
            }
        }
        const std::string auto_label = app::trf("modal.settings.lang_auto", {{"lang", sys_native}});
        std::string current_lang_label = auto_label;
        if (s.language != "auto") {
            for (const auto& lm : locales) {
                if (lm.code == s.language) {
                    current_lang_label = lm.native_name + " (" + lm.code + ")";
                    break;
                }
            }
        }

        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::BeginCombo(app::tr_c("modal.settings.language"), current_lang_label.c_str())) {
            const bool is_auto = (s.language == "auto");
            if (ImGui::Selectable(auto_label.c_str(), is_auto)) {
                s.language = "auto";
                loc.set_language(s.language);
            }
            if (is_auto) ImGui::SetItemDefaultFocus();

            for (const auto& lm : locales) {
                const bool selected = (s.language == lm.code);
                const std::string label = lm.native_name + " (" + lm.code + ")";
                if (ImGui::Selectable(label.c_str(), selected)) {
                    s.language = lm.code;
                    loc.set_language(s.language);
                }
                if (selected) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        std::string bkp_utf8 = platform::to_utf8(s.backup_directory);
        ImGui::TextUnformatted(app::tr_c("modal.settings.backup_dir"));
        ImGui::SetNextItemWidth(390.0f);
        if (ImGui::InputText("##BackupDir", &bkp_utf8)) {
            s.backup_directory = platform::from_utf8(bkp_utf8);
        }
        ImGui::SameLine();
        if (ImGui::Button(app::tr_c("common.browse"))) {
            if (auto chosen = platform::pick_folder_dialog(native_hwnd, app::tr("dialog.pick_backup_dir.title"))) {
                s.backup_directory = *chosen;
            }
        }

        ImGui::Checkbox(app::tr_c("modal.settings.auto_backup"), &s.auto_backup_before_commit);
        ImGui::Checkbox(app::tr_c("modal.settings.show_deleted"), &s.show_deleted_entries);
        ImGui::Checkbox(app::tr_c("modal.settings.verify_open"), &s.verify_payloads_on_open);

        int pol_idx = static_cast<int>(s.duplicate_policy);
        const char* pol_items[] = {
            app::tr_c("modal.settings.dup_ask"),
            app::tr_c("modal.settings.dup_replace"),
            app::tr_c("modal.settings.dup_skip"),
            app::tr_c("modal.settings.dup_autorename")
        };
        ImGui::SetNextItemWidth(260.0f);
        if (ImGui::Combo(app::tr_c("modal.settings.dup_policy"), &pol_idx, pol_items, 4)) {
            s.duplicate_policy = static_cast<app::DuplicateImportPolicy>(pol_idx);
        }

        ImGui::SetNextItemWidth(260.0f);
        ImGui::SliderFloat(app::tr_c("modal.settings.ui_scale"), &s.ui_scale, 0.85f, 1.50f, "%.2fx");

        ImGui::Spacing();
        if (ImGui::Button(app::tr_c("modal.settings.save"), ImVec2(140, 0))) {
            controller.settings() = s;
            loc.set_language(s.language);
            controller.backup_manager().set_backup_directory(s.backup_directory);
            controller.save_settings();
            state.filter_dirty = true;
            state.push_toast(app::tr("toast.settings_saved"), app::tr("toast.settings_saved_detail"));
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button(app::tr_c("common.cancel"), ImVec2(100, 0))) {
            loc.set_language(state.settings_initial_language);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 6. About modal
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal(
            modal_label("modal.about.title", "###AboutModal").c_str(),
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted(app::tr_c("modal.about.header"));
        ImGui::Separator();
        ImGui::TextWrapped("%s", app::tr_c("modal.about.body"));
        ImGui::Spacing();
        if (ImGui::Button(app::tr_c("common.close"), ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 7. Unsaved changes confirm modal
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(
            modal_label("modal.unsaved.title", "###UnsavedModal").c_str(),
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("%s", app::tr_c("modal.unsaved.prompt"));
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.16f, 0.16f, 1.0f));
        if (ImGui::Button(app::tr_c("modal.unsaved.discard"), ImVec2(145, 0))) {
            if (state.close_app_after_confirm) {
                request_window_close = true;
            } else if (state.close_tab_pending.has_value()) {
                controller.close_tab(*state.close_tab_pending);
                state.selected_ids.clear();
                state.primary_selected_id.reset();
                state.filter_dirty = true;
            }
            state.close_tab_pending.reset();
            state.close_app_after_confirm = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button(app::tr_c("modal.unsaved.keep"), ImVec2(130, 0))) {
            state.close_tab_pending.reset();
            state.close_app_after_confirm = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 8. Non-intrusive Toast overlay (bottom-right)
    const double now = ImGui::GetTime();
    state.toasts.erase(
        std::remove_if(state.toasts.begin(), state.toasts.end(), [now](const UiState::Toast& t) {
            return t.expires_at <= now;
        }),
        state.toasts.end());

    if (!state.toasts.empty()) {
        const ImGuiViewport* vp = ImGui::GetMainViewport();
        float y_off = 38.0f;
        for (std::size_t i = state.toasts.size(); i-- > 0;) {
            const auto& t = state.toasts[i];
            const std::string win_name = "##Toast_" + std::to_string(i);
            ImGui::SetNextWindowPos(
                ImVec2(vp->WorkPos.x + vp->WorkSize.x - 16.0f, vp->WorkPos.y + vp->WorkSize.y - y_off),
                ImGuiCond_Always,
                ImVec2(1.0f, 1.0f));
            ImGui::SetNextWindowSizeConstraints(ImVec2(260.0f, 0.0f), ImVec2(400.0f, 140.0f));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, pal.bg_surface);
            ImGui::PushStyleColor(ImGuiCol_Border, t.is_error ? pal.danger : pal.accent);
            constexpr ImGuiWindowFlags kToastFlags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoFocusOnAppearing |
                ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoMove;
            if (ImGui::Begin(win_name.c_str(), nullptr, kToastFlags)) {
                ImGui::TextColored(
                    t.is_error ? pal.danger : pal.success,
                    "%s %s",
                    t.is_error ? icons::kCircleXmark : icons::kCircleCheck,
                    t.message.c_str());
                if (!t.detail.empty()) {
                    ImGui::TextDisabled("%s", t.detail.c_str());
                }
                y_off += ImGui::GetWindowHeight() + 8.0f;
            }
            ImGui::End();
            ImGui::PopStyleColor(2);
        }
    }
}

} // namespace imgstudio::ui
