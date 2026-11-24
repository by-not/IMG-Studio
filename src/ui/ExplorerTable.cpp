#include "ui/ExplorerTable.hpp"
#include "core/ImgFilename.hpp"
#include "platform/FileDialogs.hpp"
#include "platform/Platform.hpp"
#include "ui/Icons.hpp"
#include "ui/Theme.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <cstdio>
#include <string>

namespace imgstudio::ui {

namespace {

std::string format_size_human(std::uint64_t bytes) {
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

const char* category_icon(core::FileCategory cat) noexcept {
    switch (cat) {
        case core::FileCategory::DffModel: return icons::kCube;
        case core::FileCategory::TxdTexture: return icons::kImage;
        case core::FileCategory::ColCollision: return icons::kLayerGroup;
        case core::FileCategory::IfpAnimation:
        case core::FileCategory::ScmScript: return icons::kFileCode;
        default: return icons::kFileLines;
    }
}

std::vector<std::uint64_t> collect_active_selection(
    const app::ArchiveDocument& doc,
    const UiState& state) {
    std::vector<std::uint64_t> out;
    for (const auto& e : doc.entries()) {
        if (e.change_state == app::EntryChangeState::Deleted) {
            continue;
        }
        if (state.selected_ids.count(e.id) > 0U) {
            out.push_back(e.id);
        }
    }
    return out;
}

} // namespace

void trigger_import_files(
    app::ArchiveController& controller,
    UiState& state,
    const std::vector<std::filesystem::path>& files) {
    auto* doc = controller.active_document();
    if (!doc || files.empty()) {
        return;
    }

    auto res = doc->stage_import(files, controller.settings().duplicate_policy);
    if (!res.ok()) {
        state.push_toast("Import failed", res.error->message, true);
        return;
    }

    const auto& r = *res.value;
    state.filter_dirty = true;

    if (!r.unresolved_duplicates.empty()) {
        state.pending_duplicate_files = r.unresolved_duplicates;
        state.open_duplicate_modal = true;
    }

    if (!r.rejected.empty()) {
        std::string first_reason = r.rejected[0].source_path.filename().string() + ": " + r.rejected[0].reason;
        state.push_toast(
            "Imported with " + std::to_string(r.rejected.size()) + " rejected file(s)",
            first_reason,
            true);
    } else if (r.imported_count + r.replaced_count + r.renamed_count > 0U) {
        state.push_toast(
            "Staged import complete",
            "Added: " + std::to_string(r.imported_count) +
                ", Replaced: " + std::to_string(r.replaced_count) +
                ", Renamed: " + std::to_string(r.renamed_count));
    } else if (r.skipped_count > 0U) {
        state.push_toast("Import skipped duplicates", std::to_string(r.skipped_count) + " file(s) skipped.");
    }
}

void trigger_export_entries(
    app::ArchiveController& controller,
    UiState& state,
    void* native_hwnd,
    bool export_all) {
    auto* doc = controller.active_document();
    if (!doc) {
        return;
    }
    std::vector<std::uint64_t> ids;
    if (!export_all) {
        ids = collect_active_selection(*doc, state);
        if (ids.empty()) {
            state.push_toast("Nothing selected", "Select one or more entries to export.", true);
            return;
        }
    }
    auto folder = platform::pick_folder_dialog(
        native_hwnd,
        export_all ? "Export All Archive Entries To Folder" : "Export Selected Entries To Folder");
    if (!folder.has_value()) {
        return;
    }
    controller.start_export_active(std::move(ids), export_all, *folder);
}

void trigger_replace_selected(
    app::ArchiveController& controller,
    UiState& state,
    void* native_hwnd) {
    auto* doc = controller.active_document();
    if (!doc) {
        return;
    }
    if (!state.primary_selected_id.has_value()) {
        state.push_toast("No entry selected", "Select an entry to replace.", true);
        return;
    }
    const auto* entry = doc->find_by_id(*state.primary_selected_id);
    if (!entry || entry->change_state == app::EntryChangeState::Deleted) {
        state.push_toast("Cannot replace", "Selected entry is marked for deletion.", true);
        return;
    }
    auto file = platform::open_file_dialog(
        native_hwnd,
        "Replace '" + entry->name + "' With File",
        {{"All Files (*.*)", "*.*"}});
    if (!file.has_value()) {
        return;
    }
    auto res = doc->stage_replace(entry->id, *file);
    if (!res.ok()) {
        state.push_toast("Replace failed", res.error->message, true);
    } else {
        state.filter_dirty = true;
        state.push_toast("Staged replacement", entry->name);
    }
}

void trigger_rename_selected(
    app::ArchiveController& controller,
    UiState& state) {
    auto* doc = controller.active_document();
    if (!doc || !state.primary_selected_id.has_value()) {
        return;
    }
    const auto* entry = doc->find_by_id(*state.primary_selected_id);
    if (!entry || entry->change_state == app::EntryChangeState::Deleted) {
        return;
    }
    state.rename_target_id = entry->id;
    state.rename_buffer = entry->name;
    state.rename_error.clear();
    state.open_rename_modal = true;
}

void trigger_delete_selected(
    app::ArchiveController& controller,
    UiState& state) {
    auto* doc = controller.active_document();
    if (!doc) {
        return;
    }
    auto ids = collect_active_selection(*doc, state);
    if (ids.empty()) {
        return;
    }
    state.delete_target_ids = std::move(ids);
    state.open_delete_modal = true;
}

void render_explorer_table(
    app::ArchiveController& controller,
    UiState& state,
    void* native_hwnd,
    const FontBundle& fonts) {
    auto* doc = controller.active_document();
    if (!doc) {
        return;
    }

    const auto& pal = theme_palette();
    const auto sev_map = app::build_entry_severity_map(doc->validation_report());
    state.filter.show_deleted = controller.settings().show_deleted_entries;

    if (state.filter_dirty) {
        state.visible_indices = app::filter_and_sort_entries(doc->entries(), sev_map, state.filter);
        state.filter_dirty = false;
    }

    // Search and filter bar
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 6.0f));
    if (state.focus_search_requested) {
        ImGui::SetKeyboardFocusHere();
        state.focus_search_requested = false;
    }
    ImGui::SetNextItemWidth(280.0f);
    if (ImGui::InputTextWithHint("##SearchEntries", "Search filename (Ctrl+F)...", &state.filter.search_text)) {
        state.filter_dirty = true;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(115.0f);
    if (ImGui::InputTextWithHint("##ExtFilter", ".ext filter", &state.filter.extension_filter)) {
        state.filter_dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) {
        state.filter.search_text.clear();
        state.filter.extension_filter.clear();
        state.filter.quick_filter = app::QuickFilterKind::All;
        state.filter_dirty = true;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("Showing %zu / %zu entries", state.visible_indices.size(), doc->entries().size());
    ImGui::PopStyleVar();

    ImGui::Spacing();

    constexpr ImGuiTableFlags kTableFlags =
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_Reorderable |
        ImGuiTableFlags_Hideable |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_BordersOuter |
        ImGuiTableFlags_BordersInnerV |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_Sortable |
        ImGuiTableFlags_SizingStretchProp;

    if (!ImGui::BeginTable("##ArchiveExplorerTable", 8, kTableFlags)) {
        return;
    }

    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_PreferSortAscending, 92.0f, 7);
    ImGui::TableSetupColumn("Filename", ImGuiTableColumnFlags_WidthStretch | ImGuiTableColumnFlags_DefaultSort, 2.2f, 1);
    ImGui::TableSetupColumn("Ext", ImGuiTableColumnFlags_WidthFixed, 64.0f, 2);
    ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 125.0f, 3);
    ImGui::TableSetupColumn("Sector Offset", ImGuiTableColumnFlags_WidthFixed, 108.0f, 4);
    ImGui::TableSetupColumn("Streaming", ImGuiTableColumnFlags_WidthFixed, 105.0f, 5);
    ImGui::TableSetupColumn("Archive", ImGuiTableColumnFlags_WidthFixed, 92.0f, 6);
    ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed, 66.0f, 0);
    ImGui::TableHeadersRow();

    if (ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs()) {
        if (sort_specs->SpecsDirty && sort_specs->SpecsCount > 0) {
            const auto& spec = sort_specs->Specs[0];
            state.filter.sort_column = static_cast<app::SortColumn>(spec.ColumnUserID);
            state.filter.sort_ascending = (spec.SortDirection == ImGuiSortDirection_Ascending);
            state.visible_indices = app::filter_and_sort_entries(doc->entries(), sev_map, state.filter);
            sort_specs->SpecsDirty = false;
        }
    }

    const bool table_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_ChildWindows);
    if (table_focused && !state.visible_indices.empty()) {
        const ImGuiIO& io = ImGui::GetIO();
        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) {
            state.selected_ids.clear();
            for (std::size_t idx : state.visible_indices) {
                state.selected_ids.insert(doc->entries()[idx].id);
            }
            if (!state.visible_indices.empty()) {
                state.primary_selected_id = doc->entries()[state.visible_indices.front()].id;
            }
        }

        int move_delta = 0;
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow, true)) move_delta = 1;
        else if (ImGui::IsKeyPressed(ImGuiKey_UpArrow, true)) move_delta = -1;
        else if (ImGui::IsKeyPressed(ImGuiKey_PageDown, true)) move_delta = 15;
        else if (ImGui::IsKeyPressed(ImGuiKey_PageUp, true)) move_delta = -15;

        if (move_delta != 0) {
            std::size_t cur_row = 0;
            if (state.primary_selected_id.has_value()) {
                for (std::size_t r = 0; r < state.visible_indices.size(); ++r) {
                    if (doc->entries()[state.visible_indices[r]].id == *state.primary_selected_id) {
                        cur_row = r;
                        break;
                    }
                }
            }
            const std::ptrdiff_t next = std::clamp<std::ptrdiff_t>(
                static_cast<std::ptrdiff_t>(cur_row) + move_delta,
                0,
                static_cast<std::ptrdiff_t>(state.visible_indices.size()) - 1);
            const std::uint64_t next_id = doc->entries()[state.visible_indices[static_cast<std::size_t>(next)]].id;
            state.selected_ids.clear();
            state.selected_ids.insert(next_id);
            state.primary_selected_id = next_id;
            state.anchor_visible_row = static_cast<std::size_t>(next);
        }
    }

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(state.visible_indices.size()));
    while (clipper.Step()) {
        for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
            const std::size_t entry_idx = state.visible_indices[static_cast<std::size_t>(row)];
            const auto& entry = doc->entries()[entry_idx];
            const bool is_selected = state.selected_ids.count(entry.id) > 0U;

            ImGui::TableNextRow();
            ImGui::PushID(static_cast<int>(entry.id & 0x7FFFFFFFULL));

            // Col 0: Status
            ImGui::TableSetColumnIndex(0);
            ImVec4 status_col = pal.text_secondary;
            const char* status_icon = icons::kCircleCheck;
            const char* status_text = "Clean";
            if (entry.change_state == app::EntryChangeState::Imported) {
                status_col = pal.success;
                status_icon = icons::kPlus;
                status_text = "Added";
            } else if (entry.change_state == app::EntryChangeState::Replaced) {
                status_col = pal.warning;
                status_icon = icons::kArrowsRotate;
                status_text = "Replaced";
            } else if (entry.change_state == app::EntryChangeState::Renamed) {
                status_col = pal.info_cyan;
                status_icon = icons::kPen;
                status_text = "Renamed";
            } else if (entry.change_state == app::EntryChangeState::Deleted) {
                status_col = pal.danger;
                status_icon = icons::kTrash;
                status_text = "Deleted";
            } else if (entry.original_index.has_value()) {
                auto sit = sev_map.find(*entry.original_index);
                if (sit != sev_map.end()) {
                    if (sit->second == core::DiagnosticSeverity::Error) {
                        status_col = pal.danger;
                        status_icon = icons::kCircleXmark;
                        status_text = "Error";
                    } else if (sit->second == core::DiagnosticSeverity::Warning) {
                        status_col = pal.warning;
                        status_icon = icons::kTriangleWarn;
                        status_text = "Warn";
                    } else {
                        status_col = pal.info_cyan;
                        status_icon = icons::kCircleInfo;
                        status_text = "Info";
                    }
                } else {
                    status_col = pal.success;
                    status_icon = icons::kCircleCheck;
                    status_text = "OK";
                }
            }

            const std::string row_label = std::string("##row_") + std::to_string(entry.id);
            if (ImGui::Selectable(
                    row_label.c_str(),
                    is_selected,
                    ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                const ImGuiIO& io = ImGui::GetIO();
                if (io.KeyShift && state.anchor_visible_row.has_value()) {
                    const std::size_t lo = std::min(*state.anchor_visible_row, static_cast<std::size_t>(row));
                    const std::size_t hi = std::max(*state.anchor_visible_row, static_cast<std::size_t>(row));
                    if (!io.KeyCtrl) {
                        state.selected_ids.clear();
                    }
                    for (std::size_t r = lo; r <= hi && r < state.visible_indices.size(); ++r) {
                        state.selected_ids.insert(doc->entries()[state.visible_indices[r]].id);
                    }
                    state.primary_selected_id = entry.id;
                } else if (io.KeyCtrl) {
                    if (is_selected) {
                        state.selected_ids.erase(entry.id);
                    } else {
                        state.selected_ids.insert(entry.id);
                        state.primary_selected_id = entry.id;
                    }
                    state.anchor_visible_row = static_cast<std::size_t>(row);
                } else {
                    state.selected_ids.clear();
                    state.selected_ids.insert(entry.id);
                    state.primary_selected_id = entry.id;
                    state.anchor_visible_row = static_cast<std::size_t>(row);
                }
            }

            if (ImGui::BeginPopupContextItem("EntryContextMenu")) {
                if (!is_selected) {
                    state.selected_ids.clear();
                    state.selected_ids.insert(entry.id);
                    state.primary_selected_id = entry.id;
                }
                const bool busy = controller.is_busy();
                if (ImGui::MenuItem("Export Selected...", "Ctrl+E", false, !busy)) {
                    trigger_export_entries(controller, state, native_hwnd, false);
                }
                if (ImGui::MenuItem("Replace With File...", nullptr, false, !busy && state.selected_ids.size() == 1U)) {
                    trigger_replace_selected(controller, state, native_hwnd);
                }
                if (ImGui::MenuItem("Rename...", "F2", false, !busy && state.selected_ids.size() == 1U)) {
                    trigger_rename_selected(controller, state);
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Delete Selected", "Del", false, !busy)) {
                    trigger_delete_selected(controller, state);
                }
                ImGui::EndPopup();
            }

            ImGui::SameLine();
            ImGui::TextColored(status_col, "%s %s", status_icon, status_text);

            // Col 1: Filename
            ImGui::TableSetColumnIndex(1);
            if (entry.change_state == app::EntryChangeState::Deleted) {
                ImGui::TextColored(pal.text_muted, "%s  %s", category_icon(entry.category), entry.name.c_str());
            } else {
                ImGui::Text("%s  %s", category_icon(entry.category), entry.name.c_str());
            }

            // Col 2: Extension
            ImGui::TableSetColumnIndex(2);
            ImGui::TextUnformatted(entry.extension.empty() ? "-" : entry.extension.c_str());

            // Col 3: Type
            ImGui::TableSetColumnIndex(3);
            const auto cat_lbl = core::file_category_label(entry.category);
            ImGui::TextUnformatted(cat_lbl.data(), cat_lbl.data() + cat_lbl.size());

            // Col 4: Sector Offset
            ImGui::TableSetColumnIndex(4);
            if (fonts.mono) ImGui::PushFont(fonts.mono);
            if (entry.uses_external_file) {
                ImGui::TextDisabled("pending");
            } else {
                ImGui::Text("%u", entry.offset_sectors);
            }
            if (fonts.mono) ImGui::PopFont();

            // Col 5: Streaming Size
            ImGui::TableSetColumnIndex(5);
            if (fonts.mono) ImGui::PushFont(fonts.mono);
            const std::string sz_str = format_size_human(entry.display_bytes());
            ImGui::Text("%u (%s)", entry.streaming_size, sz_str.c_str());
            if (fonts.mono) ImGui::PopFont();

            // Col 6: Archive Size
            ImGui::TableSetColumnIndex(6);
            if (fonts.mono) ImGui::PushFont(fonts.mono);
            ImGui::Text("%u", entry.archive_size);
            if (fonts.mono) ImGui::PopFont();

            // Col 7: Original Index
            ImGui::TableSetColumnIndex(7);
            if (fonts.mono) ImGui::PushFont(fonts.mono);
            if (entry.original_index.has_value()) {
                ImGui::Text("#%u", *entry.original_index);
            } else {
                ImGui::TextDisabled("new");
            }
            if (fonts.mono) ImGui::PopFont();

            ImGui::PopID();
        }
    }

    ImGui::EndTable();
}

} // namespace imgstudio::ui
