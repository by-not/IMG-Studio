#include "ui/BatchView.hpp"
#include "platform/FileDialogs.hpp"
#include "platform/Platform.hpp"
#include "ui/Icons.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <string>

namespace imgstudio::ui {

void BatchView::render(
    app::ArchiveController& controller,
    UiState& ui_state,
    void* native_hwnd,
    const FontBundle& fonts) {
    const auto& pal = theme_palette();
    const auto summary = processor_.snapshot();
    const bool running = summary.running;

    if (summary.finished && !last_finished_toast_sent_) {
        last_finished_toast_sent_ = true;
        ui_state.push_toast(
            "Batch job finished",
            "Succeeded: " + std::to_string(summary.succeeded) +
                ", Failed: " + std::to_string(summary.failed),
            summary.failed > 0U);
    }

    // Configuration strip
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_panel);
    ImGui::BeginChild("##BatchConfig", ImVec2(0.0f, 155.0f), true);
    {
        if (ImGui::Button((std::string(icons::kPlus) + " Add IMG Archives...").c_str()) && !running) {
            auto files = platform::open_files_dialog(
                native_hwnd, "Select IMG Archives For Batch Queue",
                {{"IMG Archives (*.img)", "*.img"}});
            for (auto& f : files) {
                queued_archives_.push_back(std::move(f));
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Open Tabs") && !running) {
            for (const auto& t : controller.tabs()) {
                queued_archives_.push_back(t->path());
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Clear Queue") && !running) {
            queued_archives_.clear();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Queued archives: %zu", queued_archives_.size());

        const char* ops[] = {
            "Validate Archives",
            "Create Manual Backups",
            "Export All Entries",
            "Import Files Into Archives",
            "Replace Matching Entries"
        };
        ImGui::SetNextItemWidth(230.0f);
        ImGui::Combo("Operation", &op_idx_, ops, 5);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::SliderInt("Workers", &worker_count_, 1, 4);

        const auto op = static_cast<app::BatchOperationKind>(op_idx_);
        if (op == app::BatchOperationKind::Validate) {
            ImGui::Checkbox("Verify full payload readability", &verify_payloads_);
        } else if (op == app::BatchOperationKind::ExportAll) {
            std::string exp_str = platform::to_utf8(export_dir_);
            ImGui::SetNextItemWidth(420.0f);
            if (ImGui::InputTextWithHint("##BatchExpDir", "Export root directory...", &exp_str)) {
                export_dir_ = platform::from_utf8(exp_str);
            }
            ImGui::SameLine();
            if (ImGui::Button("Browse Folder...") && !running) {
                if (auto d = platform::pick_folder_dialog(native_hwnd, "Select Batch Export Directory")) {
                    export_dir_ = *d;
                }
            }
        } else if (op == app::BatchOperationKind::ImportFiles ||
                   op == app::BatchOperationKind::ReplaceMatching) {
            if (ImGui::Button("Select Asset Files...") && !running) {
                input_files_ = platform::open_files_dialog(
                    native_hwnd, "Select Files For Batch Import/Replace",
                    {{"All Files (*.*)", "*.*"}});
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%zu input file(s) selected", input_files_.size());
            ImGui::SameLine();
            ImGui::Checkbox("Auto-backup before rebuild", &auto_backup_);
            if (op == app::BatchOperationKind::ImportFiles) {
                ImGui::SameLine();
                const char* dpol[] = {"Ask (fallback Replace)", "Replace Existing", "Skip Duplicates", "Auto-Rename"};
                ImGui::SetNextItemWidth(160.0f);
                ImGui::Combo("##BatchDup", &dup_policy_idx_, dpol, 4);
            }
        }

        const bool can_start = !running && !queued_archives_.empty() &&
            (op != app::BatchOperationKind::ExportAll || !export_dir_.empty()) &&
            ((op != app::BatchOperationKind::ImportFiles && op != app::BatchOperationKind::ReplaceMatching) ||
             !input_files_.empty());

        if (!can_start) ImGui::BeginDisabled();
        ImGui::PushStyleColor(ImGuiCol_Button, pal.accent);
        if (ImGui::Button("Start Batch Job", ImVec2(160.0f, 0.0f))) {
            app::BatchJobSpec spec;
            spec.operation = op;
            spec.archive_paths = queued_archives_;
            spec.output_directory = export_dir_;
            spec.input_files = input_files_;
            spec.backup_directory = controller.backup_manager().backup_directory();
            spec.create_auto_backup = auto_backup_;
            spec.verify_payload_readability = verify_payloads_;
            spec.duplicate_policy = static_cast<app::DuplicateImportPolicy>(dup_policy_idx_);
            spec.worker_threads = static_cast<std::size_t>(worker_count_);
            last_finished_toast_sent_ = false;
            processor_.start(std::move(spec));
        }
        ImGui::PopStyleColor();
        if (!can_start) ImGui::EndDisabled();

        if (running) {
            ImGui::SameLine();
            const float frac = summary.total > 0U
                ? static_cast<float>(summary.completed) / static_cast<float>(summary.total)
                : 0.0f;
            ImGui::ProgressBar(frac, ImVec2(200.0f, 0.0f));
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                processor_.cancel();
            }
        } else if (summary.finished) {
            ImGui::SameLine();
            if (ImGui::Button("Export Summary JSON")) {
                if (auto dest = platform::save_file_dialog(
                        native_hwnd, "Export Batch Summary JSON", "batch_summary.json",
                        {{"JSON Files (*.json)", "*.json"}})) {
                    auto er = app::BatchProcessor::export_summary_json(summary, *dest);
                    ui_state.push_toast(
                        er.ok() ? "Batch report exported" : "Export failed",
                        er.ok() ? platform::to_utf8(dest->filename()) : er.error->message,
                        !er.ok());
                }
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Queue / Results table
    const float table_h = std::max(160.0f, ImGui::GetContentRegionAvail().y);
    if (ImGui::BeginTable("##BatchTable", 4,
            ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_BordersInnerV |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp,
            ImVec2(0.0f, table_h))) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Status", ImGuiTableColumnFlags_WidthFixed, 105.0f);
        ImGui::TableSetupColumn("Archive Path", ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn("Summary", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupColumn("Error Detail", ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableHeadersRow();

        if (summary.items.empty()) {
            for (std::size_t i = 0; i < queued_archives_.size(); ++i) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("Queued");
                ImGui::TableSetColumnIndex(1);
                if (fonts.mono) ImGui::PushFont(fonts.mono);
                ImGui::TextUnformatted(platform::to_utf8(queued_archives_[i]).c_str());
                if (fonts.mono) ImGui::PopFont();
                ImGui::TableSetColumnIndex(2);
                ImGui::TextDisabled("Waiting to start");
                ImGui::TableSetColumnIndex(3);
                ImGui::TextDisabled("-");
            }
        } else {
            for (const auto& item : summary.items) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImVec4 col = pal.text_secondary;
                if (item.status == app::BatchItemStatus::Succeeded) col = pal.success;
                else if (item.status == app::BatchItemStatus::Failed) col = pal.danger;
                else if (item.status == app::BatchItemStatus::Running) col = pal.accent_hover;
                else if (item.status == app::BatchItemStatus::Canceled) col = pal.warning;
                ImGui::TextColored(col, "%s", app::batch_status_label(item.status));

                ImGui::TableSetColumnIndex(1);
                if (fonts.mono) ImGui::PushFont(fonts.mono);
                ImGui::TextUnformatted(platform::to_utf8(item.archive_path).c_str());
                if (fonts.mono) ImGui::PopFont();

                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(item.summary.c_str());

                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(item.error_detail.empty() ? "-" : item.error_detail.c_str());
            }
        }
        ImGui::EndTable();
    }
}

} // namespace imgstudio::ui
