#include "ui/InspectorPanel.hpp"
#include "core/ImgFilename.hpp"
#include "platform/FileDialogs.hpp"
#include "platform/Platform.hpp"
#include "ui/Icons.hpp"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <fstream>
#include <string>

namespace imgstudio::ui {

namespace {

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
        "Export Validation Report",
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
        state.push_toast("Export failed", "Could not write validation JSON report.", true);
        return;
    }
    out << j.dump(2) << '\n';
    state.push_toast("Validation report exported", platform::to_utf8(dest->filename()));
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
    if (ImGui::BeginTabItem("Properties")) {
        if (!doc || !state.primary_selected_id.has_value()) {
            ImGui::Spacing();
            ImGui::TextDisabled("Select an entry in the explorer to inspect its VER2 metadata.");
        } else {
            const auto* e = doc->find_by_id(*state.primary_selected_id);
            if (!e) {
                ImGui::TextDisabled("Entry no longer exists.");
            } else {
                if (fonts.semibold) ImGui::PushFont(fonts.semibold);
                ImGui::TextWrapped("%s", e->name.c_str());
                if (fonts.semibold) ImGui::PopFont();
                ImGui::TextDisabled("Change State: %s", app::change_state_label(e->change_state));
                ImGui::Separator();

                if (ImGui::BeginTable("##PropTable", 2, ImGuiTableFlags_SizingStretchProp)) {
                    ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthFixed, 125.0f);
                    ImGui::TableSetupColumn("Val", ImGuiTableColumnFlags_WidthStretch);

                    property_row("Extension", e->extension.empty() ? "(none)" : e->extension, fonts.mono);
                    property_row("File Type", std::string(core::file_category_label(e->category)));
                    property_row("Original Index",
                        e->original_index.has_value() ? ("#" + std::to_string(*e->original_index)) : "Pending Import",
                        fonts.mono);

                    char hex_buf[64];
                    std::snprintf(hex_buf, sizeof(hex_buf), "0x%llX (%llu B)",
                        static_cast<unsigned long long>(e->original_offset_bytes),
                        static_cast<unsigned long long>(e->original_offset_bytes));
                    property_row("Sector Offset",
                        e->uses_external_file ? "Assigned on rebuild" : std::to_string(e->offset_sectors),
                        fonts.mono);
                    if (!e->uses_external_file) {
                        property_row("Byte Offset", hex_buf, fonts.mono);
                    }
                    property_row("Streaming Size",
                        std::to_string(e->streaming_size) + " sector(s)",
                        fonts.mono);
                    property_row("Archive Size",
                        std::to_string(e->archive_size) + " sector(s)",
                        fonts.mono);
                    property_row("Effective Size",
                        std::to_string(e->display_bytes()) + " bytes",
                        fonts.mono);
                    if (e->uses_external_file) {
                        property_row("Staged Source", platform::to_utf8(e->external_path), fonts.mono);
                    }
                    if (e->change_state == app::EntryChangeState::Renamed) {
                        property_row("Previous Name", e->original_name, fonts.mono);
                    }

                    ImGui::EndTable();
                }

                if (e->original_index.has_value()) {
                    ImGui::Spacing();
                    ImGui::TextDisabled("Entry Diagnostics");
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
                        ImGui::TextColored(pal.success, "%s No diagnostics for this entry.", icons::kCircleCheck);
                    }
                }
            }
        }
        ImGui::EndTabItem();
    }

    // 2. Pending Changes / Operation History tab
    if (ImGui::BeginTabItem("History")) {
        if (!doc) {
            ImGui::TextDisabled("No archive open.");
        } else {
            const bool busy = controller.is_busy();
            const auto& hist = doc->history();
            if (ImGui::Button("Undo", ImVec2(80, 0)) && !busy && hist.can_undo()) {
                doc->undo();
                state.filter_dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Redo", ImVec2(80, 0)) && !busy && hist.can_redo()) {
                doc->redo();
                state.filter_dirty = true;
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard All") && !busy && doc->has_pending_changes()) {
                doc->discard_pending_changes();
                state.selected_ids.clear();
                state.primary_selected_id.reset();
                state.filter_dirty = true;
                state.push_toast("Pending changes discarded", "Reverted to on-disk directory.");
            }

            ImGui::Spacing();
            ImGui::TextWrapped(
                "Pending changes are staged in memory until you Rebuild & Save. "
                "Committed rebuilds clear this undo stack; restore committed states from Backups.");
            ImGui::Separator();

            if (hist.records().empty()) {
                ImGui::TextDisabled("No pending operations.");
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
    if (ImGui::BeginTabItem("Validation")) {
        if (!doc) {
            ImGui::TextDisabled("No archive open.");
        } else {
            const auto& rep = doc->validation_report();
            const bool busy = controller.is_busy();

            if (ImGui::Button("Quick Check") && !busy) {
                controller.start_validate_active(false);
            }
            ImGui::SameLine();
            if (ImGui::Button("Full Read Check") && !busy) {
                controller.start_validate_active(true);
            }
            ImGui::SameLine();
            if (ImGui::Button("Export JSON") && !busy) {
                export_validation_report_json(*doc, native_hwnd, state);
            }

            ImGui::Spacing();
            ImGui::TextColored(pal.danger, "Errors: %zu", rep.error_count);
            ImGui::SameLine();
            ImGui::TextColored(pal.warning, "Warnings: %zu", rep.warning_count);
            ImGui::SameLine();
            ImGui::TextColored(pal.info_cyan, "Info: %zu", rep.info_count);

            ImGui::Checkbox("Errors", &state.show_val_errors);
            ImGui::SameLine();
            ImGui::Checkbox("Warnings", &state.show_val_warnings);
            ImGui::SameLine();
            ImGui::Checkbox("Info", &state.show_val_info);
            ImGui::Separator();

            ImGui::BeginChild("##ValidationList", ImVec2(0, 0), false);
            if (rep.diagnostics.empty()) {
                ImGui::TextColored(pal.success, "%s Archive passed all VER2 checks.", icons::kCircleCheck);
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
    if (ImGui::BeginTabItem("Backups")) {
        const bool busy = controller.is_busy();
        if (doc && ImGui::Button("Create Manual Backup") && !busy) {
            controller.start_manual_backup_active("User-requested manual backup");
        }
        ImGui::SameLine();
        if (ImGui::Button("Open Folder")) {
            (void)platform::open_directory_in_file_manager(controller.backup_manager().backup_directory());
        }

        ImGui::Separator();
        const auto backups = doc
            ? controller.backup_manager().list_backups(doc->path())
            : controller.backup_manager().list_backups();

        if (backups.empty()) {
            ImGui::TextDisabled("No backups recorded yet for this archive.");
            ImGui::TextWrapped("Automatic backups are created before every Rebuild & Save when enabled in Settings.");
        } else {
            ImGui::BeginChild("##BackupList", ImVec2(0, 0), false);
            for (std::size_t i = 0; i < backups.size(); ++i) {
                const auto& b = backups[i];
                ImGui::PushID(static_cast<int>(i));
                ImGui::Text("%s (%s)", b.id.c_str(), b.kind == app::BackupKind::Automatic ? "Auto" : "Manual");
                ImGui::TextDisabled("%s  |  %llu bytes",
                    b.timestamp_utc.c_str(),
                    static_cast<unsigned long long>(b.size_bytes));
                ImGui::TextDisabled("%s", b.reason.c_str());
                if (!b.file_exists) {
                    ImGui::TextColored(pal.danger, "Backup file missing on disk");
                } else {
                    if (ImGui::SmallButton("Restore") && !busy) {
                        controller.start_restore_backup(b, controller.active_tab_index());
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Reveal")) {
                        (void)platform::reveal_in_file_manager(b.backup_file_path);
                    }
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Copy Recovery Info")) {
                        ImGui::SetClipboardText(app::BackupManager::recovery_instructions(b).c_str());
                        state.push_toast("Copied recovery instructions", b.id);
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
