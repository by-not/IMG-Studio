#include "ui/Dialogs.hpp"
#include "core/ImgFilename.hpp"
#include "platform/FileDialogs.hpp"
#include "platform/Platform.hpp"
#include "ui/ExplorerTable.hpp"
#include "ui/Icons.hpp"
#include "ui/Theme.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>

namespace imgstudio::ui {

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

    // Trigger popups from boolean flags
    if (state.open_rename_modal) {
        ImGui::OpenPopup("Rename Entry");
        state.open_rename_modal = false;
    }
    if (state.open_delete_modal) {
        ImGui::OpenPopup("Confirm Deletion");
        state.open_delete_modal = false;
    }
    if (state.open_commit_modal) {
        ImGui::OpenPopup("Rebuild & Save Archive");
        state.open_commit_modal = false;
    }
    if (state.open_duplicate_modal) {
        ImGui::OpenPopup("Duplicate Entry Conflict");
        state.open_duplicate_modal = false;
    }
    if (state.open_settings_modal) {
        ImGui::OpenPopup("Settings");
        state.open_settings_modal = false;
    }
    if (state.open_about_modal) {
        ImGui::OpenPopup("About IMG Studio");
        state.open_about_modal = false;
    }
    if (state.open_close_confirm_modal) {
        ImGui::OpenPopup("Unsaved Changes");
        state.open_close_confirm_modal = false;
    }

    const ImVec2 center = ImGui::GetMainViewport()->GetCenter();

    // 1. Rename modal
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(430.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Rename Entry", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Enter new VER2 entry name (max 23 ASCII characters):");
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
            ImGui::TextDisabled("Length: %zu / %zu", state.rename_buffer.size(), core::kMaxFilenameLength);
        }

        ImGui::Spacing();
        const bool can_ok = doc != nullptr && fn_val.valid;
        if (!can_ok) ImGui::BeginDisabled();
        if (ImGui::Button("Apply Rename", ImVec2(130, 0)) || (enter_pressed && can_ok)) {
            auto res = doc->stage_rename(state.rename_target_id, state.rename_buffer);
            if (!res.ok()) {
                state.rename_error = res.error->message;
            } else {
                state.filter_dirty = true;
                state.push_toast("Staged rename", state.rename_buffer);
                ImGui::CloseCurrentPopup();
            }
        }
        if (!can_ok) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 2. Delete confirmation modal
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Confirm Deletion", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Mark %zu selected entry(s) for deletion?", state.delete_target_ids.size());
        ImGui::TextDisabled("Deleted entries remain recoverable via Undo or Discard until Rebuild & Save.");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.16f, 0.16f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.danger);
        if (ImGui::Button("Mark Deleted", ImVec2(130, 0)) && doc != nullptr) {
            auto res = doc->stage_delete(state.delete_target_ids);
            if (res.ok()) {
                state.selected_ids.clear();
                state.primary_selected_id.reset();
                state.filter_dirty = true;
                state.push_toast("Marked for deletion", std::to_string(*res.value) + " entry(s)");
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 3. Rebuild & Save confirmation modal
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Rebuild & Save Archive", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        if (doc != nullptr) {
            ImGui::TextWrapped("Archive: %s", platform::to_utf8(doc->path()).c_str());
            ImGui::Text("Active entries after rebuild: %zu", doc->active_entry_count());
            ImGui::Text("Pending modifications: %zu", doc->pending_change_count());
            ImGui::Separator();
            ImGui::Checkbox("Create automatic backup before replacing archive", &state.commit_auto_backup_override);
            ImGui::TextDisabled("Rebuild writes to a temporary file, verifies SHA-256 payloads, and replaces atomically.");
            ImGui::Spacing();

            ImGui::PushStyleColor(ImGuiCol_Button, pal.accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.accent_hover);
            if (ImGui::Button("Rebuild & Commit", ImVec2(150, 0)) && !controller.is_busy()) {
                controller.start_commit_active(state.commit_auto_backup_override);
                ImGui::CloseCurrentPopup();
            }
            ImGui::PopStyleColor(2);
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(100, 0))) {
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
    if (ImGui::BeginPopupModal("Duplicate Entry Conflict", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("%zu imported file(s) match existing entry names (case-insensitive):",
            state.pending_duplicate_files.size());
        const std::size_t preview_max = std::min<std::size_t>(6U, state.pending_duplicate_files.size());
        for (std::size_t i = 0; i < preview_max; ++i) {
            ImGui::BulletText("%s", platform::to_utf8(state.pending_duplicate_files[i].filename()).c_str());
        }
        if (state.pending_duplicate_files.size() > preview_max) {
            ImGui::TextDisabled("...and %zu more.", state.pending_duplicate_files.size() - preview_max);
        }
        ImGui::Spacing();
        auto apply_policy = [&](app::DuplicateImportPolicy pol) {
            if (doc != nullptr) {
                auto res = doc->stage_import(state.pending_duplicate_files, pol);
                if (res.ok()) {
                    state.filter_dirty = true;
                    state.push_toast("Resolved duplicates",
                        "Replaced: " + std::to_string(res.value->replaced_count) +
                        ", Renamed: " + std::to_string(res.value->renamed_count) +
                        ", Skipped: " + std::to_string(res.value->skipped_count));
                }
            }
            state.pending_duplicate_files.clear();
            ImGui::CloseCurrentPopup();
        };
        if (ImGui::Button("Replace Existing", ImVec2(135, 0))) apply_policy(app::DuplicateImportPolicy::Replace);
        ImGui::SameLine();
        if (ImGui::Button("Auto-Rename", ImVec2(120, 0))) apply_policy(app::DuplicateImportPolicy::AutoRename);
        ImGui::SameLine();
        if (ImGui::Button("Skip All", ImVec2(95, 0))) apply_policy(app::DuplicateImportPolicy::Skip);
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(85, 0))) {
            state.pending_duplicate_files.clear();
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 5. Settings modal
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(540.0f, 0.0f), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        auto& s = state.settings_draft;
        std::string bkp_utf8 = platform::to_utf8(s.backup_directory);
        ImGui::TextUnformatted("Backup Directory:");
        ImGui::SetNextItemWidth(380.0f);
        if (ImGui::InputText("##BackupDir", &bkp_utf8)) {
            s.backup_directory = platform::from_utf8(bkp_utf8);
        }
        ImGui::SameLine();
        if (ImGui::Button("Browse...")) {
            if (auto chosen = platform::pick_folder_dialog(native_hwnd, "Select Backup Directory")) {
                s.backup_directory = *chosen;
            }
        }

        ImGui::Checkbox("Create automatic backup before Rebuild & Save", &s.auto_backup_before_commit);
        ImGui::Checkbox("Show entries marked for deletion in explorer", &s.show_deleted_entries);
        ImGui::Checkbox("Verify full payload readability when opening archives", &s.verify_payloads_on_open);

        int pol_idx = static_cast<int>(s.duplicate_policy);
        const char* pol_items[] = {"Ask every time", "Replace existing", "Skip duplicates", "Auto-rename incoming"};
        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::Combo("Duplicate import policy", &pol_idx, pol_items, 4)) {
            s.duplicate_policy = static_cast<app::DuplicateImportPolicy>(pol_idx);
        }

        ImGui::SetNextItemWidth(240.0f);
        ImGui::SliderFloat("UI Scale (restart for fonts)", &s.ui_scale, 0.85f, 1.50f, "%.2fx");

        ImGui::Spacing();
        if (ImGui::Button("Save Settings", ImVec2(130, 0))) {
            controller.settings() = s;
            controller.backup_manager().set_backup_directory(s.backup_directory);
            controller.save_settings();
            state.filter_dirty = true;
            state.push_toast("Settings saved", "Preferences persisted.");
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 6. About modal
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("About IMG Studio", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("IMG Studio v2.0.0 (Windows x64)");
        ImGui::Separator();
        ImGui::TextWrapped(
            "Independent GTA San Andreas IMG VER2, RenderWare DFF & TXD Studio built with C++20, "
            "Dear ImGui, GLFW and OpenGL 3.3. Includes streaming archive engine, SHA-256 verification, "
            "automatic backups, 3D DFF viewer, TXD texture inspector, IMG Compare and Batch Processing.");
        ImGui::Spacing();
        if (ImGui::Button("Close", ImVec2(100, 0))) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    // 7. Unsaved changes confirm modal
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("There are pending uncommitted changes. Discard them and close?");
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.16f, 0.16f, 1.0f));
        if (ImGui::Button("Discard & Close", ImVec2(135, 0))) {
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
        if (ImGui::Button("Keep Editing", ImVec2(120, 0))) {
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
