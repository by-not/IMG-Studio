#include "ui/MainView.hpp"
#include "app/Localization.hpp"
#include "platform/FileDialogs.hpp"
#include "platform/Platform.hpp"
#include "ui/Dialogs.hpp"
#include "ui/ExplorerTable.hpp"
#include "ui/Icons.hpp"
#include "ui/InspectorPanel.hpp"
#include "ui/Theme.hpp"

#include <imgui.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace imgstudio::ui {

namespace {

std::string format_bytes_short(std::uint64_t bytes) {
    char buf[64];
    if (bytes < 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    } else if (bytes < 1024ULL * 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%.1f KiB", static_cast<double>(bytes) / 1024.0);
    } else if (bytes < 1024ULL * 1024ULL * 1024ULL) {
        std::snprintf(buf, sizeof(buf), "%.2f MiB", static_cast<double>(bytes) / (1024.0 * 1024.0));
    } else {
        std::snprintf(buf, sizeof(buf), "%.2f GiB", static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0));
    }
    return buf;
}

void open_archive_prompt(app::ArchiveController& controller, void* native_hwnd) {
    if (controller.is_busy()) return;
    if (auto p = platform::open_file_dialog(
            native_hwnd,
            app::tr("dialog.open_img.title"),
            {{"IMG Archives (*.img)", "*.img"}, {"All Files (*.*)", "*.*"}})) {
        controller.start_open_archive(*p);
    }
}

void create_archive_prompt(app::ArchiveController& controller, void* native_hwnd) {
    if (controller.is_busy()) return;
    if (auto p = platform::save_file_dialog(
            native_hwnd,
            app::tr("dialog.new_img.title"),
            "new_archive.img",
            {{"IMG Archives (*.img)", "*.img"}, {"All Files (*.*)", "*.*"}})) {
        controller.start_create_archive(*p);
    }
}

void import_files_prompt(app::ArchiveController& controller, UiState& state, void* native_hwnd) {
    if (controller.is_busy() || !controller.active_document()) return;
    auto files = platform::open_files_dialog(
        native_hwnd,
        app::tr("dialog.import.title"),
        {{"GTA Assets (*.dff;*.txd;*.col;*.ifp;*.ipl;*.ide)", "*.dff;*.txd;*.col;*.ifp;*.ipl;*.ide"},
         {"All Files (*.*)", "*.*"}});
    if (!files.empty()) {
        trigger_import_files(controller, state, files);
    }
}

void handle_shortcuts(
    app::ArchiveController& controller,
    UiState& state,
    void* native_hwnd,
    bool& request_window_close) {
    if (ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId)) {
        return;
    }
    const ImGuiIO& io = ImGui::GetIO();
    const bool busy = controller.is_busy();
    auto* doc = controller.active_document();

    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O, false) && !busy) {
        open_archive_prompt(controller, native_hwnd);
    } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N, false) && !busy) {
        create_archive_prompt(controller, native_hwnd);
    } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false) && !busy && doc && doc->has_pending_changes()) {
        state.commit_auto_backup_override = controller.settings().auto_backup_before_commit;
        state.open_commit_modal = true;
    } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_I, false) && !busy && doc) {
        import_files_prompt(controller, state, native_hwnd);
    } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false) && doc) {
        state.focus_search_requested = true;
    } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_E, false) && !busy && doc) {
        trigger_export_entries(controller, state, native_hwnd, false);
    } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_W, false) && !busy && controller.active_tab_index().has_value()) {
        const std::size_t idx = *controller.active_tab_index();
        if (doc && doc->has_pending_changes()) {
            state.close_tab_pending = idx;
            state.close_app_after_confirm = false;
            state.open_close_confirm_modal = true;
        } else {
            controller.close_tab(idx);
            state.selected_ids.clear();
            state.primary_selected_id.reset();
            state.filter_dirty = true;
        }
    } else if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false) && !io.KeyShift && !busy && doc) {
        if (doc->undo()) {
            state.filter_dirty = true;
        }
    } else if (((io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) ||
                (io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z, false))) &&
               !busy && doc) {
        if (doc->redo()) {
            state.filter_dirty = true;
        }
    } else if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_F2, false) && !busy && doc) {
        trigger_rename_selected(controller, state);
    } else if (!io.WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !busy && doc) {
        trigger_delete_selected(controller, state);
    } else if (ImGui::IsKeyPressed(ImGuiKey_F5, false) && !busy && doc) {
        controller.start_validate_active(false);
    }
    (void)request_window_close;
}

void render_sidebar(
    app::ArchiveController& controller,
    UiState& state,
    const FontBundle& fonts) {
    const auto& pal = theme_palette();
    auto* doc = controller.active_document();

    if (fonts.semibold) ImGui::PushFont(fonts.semibold);
    ImGui::TextUnformatted(app::tr_c("sidebar.summary_title"));
    if (fonts.semibold) ImGui::PopFont();
    ImGui::Separator();

    if (doc) {
        const auto& rep = doc->validation_report();
        ImGui::TextWrapped("%s", platform::to_utf8(doc->path().filename()).c_str());
        if (ImGui::SmallButton(app::tr_c("sidebar.reveal_explorer"))) {
            (void)platform::reveal_in_file_manager(doc->path());
        }
        ImGui::Spacing();
        ImGui::TextDisabled("%s", app::trf("sidebar.entries_stats", {
            {"active", std::to_string(doc->active_entry_count())},
            {"total", std::to_string(doc->entries().size())}
        }).c_str());
        ImGui::TextDisabled("%s", app::trf("sidebar.file_size", {
            {"size", format_bytes_short(doc->file_size_bytes())}
        }).c_str());
        ImGui::TextDisabled("%s", app::trf("sidebar.wasted_gaps", {
            {"size", format_bytes_short(rep.metrics.wasted_gap_bytes)}
        }).c_str());

        if (rep.has_errors()) {
            ImGui::TextColored(pal.danger, "%s %s", icons::kCircleXmark,
                app::trf("sidebar.errors_count", {{"count", std::to_string(rep.error_count)}}).c_str());
        } else if (rep.has_warnings()) {
            ImGui::TextColored(pal.warning, "%s %s", icons::kTriangleWarn,
                app::trf("sidebar.warnings_count", {{"count", std::to_string(rep.warning_count)}}).c_str());
        } else {
            ImGui::TextColored(pal.success, "%s %s", icons::kCircleCheck, app::tr_c("sidebar.valid_ver2"));
        }
    } else {
        ImGui::TextDisabled("%s", app::tr_c("sidebar.no_archive"));
    }

    ImGui::Spacing();
    ImGui::Spacing();
    if (fonts.semibold) ImGui::PushFont(fonts.semibold);
    ImGui::TextUnformatted(app::tr_c("sidebar.quick_filters"));
    if (fonts.semibold) ImGui::PopFont();
    ImGui::Separator();

    struct FilterOpt {
        const char* key;
        app::QuickFilterKind kind;
    };
    const FilterOpt opts[] = {
        {"sidebar.filter.all", app::QuickFilterKind::All},
        {"sidebar.filter.dff", app::QuickFilterKind::Dff},
        {"sidebar.filter.txd", app::QuickFilterKind::Txd},
        {"sidebar.filter.col", app::QuickFilterKind::Col},
        {"sidebar.filter.ifp", app::QuickFilterKind::Ifp},
        {"sidebar.filter.ipl_ide", app::QuickFilterKind::IplIde},
        {"sidebar.filter.other", app::QuickFilterKind::Other},
        {"sidebar.filter.pending", app::QuickFilterKind::PendingChanges},
        {"sidebar.filter.alerts", app::QuickFilterKind::WithAlerts},
    };

    for (const auto& o : opts) {
        const bool selected = (state.filter.quick_filter == o.kind);
        if (ImGui::Selectable(app::tr_c(o.key), selected)) {
            state.filter.quick_filter = o.kind;
            state.filter_dirty = true;
        }
    }

    ImGui::Spacing();
    ImGui::Spacing();
    if (fonts.semibold) ImGui::PushFont(fonts.semibold);
    ImGui::TextUnformatted(app::tr_c("sidebar.recent"));
    if (fonts.semibold) ImGui::PopFont();
    ImGui::Separator();

    const auto& recents = controller.settings().recent_files;
    if (recents.empty()) {
        ImGui::TextDisabled("%s", app::tr_c("sidebar.no_recent"));
    } else {
        for (std::size_t i = 0; i < recents.size(); ++i) {
            const std::string name = platform::to_utf8(recents[i].filename());
            const std::string full = platform::to_utf8(recents[i]);
            ImGui::PushID(static_cast<int>(i));
            if (ImGui::Selectable(name.c_str(), false) && !controller.is_busy()) {
                controller.start_open_archive(recents[i]);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", full.c_str());
            }
            ImGui::PopID();
        }
    }
}

void render_empty_state(app::ArchiveController& controller, void* native_hwnd, const FontBundle& fonts) {
    const auto& pal = theme_palette();
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float card_w = std::min(520.0f, avail.x - 40.0f);

    ImGui::SetCursorPos(ImVec2(
        ImGui::GetCursorPosX() + std::max(0.0f, (avail.x - card_w) * 0.5f),
        ImGui::GetCursorPosY() + std::max(40.0f, (avail.y - 260.0f) * 0.38f)));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_surface);
    ImGui::BeginChild("##EmptyStateCard", ImVec2(card_w, 250.0f), true);

    if (fonts.semibold) ImGui::PushFont(fonts.semibold);
    ImGui::Text("%s  %s", icons::kDatabase, app::tr_c("empty.title"));
    if (fonts.semibold) ImGui::PopFont();
    ImGui::Spacing();
    ImGui::TextWrapped("%s", app::tr_c("empty.body"));
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::PushStyleColor(ImGuiCol_Button, pal.accent);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.accent_hover);
    if (ImGui::Button((std::string(icons::kFolderOpen) + "  " + app::tr("empty.open_btn")).c_str(), ImVec2(215.0f, 36.0f)) &&
        !controller.is_busy()) {
        open_archive_prompt(controller, native_hwnd);
    }
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    if (ImGui::Button((std::string(icons::kFileCirclePlus) + "  " + app::tr("empty.create_btn")).c_str(), ImVec2(205.0f, 36.0f)) &&
        !controller.is_busy()) {
        create_archive_prompt(controller, native_hwnd);
    }

    ImGui::Spacing();
    ImGui::TextDisabled("%s", app::tr_c("empty.shortcuts"));
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace

void render_main_view(
    app::ArchiveController& controller,
    UiState& state,
    StudioModules& modules,
    void* native_hwnd,
    const FontBundle& fonts,
    bool& request_window_close) {
    // Poll async completions from worker thread
    for (const auto& comp : controller.poll_completions()) {
        state.push_toast(comp.message, comp.detail, !comp.ok);
        state.filter_dirty = true;
        if (comp.kind == app::AsyncCompletion::Kind::OpenedArchive ||
            comp.kind == app::AsyncCompletion::Kind::CreatedArchive ||
            comp.kind == app::AsyncCompletion::Kind::CommittedRebuild ||
            comp.kind == app::AsyncCompletion::Kind::RestoredBackup) {
            state.selected_ids.clear();
            state.primary_selected_id.reset();
        }
    }

    auto* doc = controller.active_document();
    const std::uint64_t cur_tab_ptr = reinterpret_cast<std::uint64_t>(doc);
    if (cur_tab_ptr != state.last_seen_tab_ptr) {
        state.last_seen_tab_ptr = cur_tab_ptr;
        state.selected_ids.clear();
        state.primary_selected_id.reset();
        state.filter_dirty = true;
    }

    handle_shortcuts(controller, state, native_hwnd, request_window_close);

    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);
    constexpr ImGuiWindowFlags kRootFlags =
        ImGuiWindowFlags_NoTitleBar |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus |
        ImGuiWindowFlags_MenuBar;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));
    ImGui::Begin("##MainRootWindow", nullptr, kRootFlags);
    ImGui::PopStyleVar(3);

    const bool busy = controller.is_busy();
    const auto& pal = theme_palette();

    // 1. Menu Bar
    if (ImGui::BeginMenuBar()) {
        if (ImGui::BeginMenu((app::tr("menu.file") + "###MenuFile").c_str())) {
            if (ImGui::MenuItem(app::tr_c("menu.file.open"), "Ctrl+O", false, !busy)) {
                open_archive_prompt(controller, native_hwnd);
            }
            if (ImGui::MenuItem(app::tr_c("menu.file.new"), "Ctrl+N", false, !busy)) {
                create_archive_prompt(controller, native_hwnd);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(app::tr_c("menu.file.rebuild_save"), "Ctrl+S", false, !busy && doc && doc->has_pending_changes())) {
                state.commit_auto_backup_override = controller.settings().auto_backup_before_commit;
                state.open_commit_modal = true;
            }
            if (ImGui::MenuItem(app::tr_c("menu.file.close_tab"), "Ctrl+W", false, !busy && controller.active_tab_index().has_value())) {
                const std::size_t idx = *controller.active_tab_index();
                if (doc && doc->has_pending_changes()) {
                    state.close_tab_pending = idx;
                    state.open_close_confirm_modal = true;
                } else {
                    controller.close_tab(idx);
                    state.filter_dirty = true;
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem(app::tr_c("menu.file.settings"), nullptr, false, !busy)) {
                state.settings_draft = controller.settings();
                state.open_settings_modal = true;
            }
            if (ImGui::MenuItem(app::tr_c("menu.file.exit"))) {
                if (controller.any_unsaved_changes()) {
                    state.close_app_after_confirm = true;
                    state.open_close_confirm_modal = true;
                } else {
                    request_window_close = true;
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu((app::tr("menu.edit") + "###MenuEdit").c_str())) {
            const bool can_undo = !busy && doc && doc->history().can_undo();
            const bool can_redo = !busy && doc && doc->history().can_redo();
            if (ImGui::MenuItem(app::tr_c("menu.edit.undo"), "Ctrl+Z", false, can_undo) && doc->undo()) {
                state.filter_dirty = true;
            }
            if (ImGui::MenuItem(app::tr_c("menu.edit.redo"), "Ctrl+Y", false, can_redo) && doc->redo()) {
                state.filter_dirty = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem(app::tr_c("menu.edit.select_all"), "Ctrl+A", false, doc != nullptr)) {
                state.selected_ids.clear();
                for (std::size_t idx : state.visible_indices) {
                    state.selected_ids.insert(doc->entries()[idx].id);
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu((app::tr("menu.entries") + "###MenuEntries").c_str())) {
            if (ImGui::MenuItem(app::tr_c("menu.entries.import"), "Ctrl+I", false, !busy && doc != nullptr)) {
                import_files_prompt(controller, state, native_hwnd);
            }
            if (ImGui::MenuItem(app::tr_c("menu.entries.export_selected"), "Ctrl+E", false, !busy && doc && !state.selected_ids.empty())) {
                trigger_export_entries(controller, state, native_hwnd, false);
            }
            if (ImGui::MenuItem(app::tr_c("menu.entries.export_all"), nullptr, false, !busy && doc != nullptr)) {
                trigger_export_entries(controller, state, native_hwnd, true);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(app::tr_c("menu.entries.replace"), nullptr, false, !busy && doc && state.selected_ids.size() == 1U)) {
                trigger_replace_selected(controller, state, native_hwnd);
            }
            if (ImGui::MenuItem(app::tr_c("menu.entries.rename"), "F2", false, !busy && doc && state.selected_ids.size() == 1U)) {
                trigger_rename_selected(controller, state);
            }
            if (ImGui::MenuItem(app::tr_c("menu.entries.delete"), "Del", false, !busy && doc && !state.selected_ids.empty())) {
                trigger_delete_selected(controller, state);
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu((app::tr("menu.tools") + "###MenuTools").c_str())) {
            if (ImGui::MenuItem(app::tr_c("menu.tools.validate"), "F5", false, !busy && doc != nullptr)) {
                controller.start_validate_active(false);
                state.inspector_tab = 2;
            }
            if (ImGui::MenuItem(app::tr_c("menu.tools.validate_full"), nullptr, false, !busy && doc != nullptr)) {
                controller.start_validate_active(true);
            }
            ImGui::Separator();
            if (ImGui::MenuItem(app::tr_c("menu.tools.manual_backup"), nullptr, false, !busy && doc != nullptr)) {
                controller.start_manual_backup_active("Manual backup from menu");
            }
            if (ImGui::MenuItem(app::tr_c("menu.tools.open_backups"))) {
                (void)platform::open_directory_in_file_manager(controller.backup_manager().backup_directory());
            }
            if (doc != nullptr && ImGui::MenuItem(app::tr_c("menu.tools.send_compare"))) {
                state.pending_compare_archive_a = doc->path();
                state.active_workspace = WorkspaceTab::ImgCompare;
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu((app::tr("menu.view") + "###MenuView").c_str())) {
            if (ImGui::MenuItem(app::tr_c("workspace.explorer"))) state.active_workspace = WorkspaceTab::ArchiveExplorer;
            if (ImGui::MenuItem(app::tr_c("workspace.dff_studio"))) state.active_workspace = WorkspaceTab::DffStudio;
            if (ImGui::MenuItem(app::tr_c("workspace.txd_studio"))) state.active_workspace = WorkspaceTab::TxdStudio;
            if (ImGui::MenuItem(app::tr_c("workspace.img_compare"))) state.active_workspace = WorkspaceTab::ImgCompare;
            if (ImGui::MenuItem(app::tr_c("workspace.batch"))) state.active_workspace = WorkspaceTab::BatchProcessing;
            ImGui::Separator();
            if (ImGui::MenuItem(app::tr_c("menu.view.sidebar"), nullptr, controller.settings().show_sidebar)) {
                controller.settings().show_sidebar = !controller.settings().show_sidebar;
                controller.save_settings();
            }
            if (ImGui::MenuItem(app::tr_c("menu.view.inspector"), nullptr, controller.settings().show_inspector)) {
                controller.settings().show_inspector = !controller.settings().show_inspector;
                controller.save_settings();
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu((app::tr("menu.help") + "###MenuHelp").c_str())) {
            if (ImGui::MenuItem(app::tr_c("menu.help.about"))) {
                state.open_about_modal = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // 1b. Unified Workspace Navigation Bar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_header);
    ImGui::BeginChild("##WorkspaceSwitcher", ImVec2(0.0f, 38.0f), true, ImGuiWindowFlags_NoScrollbar);
    {
        struct WsItem {
            const char* key;
            const char* id;
            WorkspaceTab tab;
        };
        const WsItem ws_items[] = {
            {"workspace.explorer", "###Ws0", WorkspaceTab::ArchiveExplorer},
            {"workspace.dff_studio", "###Ws1", WorkspaceTab::DffStudio},
            {"workspace.txd_studio", "###Ws2", WorkspaceTab::TxdStudio},
            {"workspace.img_compare", "###Ws3", WorkspaceTab::ImgCompare},
            {"workspace.batch", "###Ws4", WorkspaceTab::BatchProcessing},
        };
        for (int wi = 0; wi < 5; ++wi) {
            if (wi > 0) ImGui::SameLine();
            const bool active = (state.active_workspace == ws_items[wi].tab);
            if (active) {
                ImGui::PushStyleColor(ImGuiCol_Button, pal.accent);
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.accent_hover);
            }
            const std::string btn_lbl = app::tr(ws_items[wi].key) + ws_items[wi].id;
            if (ImGui::Button(btn_lbl.c_str(), ImVec2(158.0f, 0.0f))) {
                state.active_workspace = ws_items[wi].tab;
            }
            if (active) {
                ImGui::PopStyleColor(2);
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (state.active_workspace == WorkspaceTab::DffStudio) {
        modules.dff_studio.render(controller, state, native_hwnd, fonts);
        render_modals_and_toasts(controller, state, native_hwnd, request_window_close);
        ImGui::End();
        return;
    }
    if (state.active_workspace == WorkspaceTab::TxdStudio) {
        modules.txd_studio.render(controller, state, native_hwnd, fonts);
        render_modals_and_toasts(controller, state, native_hwnd, request_window_close);
        ImGui::End();
        return;
    }
    if (state.active_workspace == WorkspaceTab::ImgCompare) {
        modules.img_compare.render(controller, state, native_hwnd, fonts);
        render_modals_and_toasts(controller, state, native_hwnd, request_window_close);
        ImGui::End();
        return;
    }
    if (state.active_workspace == WorkspaceTab::BatchProcessing) {
        modules.batch_view.render(controller, state, native_hwnd, fonts);
        render_modals_and_toasts(controller, state, native_hwnd, request_window_close);
        ImGui::End();
        return;
    }

    // 2. Main Toolbar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_panel);
    ImGui::BeginChild("##Toolbar", ImVec2(0.0f, 44.0f), true, ImGuiWindowFlags_NoScrollbar);
    {
        if (ImGui::Button((std::string(icons::kFolderOpen) + " " + app::tr("toolbar.open") + "###TbOpen").c_str()) && !busy) {
            open_archive_prompt(controller, native_hwnd);
        }
        ImGui::SameLine();
        if (ImGui::Button((std::string(icons::kFileCirclePlus) + " " + app::tr("toolbar.new") + "###TbNew").c_str()) && !busy) {
            create_archive_prompt(controller, native_hwnd);
        }
        ImGui::SameLine();

        const bool has_changes = (doc != nullptr && doc->has_pending_changes());
        if (!has_changes || busy) ImGui::BeginDisabled();
        if (has_changes) {
            ImGui::PushStyleColor(ImGuiCol_Button, pal.accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, pal.accent_hover);
        }
        const std::string rebuild_lbl = has_changes
            ? (std::string(icons::kFloppyDisk) + " " +
               app::trf("toolbar.rebuild_save_count", {{"count", std::to_string(doc->pending_change_count())}}) +
               "###TbRebuild")
            : (std::string(icons::kFloppyDisk) + " " + app::tr("toolbar.rebuild_save") + "###TbRebuild");
        if (ImGui::Button(rebuild_lbl.c_str())) {
            state.commit_auto_backup_override = controller.settings().auto_backup_before_commit;
            state.open_commit_modal = true;
        }
        if (has_changes) ImGui::PopStyleColor(2);
        if (!has_changes || busy) ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        if (!doc || busy) ImGui::BeginDisabled();
        if (ImGui::Button((std::string(icons::kFileImport) + " " + app::tr("toolbar.import") + "###TbImport").c_str())) {
            import_files_prompt(controller, state, native_hwnd);
        }
        ImGui::SameLine();
        const bool any_sel = !state.selected_ids.empty();
        if (!any_sel) ImGui::BeginDisabled();
        if (ImGui::Button((std::string(icons::kFileExport) + " " + app::tr("toolbar.export") + "###TbExport").c_str())) {
            trigger_export_entries(controller, state, native_hwnd, false);
        }
        if (!any_sel) ImGui::EndDisabled();

        ImGui::SameLine();
        const bool one_sel = state.selected_ids.size() == 1U;
        if (!one_sel) ImGui::BeginDisabled();
        if (ImGui::Button((std::string(icons::kArrowsRotate) + " " + app::tr("toolbar.replace") + "###TbReplace").c_str())) {
            trigger_replace_selected(controller, state, native_hwnd);
        }
        ImGui::SameLine();
        if (ImGui::Button((std::string(icons::kPen) + " " + app::tr("toolbar.rename") + "###TbRename").c_str())) {
            trigger_rename_selected(controller, state);
        }
        if (!one_sel) ImGui::EndDisabled();

        ImGui::SameLine();
        if (!any_sel) ImGui::BeginDisabled();
        if (ImGui::Button((std::string(icons::kTrash) + " " + app::tr("toolbar.delete") + "###TbDelete").c_str())) {
            trigger_delete_selected(controller, state);
        }
        if (!any_sel) ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        const bool can_undo = doc && doc->history().can_undo();
        if (!can_undo) ImGui::BeginDisabled();
        if (ImGui::Button(icons::kRotateLeft) && doc->undo()) {
            state.filter_dirty = true;
        }
        if (!can_undo) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", app::tr_c("toolbar.undo_tip"));

        ImGui::SameLine();
        const bool can_redo = doc && doc->history().can_redo();
        if (!can_redo) ImGui::BeginDisabled();
        if (ImGui::Button(icons::kRotateRight) && doc->redo()) {
            state.filter_dirty = true;
        }
        if (!can_redo) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", app::tr_c("toolbar.redo_tip"));

        ImGui::SameLine();
        if (ImGui::Button((std::string(icons::kCheckDouble) + " " + app::tr("toolbar.validate") + "###TbVal").c_str())) {
            controller.start_validate_active(false);
        }
        ImGui::SameLine();
        if (ImGui::Button((std::string(icons::kClockRotate) + " " + app::tr("toolbar.backup") + "###TbBkp").c_str())) {
            controller.start_manual_backup_active("Manual toolbar backup");
        }
        if (!doc || busy) ImGui::EndDisabled();

        ImGui::SameLine();
        if (ImGui::Button(icons::kGear) && !busy) {
            state.settings_draft = controller.settings();
            state.open_settings_modal = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", app::tr_c("toolbar.settings_tip"));
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // 3. Archive Tabs
    if (!controller.tabs().empty() && ImGui::BeginTabBar("##ArchiveTabs", ImGuiTabBarFlags_AutoSelectNewTabs | ImGuiTabBarFlags_FittingPolicyScroll)) {
        for (std::size_t i = 0; i < controller.tabs().size(); ++i) {
            const auto& tab_doc = controller.tabs()[i];
            std::string tab_label = platform::to_utf8(tab_doc->path().filename());
            if (tab_doc->has_pending_changes()) {
                tab_label += " *";
            }
            tab_label += "##tab_" + std::to_string(i);

            bool open_flag = true;
            ImGuiTabItemFlags tflags = ImGuiTabItemFlags_None;
            if (controller.active_tab_index() == i) {
                tflags |= ImGuiTabItemFlags_SetSelected;
            }
            if (ImGui::BeginTabItem(tab_label.c_str(), &open_flag, tflags)) {
                if (ImGui::IsItemActivated()) {
                    controller.set_active_tab(i);
                }
                ImGui::EndTabItem();
            }
            if (!open_flag && !busy) {
                if (tab_doc->has_pending_changes()) {
                    state.close_tab_pending = i;
                    state.open_close_confirm_modal = true;
                } else {
                    controller.close_tab(i);
                    state.filter_dirty = true;
                    break;
                }
            }
        }
        ImGui::EndTabBar();
    }

    // 4. Main Workspace (Sidebar + Central Explorer + Inspector)
    const float status_bar_h = 30.0f;
    const float workspace_h = std::max(180.0f, ImGui::GetContentRegionAvail().y - status_bar_h - 6.0f);

    if (controller.settings().show_sidebar) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_panel);
        ImGui::BeginChild("##LeftSidebar", ImVec2(controller.settings().sidebar_width, workspace_h), true);
        render_sidebar(controller, state, fonts);
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    const float inspector_w = controller.settings().show_inspector
        ? (controller.settings().inspector_width + 8.0f)
        : 0.0f;
    const float center_w = std::max(280.0f, ImGui::GetContentRegionAvail().x - inspector_w);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_panel);
    ImGui::BeginChild("##CenterExplorer", ImVec2(center_w, workspace_h), true);
    if (doc) {
        render_explorer_table(controller, state, native_hwnd, fonts);
    } else {
        render_empty_state(controller, native_hwnd, fonts);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (controller.settings().show_inspector) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_panel);
        ImGui::BeginChild("##RightInspector", ImVec2(0.0f, workspace_h), true);
        render_inspector_panel(controller, state, native_hwnd, fonts);
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    // 5. Bottom Status Bar
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_header);
    ImGui::BeginChild("##StatusBar", ImVec2(0.0f, status_bar_h), true, ImGuiWindowFlags_NoScrollbar);
    {
        const auto prog = controller.progress_snapshot();
        if (prog.running) {
            ImGui::TextColored(pal.accent_hover, "%s %s", icons::kArrowsRotate, prog.label.c_str());
            ImGui::SameLine();
            if (prog.total > 0U) {
                const float frac = static_cast<float>(prog.current) / static_cast<float>(prog.total);
                ImGui::ProgressBar(frac, ImVec2(180.0f, 16.0f));
                ImGui::SameLine();
                ImGui::TextDisabled("%zu / %zu (%s)", prog.current, prog.total, prog.detail.c_str());
            } else {
                ImGui::TextDisabled("%s", prog.detail.c_str());
            }
            ImGui::SameLine();
            if (ImGui::SmallButton(app::tr_c("common.cancel"))) {
                controller.cancel_current_task();
            }
        } else if (doc) {
            std::uint64_t sel_bytes = 0;
            for (const auto& e : doc->entries()) {
                if (state.selected_ids.count(e.id) > 0U) {
                    sel_bytes += e.display_bytes();
                }
            }
            ImGui::TextUnformatted(app::tr_c("status.ready"));
            ImGui::SameLine();
            ImGui::TextDisabled("|  %s", app::trf("status.selected", {
                {"count", std::to_string(state.selected_ids.size())},
                {"size", format_bytes_short(sel_bytes)}
            }).c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("|  %s", app::trf("status.pending_changes", {
                {"count", std::to_string(doc->pending_change_count())}
            }).c_str());
            ImGui::SameLine();
            ImGui::TextDisabled("|  %s", app::trf("status.path", {
                {"path", platform::to_utf8(doc->path())}
            }).c_str());
        } else {
            ImGui::TextDisabled("%s", app::tr_c("status.ready_empty"));
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    render_modals_and_toasts(controller, state, native_hwnd, request_window_close);
    ImGui::End();
}

} // namespace imgstudio::ui
