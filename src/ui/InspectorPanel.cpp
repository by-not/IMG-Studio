#include "ui/InspectorPanel.hpp"
#include "app/Localization.hpp"
#include "core/ImgFilename.hpp"
#include "platform/FileDialogs.hpp"
#include "platform/Platform.hpp"
#include "ui/ExplorerTable.hpp"
#include "ui/Icons.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>
#include <string>

namespace imgstudio::ui {

namespace {

const char* localized_change_state(app::EntryChangeState st) {
    switch (st) {
        case app::EntryChangeState::Unchanged: return app::tr_c("explorer.status.clean");
        case app::EntryChangeState::Imported: return app::tr_c("explorer.status.added");
        case app::EntryChangeState::Replaced: return app::tr_c("explorer.status.replaced");
        case app::EntryChangeState::Renamed: return app::tr_c("explorer.status.renamed");
        case app::EntryChangeState::Deleted: return app::tr_c("explorer.status.deleted");
    }
    return app::tr_c("explorer.status.clean");
}

void property_row(const char* label, const std::string& value, ImFont* mono_font = nullptr) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::TextDisabled("%s", label);
    ImGui::TableSetColumnIndex(1);
    if (mono_font) ImGui::PushFont(mono_font);
    ImGui::TextWrapped("%s", value.c_str());
    if (mono_font) ImGui::PopFont();
}

void export_validation_report_json(
    const app::ArchiveDocument& doc,
    void* native_hwnd,
    UiState& state) {
    auto dest = platform::save_file_dialog(
        native_hwnd,
        app::tr("dialog.export_val.title"),
        doc.path().stem().string() + "_validation.json",
        {{"JSON Files (*.json)", "*.json"}, {"All Files (*.*)", "*.*"}});
    if (!dest.has_value()) {
        return;
    }

    const auto& rep = doc.validation_report();
    nlohmann::json j;
    j["archive_path"] = platform::to_utf8(doc.path());
    j["file_size_bytes"] = rep.metrics.file_size_bytes;
    j["declared_entry_count"] = rep.metrics.declared_entry_count;
    j["parsed_entry_count"] = rep.metrics.parsed_entry_count;
    j["error_count"] = rep.error_count;
    j["warning_count"] = rep.warning_count;
    j["info_count"] = rep.info_count;
    j["wasted_gap_bytes"] = rep.metrics.wasted_gap_bytes;
    j["is_fragmented"] = rep.metrics.is_fragmented;

    nlohmann::json diags = nlohmann::json::array();
    for (const auto& d : rep.diagnostics) {
        nlohmann::json item;
        item["severity"] = core::diagnostic_severity_label(d.severity);
        item["code"] = static_cast<int>(d.code);
        if (d.entry_index.has_value()) {
            item["entry_index"] = *d.entry_index;
        }
        item["entry_name"] = d.entry_name;
        item["message"] = d.message;
        diags.push_back(std::move(item));
    }
    j["diagnostics"] = std::move(diags);

    std::ofstream out(*dest, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        state.push_toast(app::tr("toast.export_val_failed"), app::tr("toast.export_val_failed_detail"), true);
        return;
    }
    out << j.dump(2) << '\n';
    state.push_toast(app::tr("toast.export_val_ok"), platform::to_utf8(dest->filename()));
}

} // namespace

void render_inspector_panel(
    app::ArchiveController& controller,
    UiState& state,
    void* native_hwnd,
    const FontBundle& fonts) {
    auto* doc = controller.active_document();
    const auto& pal = theme_palette();

    if (!ImGui::BeginTabBar("##InspectorTabs")) {
        return;
    }

    // 1. Properties tab
    if (ImGui::BeginTabItem((app::tr("inspector.tab.properties") + "###InspProps").c_str())) {
        if (!doc || !state.primary_selected_id.has_value()) {
            ImGui::Spacing();
            ImGui::TextDisabled("%s", app::tr_c("inspector.select_prompt"));
        } else {
            const auto* e = doc->find_by_id(*state.primary_selected_id);
            if (!e) {
                ImGui::TextDisabled("%s", app::tr_c("inspector.entry_missing"));
            } else {
                if (fonts.semibold) ImGui::PushFont(fonts.semibold);
                ImGui::TextWrapped("%s", e->name.c_str());
                if (fonts.semibold) ImGui::PopFont();
                const std::string st_txt = app::trf("inspector.change_state", {
                    {"state", localized_change_state(e->change_state)}
                });
                ImGui::TextDisabled("%s", st_txt.c_str());
                ImGui::Separator();

                if (ImGui::BeginTable("##PropTable", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 125.0f);
                    ImGui::TableSetupColumn("Val", ImGuiTableColumnFlags_WidthStretch);

                    property_row(
                        app::tr_c("inspector.prop.extension"),
                        e->extension.empty() ? app::tr("common.none") : e->extension,
                        fonts.mono);
                    property_row(
                        app::tr_c("inspector.prop.file_type"),
                        localized_category_label(e->category));
                    property_row(
                        app::tr_c("inspector.prop.orig_index"),
                        e->original_index.has_value()
                            ? ("#" + std::to_string(*e->original_index))
                            : app::tr("inspector.prop.pending_import"),
                        fonts.mono);

                    char hex_buf[64];
                    std::snprintf(hex_buf, sizeof(hex_buf), "0x%llX (%llu %s)",
                        static_cast<unsigned long long>(e->original_offset_bytes),
                        static_cast<unsigned long long>(e->original_offset_bytes),
                        app::tr_c("common.bytes"));
                    property_row(
                        app::tr_c("inspector.prop.sector_offset"),
                        e->uses_external_file
                            ? app::tr("inspector.prop.assigned_rebuild")
                            : std::to_string(e->offset_sectors),
                        fonts.mono);
                    if (!e->uses_external_file) {
                        property_row(app::tr_c("inspector.prop.byte_offset"), hex_buf, fonts.mono);
                    }
                    property_row(
                        app::tr_c("inspector.prop.streaming_size"),
                        app::trf("inspector.prop.sectors_unit", {{"count", std::to_string(e->streaming_size)}}),
                        fonts.mono);
                    property_row(
                        app::tr_c("inspector.prop.archive_size"),
                        app::trf("inspector.prop.sectors_unit", {{"count", std::to_string(e->archive_size)}}),
                        fonts.mono);
                    property_row(
                        app::tr_c("inspector.prop.effective_size"),
                        std::to_string(e->display_bytes()) + " " + app::tr("common.bytes"),
                        fonts.mono);
                    if (e->uses_external_file) {
                        property_row(app::tr_c("inspector.prop.staged_source"), platform::to_utf8(e->external_path), fonts.mono);
                    }
                    if (e->change_state == app::EntryChangeState::Renamed) {
                        property_row(app::tr_c("inspector.prop.prev_name"), e->original_name, fonts.mono);
                    }

                    ImGui::EndTable();
                }

                if (e->original_index.has_value()) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("%s", app::tr_c("inspector.entry_diagnostics"));
                    ImGui::Separator();
                    bool any_diag = false;
                    for (const auto& d : doc->validation_report().diagnostics) {
                        if (d.entry_index == e->original_index) {
                            any_diag = true;
                            const ImVec4 c = (d.severity == core::DiagnosticSeverity::Error) ? pal.danger
                                : ((d.severity == core::DiagnosticSeverity::Warning) ? pal.warning : pal.info_cyan);
                            ImGui::TextColored(c, "[%s]", core::diagnostic_severity_label(d.severity).data());
                            ImGui::SameLine();
                            ImGui::TextWrapped("%s", d.message.c_str());
                        }
                    }
                    if (!any_diag) {
                        ImGui::TextColored(pal.success, "%s %s", icons::kCircleCheck, app::tr_c("inspector.no_entry_diagnostics"));
                    }
                }
            }
        }
        ImGui::EndTabItem();
    }

    // 2. Pending Changes / Operation History tab
    if (ImGui::BeginTabItem((app::tr("inspector.tab.history") + "###InspHist").c_str())) {
        if (!doc) {
            ImGui::TextDisabled("%s", app::tr_c("inspector.no_archive_open"));
        } else {
            const bool busy = controller.is_busy();
            const auto& hist = doc->history();
            if (ImGui::Button(app::tr_c("inspector.history.undo"), ImVec2(85, 0)) && !busy && hist.can_undo()) {
                doc->undo();
                state.filter_dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(app::tr_c("inspector.history.redo"), ImVec2(85, 0)) && !busy && hist.can_redo()) {
                doc->redo();
                state.filter_dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button(app::tr_c("inspector.history.discard")) && !busy && doc->has_pending_changes()) {
                doc->discard_pending_changes();
                state.selected_ids.clear();
                state.primary_selected_id.reset();
                state.filter_dirty = true;
                state.push_toast(app::tr("toast.discarded"), app::tr("toast.discarded_detail"));
            }

            ImGui::Spacing();
            ImGui::TextWrapped("%s", app::tr_c("inspector.history.note"));
            ImGui::Separator();

            if (hist.records().empty()) {
                ImGui::TextDisabled("%s", app::tr_c("inspector.history.empty"));
            } else {
                ImGui::BeginChild("##HistoryScroll", ImVec2(0, 0), false);
                for (std::size_t i = 0; i < hist.records().size(); ++i) {
                    const auto& rec = hist.records()[i];
                    const bool active = i < hist.cursor();
                    if (!active) {
                        ImGui::PushStyleColor(ImGuiCol_Text, pal.text_muted);
                    }
                    ImGui::Text("%zu. [%s] %s",
                        i + 1U,
                        app::operation_kind_label(rec.kind),
                        rec.description.c_str());
                    if (!active) {
                        ImGui::PopStyleColor();
                    }
                }
                ImGui::EndChild();
            }
        }
        ImGui::EndTabItem();
    }

    // 3. Validation tab
    if (ImGui::BeginTabItem((app::tr("inspector.tab.validation") + "###InspVal").c_str())) {
        if (!doc) {
            ImGui::TextDisabled("%s", app::tr_c("inspector.no_archive_open"));
        } else {
            const auto& rep = doc->validation_report();
            const bool busy = controller.is_busy();

            if (ImGui::Button(app::tr_c("inspector.validation.quick")) && !busy) {
                controller.start_validate_active(false);
            }
            ImGui::SameLine();
            if (ImGui::Button(app::tr_c("inspector.validation.full")) && !busy) {
                controller.start_validate_active(true);
            }
            ImGui::SameLine();
            if (ImGui::Button(app::tr_c("inspector.validation.export_json")) && !busy) {
                export_validation_report_json(*doc, native_hwnd, state);
            }

            ImGui::Spacing();
            ImGui::TextColored(pal.danger, "%s",
                app::trf("inspector.validation.errors", {{"count", std::to_string(rep.error_count)}}).c_str());
            ImGui::SameLine();
            ImGui::TextColored(pal.warning, "%s",
                app::trf("inspector.validation.warnings", {{"count", std::to_string(rep.warning_count)}}).c_str());
            ImGui::SameLine();
            ImGui::TextColored(pal.info_cyan, "%s",
                app::trf("inspector.validation.info", {{"count", std::to_string(rep.info_count)}}).c_str());

            ImGui::Checkbox(app::tr_c("inspector.validation.filter_errors"), &state.show_val_errors);
            ImGui::SameLine();
            ImGui::Checkbox(app::tr_c("inspector.validation.filter_warnings"), &state.show_val_warnings);
            ImGui::SameLine();
            ImGui::Checkbox(app::tr_c("inspector.validation.filter_info"), &state.show_val_info);
            ImGui::Separator();

            ImGui::BeginChild("##ValidationList", ImVec2(0, 0), false);
            if (rep.diagnostics.empty()) {
                ImGui::TextColored(pal.success, "%s %s", icons::kCircleCheck, app::tr_c("inspector.validation.passed"));
            } else {
                for (std::size_t i = 0; i < rep.diagnostics.size(); ++i) {
                    const auto& d = rep.diagnostics[i];
                    if (d.severity == core::DiagnosticSeverity::Error && !state.show_val_errors) continue;
                    if (d.severity == core::DiagnosticSeverity::Warning && !state.show_val_warnings) continue;
                    if (d.severity == core::DiagnosticSeverity::Info && !state.show_val_info) continue;

                    const ImVec4 col = (d.severity == core::DiagnosticSeverity::Error) ? pal.danger
                        : ((d.severity == core::DiagnosticSeverity::Warning) ? pal.warning : pal.info_cyan);

                    ImGui::PushID(static_cast<int>(i));
                    ImGui::TextColored(col, "[%s]", core::diagnostic_severity_label(d.severity).data());
                    if (d.entry_index.has_value()) {
                        ImGui::SameLine();
                        if (ImGui::SmallButton(d.entry_name.empty()
                                ? ("#" + std::to_string(*d.entry_index)).c_str()
                                : d.entry_name.c_str())) {
                            for (const auto& entry : doc->entries()) {
                                if (entry.original_index == d.entry_index) {
                                    state.selected_ids.clear();
                                    state.selected_ids.insert(entry.id);
                                    state.primary_selected_id = entry.id;
                                    break;
                                }
                            }
                        }
                    }
                    ImGui::TextWrapped("%s", d.message.c_str());
                    ImGui::Separator();
                    ImGui::PopID();
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndTabItem();
    }

    // 4. Backups tab
    if (ImGui::BeginTabItem((app::tr("inspector.tab.backups") + "###InspBkp").c_str())) {
        const bool busy = controller.is_busy();
        if (doc && ImGui::Button(app::tr_c("inspector.backups.create")) && !busy) {
            controller.start_manual_backup_active("User-requested manual backup");
        }
        ImGui::SameLine();
        if (ImGui::Button(app::tr_c("inspector.backups.open_folder"))) {
            (void)platform::open_directory_in_file_manager(controller.backup_manager().backup_directory());
        }

        ImGui::Separator();
        const auto backups = doc
            ? controller.backup_manager().list_backups(doc->path())
            : controller.backup_manager().list_backups();

        if (backups.empty()) {
            ImGui::TextDisabled("%s", app::tr_c("inspector.backups.none"));
            ImGui::TextWrapped("%s", app::tr_c("inspector.backups.hint"));
        } else {
            ImGui::BeginChild("##BackupList", ImVec2(0, 0), false);
            for (std::size_t i = 0; i < backups.size(); ++i) {
                const auto& b = backups[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::Text("%s (%s)",
                    b.id.c_str(),
                    b.kind == app::BackupKind::Automatic
                        ? app::tr_c("inspector.backups.auto")
                        : app::tr_c("inspector.backups.manual"));
                ImGui::TextDisabled("%s  |  %llu %s",
                    b.timestamp_utc.c_str(),
                    static_cast<unsigned long long>(b.size_bytes),
                    app::tr_c("common.bytes"));
                ImGui::TextDisabled("%s", b.reason.c_str());
                if (!b.file_exists) {
                    ImGui::TextColored(pal.danger, "%s", app::tr_c("inspector.backups.missing"));
                } else {
                    if (ImGui::SmallButton(app::tr_c("inspector.backups.restore")) && !busy) {
                        controller.start_restore_backup(b, controller.active_tab_index());
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton(app::tr_c("inspector.backups.reveal"))) {
                        (void)platform::reveal_in_file_manager(b.backup_file_path);
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton(app::tr_c("inspector.backups.copy_info"))) {
                        ImGui::SetClipboardText(app::BackupManager::recovery_instructions(b).c_str());
                        state.push_toast(app::tr("toast.copied_recovery"), b.id);
                    }
                }
                ImGui::Separator();
                ImGui::PopID();
            }
            ImGui::EndChild();
        }
        ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
}

} // namespace imgstudio::ui
