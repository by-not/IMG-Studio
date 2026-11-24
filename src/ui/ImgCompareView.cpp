#include "ui/ImgCompareView.hpp"
#include "app/Localization.hpp"
#include "core/ImgFilename.hpp"
#include "platform/FileDialogs.hpp"
#include "platform/Platform.hpp"
#include "ui/Icons.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <string>
#include <vector>

namespace imgstudio::ui {

namespace {

const char* localized_compare_diff(core::CompareDiffKind kind) {
    switch (kind) {
        case core::CompareDiffKind::Unchanged: return app::tr_c("compare.diff.unchanged");
        case core::CompareDiffKind::Added: return app::tr_c("compare.diff.added");
        case core::CompareDiffKind::Removed: return app::tr_c("compare.diff.removed");
        case core::CompareDiffKind::Modified: return app::tr_c("compare.diff.modified");
        case core::CompareDiffKind::MetadataDiff: return app::tr_c("compare.diff.metadata");
        case core::CompareDiffKind::Ambiguous: return app::tr_c("compare.diff.ambiguous");
    }
    return app::tr_c("compare.diff.unchanged");
}

} // namespace

void ImgCompareView::render(
    app::ArchiveController& controller,
    UiState& ui_state,
    void* native_hwnd,
    const FontBundle& fonts) {
    if (ui_state.pending_compare_archive_a.has_value()) {
        archive_a_ = *ui_state.pending_compare_archive_a;
        ui_state.pending_compare_archive_a.reset();
    }

    if (auto comp = service_.poll_completion()) {
        ui_state.push_toast(comp->message, comp->detail, !comp->ok);
        if (comp->ok && comp->result.has_value()) {
            result_ = std::move(*comp->result);
            selected_row_.reset();
        }
    }

    const auto& pal = theme_palette();
    const bool busy = service_.is_busy();

    // Top selection panel
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_panel);
    ImGui::BeginChild("##CompareTop", ImVec2(0.0f, 118.0f), true);
    {
        std::string a_str = platform::to_utf8(archive_a_);
        std::string b_str = platform::to_utf8(archive_b_);

        ImGui::TextUnformatted(app::tr_c("compare.archive_a"));
        ImGui::SameLine(95.0f);
        ImGui::SetNextItemWidth(430.0f);
        if (ImGui::InputText("##CompA", &a_str)) {
            archive_a_ = platform::from_utf8(a_str);
        }
        ImGui::SameLine();
        if (ImGui::Button((app::tr("compare.browse_a") + "###CmpBrowseA").c_str()) && !busy) {
            if (auto p = platform::open_file_dialog(
                    native_hwnd, app::tr("compare.select_a"), {{"IMG Archives (*.img)", "*.img"}})) {
                archive_a_ = *p;
            }
        }
        ImGui::SameLine();
        if (const auto* doc = controller.active_document()) {
            if (ImGui::Button((app::tr("compare.use_active_a") + "###CmpActiveA").c_str()) && !busy) {
                archive_a_ = doc->path();
            }
        }

        ImGui::TextUnformatted(app::tr_c("compare.archive_b"));
        ImGui::SameLine(95.0f);
        ImGui::SetNextItemWidth(430.0f);
        if (ImGui::InputText("##CompB", &b_str)) {
            archive_b_ = platform::from_utf8(b_str);
        }
        ImGui::SameLine();
        if (ImGui::Button((app::tr("compare.browse_b") + "###CmpBrowseB").c_str()) && !busy) {
            if (auto p = platform::open_file_dialog(
                    native_hwnd, app::tr("compare.select_b"), {{"IMG Archives (*.img)", "*.img"}})) {
                archive_b_ = *p;
            }
        }
        ImGui::SameLine();
        if (const auto* doc = controller.active_document()) {
            if (ImGui::Button((app::tr("compare.use_active_b") + "###CmpActiveB").c_str()) && !busy) {
                archive_b_ = doc->path();
            }
        }

        const bool can_run = !busy && !archive_a_.empty() && !archive_b_.empty();
        if (!can_run) ImGui::BeginDisabled();
        ImGui::PushStyleColor(ImGuiCol_Button, pal.accent);
        if (ImGui::Button((std::string(icons::kCheckDouble) + " " + app::tr("compare.run") + "###CmpRun").c_str(), ImVec2(190.0f, 0.0f))) {
            service_.start_compare(archive_a_, archive_b_);
        }
        ImGui::PopStyleColor();
        if (!can_run) ImGui::EndDisabled();

        if (busy) {
            ImGui::SameLine();
            const auto p = service_.progress_snapshot();
            if (p.total > 0U) {
                ImGui::ProgressBar(static_cast<float>(p.current) / static_cast<float>(p.total), ImVec2(180.0f, 0.0f));
                ImGui::SameLine();
            }
            if (ImGui::Button(app::tr_c("common.cancel"))) {
                service_.cancel();
            }
        } else if (result_.has_value()) {
            ImGui::SameLine();
            if (ImGui::Button((app::tr("compare.export_json") + "###CmpExpJson").c_str())) {
                if (auto dest = platform::save_file_dialog(
                        native_hwnd, app::tr("compare.export_json_title"), "img_compare_report.json",
                        {{"JSON Files (*.json)", "*.json"}})) {
                    auto er = app::CompareService::export_report_json(*result_, *dest);
                    ui_state.push_toast(
                        er.ok() ? app::tr("toast.cmp_json_ok") : app::tr("toast.cmp_export_failed"),
                        er.ok() ? platform::to_utf8(dest->filename()) : er.error->message,
                        !er.ok());
                }
            }
            ImGui::SameLine();
            if (ImGui::Button((app::tr("compare.export_txt") + "###CmpExpTxt").c_str())) {
                if (auto dest = platform::save_file_dialog(
                        native_hwnd, app::tr("compare.export_txt_title"), "img_compare_report.txt",
                        {{"Text Files (*.txt)", "*.txt"}})) {
                    auto er = app::CompareService::export_report_text(*result_, *dest);
                    ui_state.push_toast(
                        er.ok() ? app::tr("toast.cmp_txt_ok") : app::tr("toast.cmp_export_failed"),
                        er.ok() ? platform::to_utf8(dest->filename()) : er.error->message,
                        !er.ok());
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (!result_.has_value()) {
        ImGui::Spacing();
        ImGui::TextDisabled("%s", app::tr_c("compare.empty_prompt"));
        return;
    }

    const auto& r = *result_;
    // Summary & filters
    ImGui::TextUnformatted(app::trf("compare.counts_prefix", {
        {"a", std::to_string(r.counts.archive_a_entries)},
        {"b", std::to_string(r.counts.archive_b_entries)}
    }).c_str());
    ImGui::SameLine(); ImGui::TextColored(pal.text_secondary, "%s", app::trf("compare.unchanged", {{"count", std::to_string(r.counts.unchanged)}}).c_str());
    ImGui::SameLine(); ImGui::TextColored(pal.success, "%s", app::trf("compare.added", {{"count", std::to_string(r.counts.added)}}).c_str());
    ImGui::SameLine(); ImGui::TextColored(pal.danger, "%s", app::trf("compare.removed", {{"count", std::to_string(r.counts.removed)}}).c_str());
    ImGui::SameLine(); ImGui::TextColored(pal.warning, "%s", app::trf("compare.modified", {{"count", std::to_string(r.counts.modified)}}).c_str());
    ImGui::SameLine(); ImGui::TextColored(pal.info_cyan, "%s", app::trf("compare.metadata", {{"count", std::to_string(r.counts.metadata_diff)}}).c_str());
    ImGui::SameLine(); ImGui::TextColored(pal.danger, "%s", app::trf("compare.ambiguous", {{"count", std::to_string(r.counts.ambiguous)}}).c_str());

    ImGui::SetNextItemWidth(240.0f);
    ImGui::InputTextWithHint("##CmpSearch", app::tr_c("compare.search_hint"), &search_text_);
    ImGui::SameLine();
    const char* f_items[] = {
        app::tr_c("compare.filter.all"),
        app::tr_c("compare.filter.diff"),
        app::tr_c("compare.filter.added"),
        app::tr_c("compare.filter.removed"),
        app::tr_c("compare.filter.modified"),
        app::tr_c("compare.filter.metadata"),
        app::tr_c("compare.filter.ambiguous")
    };
    ImGui::SetNextItemWidth(160.0f);
    ImGui::Combo("##CmpFilter", &diff_filter_, f_items, 7);

    const std::string needle = core::to_ascii_lower(search_text_);
    std::vector<std::size_t> visible;
    visible.reserve(r.entries.size());
    for (std::size_t i = 0; i < r.entries.size(); ++i) {
        const auto& e = r.entries[i];
        if (!needle.empty() && core::to_ascii_lower(e.display_name).find(needle) == std::string::npos) {
            continue;
        }
        if (diff_filter_ == 1 && e.kind == core::CompareDiffKind::Unchanged) continue;
        if (diff_filter_ == 2 && e.kind != core::CompareDiffKind::Added) continue;
        if (diff_filter_ == 3 && e.kind != core::CompareDiffKind::Removed) continue;
        if (diff_filter_ == 4 && e.kind != core::CompareDiffKind::Modified) continue;
        if (diff_filter_ == 5 && e.kind != core::CompareDiffKind::MetadataDiff) continue;
        if (diff_filter_ == 6 && e.kind != core::CompareDiffKind::Ambiguous) continue;
        visible.push_back(i);
    }

    const float detail_h = 130.0f;
    const float table_h = std::max(140.0f, ImGui::GetContentRegionAvail().y - detail_h - 8.0f);

    if (ImGui::BeginTable("##CmpTable", 6,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0.0f, table_h))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn((app::tr("compare.col.status") + "###CmpColStatus").c_str(), ImGuiTableColumnFlags_WidthFixed, 110.0f);
        ImGui::TableSetupColumn((app::tr("compare.col.name") + "###CmpColName").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupColumn((app::tr("compare.col.size_a") + "###CmpColA").c_str(), ImGuiTableColumnFlags_WidthFixed, 125.0f);
        ImGui::TableSetupColumn((app::tr("compare.col.size_b") + "###CmpColB").c_str(), ImGuiTableColumnFlags_WidthFixed, 125.0f);
        ImGui::TableSetupColumn((app::tr("compare.col.meta") + "###CmpColMeta").c_str(), ImGuiTableColumnFlags_WidthFixed, 165.0f);
        ImGui::TableSetupColumn((app::tr("compare.col.detail") + "###CmpColDetail").c_str(), ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableHeadersRow();

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(visible.size()));
        while (clipper.Step()) {
            for (int row = clipper.DisplayStart; row < clipper.DisplayEnd; ++row) {
                const std::size_t idx = visible[static_cast<std::size_t>(row)];
                const auto& item = r.entries[idx];
                const bool sel = (selected_row_.has_value() && *selected_row_ == idx);

                ImGui::TableNextRow();
                ImGui::PushID(static_cast<int>(idx));
                ImGui::TableSetColumnIndex(0);

                ImVec4 col = pal.text_secondary;
                if (item.kind == core::CompareDiffKind::Added) col = pal.success;
                else if (item.kind == core::CompareDiffKind::Removed || item.kind == core::CompareDiffKind::Ambiguous) col = pal.danger;
                else if (item.kind == core::CompareDiffKind::Modified) col = pal.warning;
                else if (item.kind == core::CompareDiffKind::MetadataDiff) col = pal.info_cyan;

                if (ImGui::Selectable(("##cmp_" + std::to_string(idx)).c_str(), sel,
                        ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap)) {
                    selected_row_ = idx;
                }
                ImGui::SameLine();
                ImGui::TextColored(col, "%s", localized_compare_diff(item.kind));

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(item.display_name.c_str());

                ImGui::TableSetColumnIndex(2);
                if (fonts.mono) ImGui::PushFont(fonts.mono);
                if (item.side_a.present) ImGui::Text("%llu", static_cast<unsigned long long>(item.side_a.effective_bytes));
                else ImGui::TextDisabled("-");
                if (fonts.mono) ImGui::PopFont();

                ImGui::TableSetColumnIndex(3);
                if (fonts.mono) ImGui::PushFont(fonts.mono);
                if (item.side_b.present) ImGui::Text("%llu", static_cast<unsigned long long>(item.side_b.effective_bytes));
                else ImGui::TextDisabled("-");
                if (fonts.mono) ImGui::PopFont();

                ImGui::TableSetColumnIndex(4);
                if (fonts.mono) ImGui::PushFont(fonts.mono);
                ImGui::Text("%u/%u vs %u/%u",
                    item.side_a.present ? item.side_a.streaming_size : 0U,
                    item.side_a.present ? item.side_a.archive_size : 0U,
                    item.side_b.present ? item.side_b.streaming_size : 0U,
                    item.side_b.present ? item.side_b.archive_size : 0U);
                if (fonts.mono) ImGui::PopFont();

                ImGui::TableSetColumnIndex(5);
                ImGui::TextUnformatted(item.detail.c_str());
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_panel);
    ImGui::BeginChild("##CmpDetail", ImVec2(0.0f, 0.0f), true);
    if (selected_row_.has_value() && *selected_row_ < r.entries.size()) {
        const auto& sel = r.entries[*selected_row_];
        ImGui::TextUnformatted(app::trf("compare.detail_entry", {
            {"name", sel.display_name},
            {"status", localized_compare_diff(sel.kind)}
        }).c_str());
        if (fonts.mono) ImGui::PushFont(fonts.mono);
        ImGui::Text("SHA-256 A: %s", sel.side_a.present ? sel.side_a.sha256_hex.c_str() : app::tr_c("common.not_present"));
        ImGui::Text("SHA-256 B: %s", sel.side_b.present ? sel.side_b.sha256_hex.c_str() : app::tr_c("common.not_present"));
        if (fonts.mono) ImGui::PopFont();
        ImGui::TextDisabled("%s", sel.detail.c_str());
    } else {
        ImGui::TextDisabled("%s", app::tr_c("compare.detail_prompt"));
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace imgstudio::ui
