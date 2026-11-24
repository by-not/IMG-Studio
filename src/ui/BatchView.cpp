#include "ui/BatchView.hpp"
#include "app/Localization.hpp"
#include "platform/FileDialogs.hpp"
#include "platform/Platform.hpp"
#include "ui/Icons.hpp"

#include <imgui.h>
#include <misc/cpp/imgui_stdlib.h>

#include <algorithm>
#include <string>

namespace imgstudio::ui {

namespace {

const char* localized_batch_status(app::BatchItemStatus st) {
    switch (st) {
        case app::BatchItemStatus::Pending: return app::tr_c("batch.status.pending");
        case app::BatchItemStatus::Running: return app::tr_c("batch.status.running");
        case app::BatchItemStatus::Succeeded: return app::tr_c("batch.status.succeeded");
        case app::BatchItemStatus::Failed: return app::tr_c("batch.status.failed");
        case app::BatchItemStatus::Canceled: return app::tr_c("batch.status.canceled");
    }
    return app::tr_c("batch.status.pending");
}

} // namespace

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
            app::tr("toast.batch_done"),
            app::trf("toast.batch_done_detail", {
                {"ok", std::to_string(summary.succeeded)},
                {"fail", std::to_string(summary.failed)}
            }),
            summary.failed > 0U);
    }

    // Configuration strip
    ImGui::PushStyleColor(ImGuiCol_ChildBg, pal.bg_panel);
    ImGui::BeginChild("##BatchConfig", ImVec2(0.0f, 155.0f), true);
    {
        if (ImGui::Button((std::string(icons::kPlus) + " " + app::tr("batch.add_archives") + "###BatchAdd").c_str()) && !running) {
            auto files = platform::open_files_dialog(
                native_hwnd, app::tr("batch.add_dialog"),
                {{"IMG Archives (*.img)", "*.img"}});
            for (auto& f : files) {
                queued_archives_.push_back(std::move(f));
            }
        }
        ImGui::SameLine();
        if (ImGui::Button((app::tr("batch.add_open_tabs") + "###BatchTabs").c_str()) && !running) {
            for (const auto& t : controller.tabs()) {
                queued_archives_.push_back(t->path());
            }
        }
        ImGui::SameLine();
        if (ImGui::Button((app::tr("batch.clear_queue") + "###BatchClear").c_str()) && !running) {
            queued_archives_.clear();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", app::trf("batch.queued_count", {
            {"count", std::to_string(queued_archives_.size())}
        }).c_str());

        const char* ops[] = {
            app::tr_c("batch.op.validate"),
            app::tr_c("batch.op.backup"),
            app::tr_c("batch.op.export_all"),
            app::tr_c("batch.op.import_files"),
            app::tr_c("batch.op.replace_matching")
        };
        ImGui::SetNextItemWidth(230.0f);
        ImGui::Combo((app::tr("batch.operation") + "###BatchOp").c_str(), &op_idx_, ops, 5);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::SliderInt((app::tr("batch.workers") + "###BatchWorkers").c_str(), &worker_count_, 1, 4);

        const auto op = static_cast<app::BatchOperationKind>(op_idx_);
        if (op == app::BatchOperationKind::Validate) {
            ImGui::Checkbox((app::tr("batch.verify_reads") + "###BatchVerify").c_str(), &verify_payloads_);
        } else if (op == app::BatchOperationKind::ExportAll) {
            std::string exp_str = platform::to_utf8(export_dir_);
            ImGui::SetNextItemWidth(420.0f);
            if (ImGui::InputTextWithHint("##BatchExpDir", app::tr_c("batch.export_dir_hint"), &exp_str)) {
                export_dir_ = platform::from_utf8(exp_str);
            }
            ImGui::SameLine();
            if (ImGui::Button((app::tr("batch.browse_folder") + "###BatchBrowseExp").c_str()) && !running) {
                if (auto d = platform::pick_folder_dialog(native_hwnd, app::tr("batch.browse_folder_title"))) {
                    export_dir_ = *d;
                }
            }
        } else if (op == app::BatchOperationKind::ImportFiles ||
                   op == app::BatchOperationKind::ReplaceMatching) {
            if (ImGui::Button((app::tr("batch.select_inputs") + "###BatchSelInputs").c_str()) && !running) {
                input_files_ = platform::open_files_dialog(
                    native_hwnd, app::tr("batch.select_inputs_title"),
                    {{"All Files (*.*)", "*.*"}});
            }
            ImGui::SameLine();
            ImGui::TextDisabled("%s", app::trf("batch.inputs_selected", {
                {"count", std::to_string(input_files_.size())}
            }).c_str());
            ImGui::SameLine();
            ImGui::Checkbox((app::tr("batch.auto_backup") + "###BatchAutoBkp").c_str(), &auto_backup_);
            if (op == app::BatchOperationKind::ImportFiles) {
                ImGui::SameLine();
                const char* dpol[] = {
                    app::tr_c("batch.dup.ask_fallback"),
                    app::tr_c("batch.dup.replace"),
                    app::tr_c("batch.dup.skip"),
                    app::tr_c("batch.dup.autorename")
                };
                ImGui::SetNextItemWidth(175.0f);
                ImGui::Combo("##BatchDup", &dup_policy_idx_, dpol, 4);
            }
        }

        const bool can_start = !running && !queued_archives_.empty() &&
            (op != app::BatchOperationKind::ExportAll || !export_dir_.empty()) &&
            ((op != app::BatchOperationKind::ImportFiles && op != app::BatchOperationKind::ReplaceMatching) ||
             !input_files_.empty());

        if (!can_start) ImGui::BeginDisabled();
        ImGui::PushStyleColor(ImGuiCol_Button, pal.accent);
        if (ImGui::Button((app::tr("batch.start") + "###BatchStart").c_str(), ImVec2(175.0f, 0.0f))) {
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
            if (ImGui::Button(app::tr_c("common.cancel"))) {
                processor_.cancel();
            }
        } else if (summary.finished) {
            ImGui::SameLine();
            if (ImGui::Button((app::tr("batch.export_json") + "###BatchExpJson").c_str())) {
                if (auto dest = platform::save_file_dialog(
                        native_hwnd, app::tr("batch.export_json_title"), "batch_summary.json",
                        {{"JSON Files (*.json)", "*.json"}})) {
                    auto er = app::BatchProcessor::export_summary_json(summary, *dest);
                    ui_state.push_toast(
                        er.ok() ? app::tr("toast.batch_export_ok") : app::tr("toast.cmp_export_failed"),
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
        ImGui::TableSetupColumn((app::tr("batch.col.status") + "###BatchColStatus").c_str(), ImGuiTableColumnFlags_WidthFixed, 105.0f);
        ImGui::TableSetupColumn((app::tr("batch.col.archive") + "###BatchColArchive").c_str(), ImGuiTableColumnFlags_WidthStretch, 2.0f);
        ImGui::TableSetupColumn((app::tr("batch.col.summary") + "###BatchColSummary").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableSetupColumn((app::tr("batch.col.error") + "###BatchColError").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.5f);
        ImGui::TableHeadersRow();

        if (summary.items.empty()) {
            for (std::size_t i = 0; i < queued_archives_.size(); ++i) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%s", app::tr_c("batch.queued"));
                ImGui::TableSetColumnIndex(1);
                if (fonts.mono) ImGui::PushFont(fonts.mono);
                ImGui::TextUnformatted(platform::to_utf8(queued_archives_[i]).c_str());
                if (fonts.mono) ImGui::PopFont();
                ImGui::TableSetColumnIndex(2);
                ImGui::TextDisabled("%s", app::tr_c("batch.waiting"));
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
                ImGui::TextColored(col, "%s", localized_batch_status(item.status));

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
