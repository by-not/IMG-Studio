#include "app/BatchProcessor.hpp"
#include "app/ArchiveDocument.hpp"
#include "app/BackupManager.hpp"
#include "core/ImgFilename.hpp"
#include "platform/AtomicFileOps.hpp"
#include "platform/Platform.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <unordered_set>

namespace imgstudio::app {

const char* batch_operation_label(BatchOperationKind op) noexcept {
    switch (op) {
        case BatchOperationKind::Validate: return "Batch Validate";
        case BatchOperationKind::Backup: return "Batch Backup";
        case BatchOperationKind::ExportAll: return "Batch Export All";
        case BatchOperationKind::ImportFiles: return "Batch Import Files";
        case BatchOperationKind::ReplaceMatching: return "Batch Replace Matching";
    }
    return "Unknown";
}

const char* batch_status_label(BatchItemStatus st) noexcept {
    switch (st) {
        case BatchItemStatus::Pending: return "Pending";
        case BatchItemStatus::Running: return "Running";
        case BatchItemStatus::Succeeded: return "Succeeded";
        case BatchItemStatus::Failed: return "Failed";
        case BatchItemStatus::Canceled: return "Canceled";
    }
    return "Unknown";
}

namespace {

std::vector<std::filesystem::path> deduplicate_paths(const std::vector<std::filesystem::path>& input) {
    std::vector<std::filesystem::path> out;
    std::unordered_set<std::string> seen;
    std::error_code ec;
    for (const auto& p : input) {
        auto canon = std::filesystem::weakly_canonical(p, ec);
        const std::string key = core::to_ascii_lower(platform::to_utf8(ec ? p : canon));
        if (seen.insert(key).second) {
            out.push_back(p);
        }
    }
    return out;
}

BatchItemResult execute_single_item(
    const BatchJobSpec& spec,
    const std::filesystem::path& archive_path,
    std::mutex& backup_index_mutex,
    const std::atomic<bool>* cancel_flag) {
    BatchItemResult res;
    res.archive_path = archive_path;

    if (cancel_flag && cancel_flag->load(std::memory_order_relaxed)) {
        res.status = BatchItemStatus::Canceled;
        res.summary = "Canceled before execution.";
        return res;
    }

    switch (spec.operation) {
        case BatchOperationKind::Validate: {
            core::ValidationOptions vopts;
            vopts.verify_payload_readability = spec.verify_payload_readability;
            vopts.cancel_flag = cancel_flag;
            auto doc_res = ArchiveDocument::open(archive_path, vopts);
            if (!doc_res.ok()) {
                res.status = BatchItemStatus::Failed;
                res.summary = doc_res.error->message;
                res.error_detail = doc_res.error->detail;
                return res;
            }
            const auto& rep = doc_res.value->validation_report();
            if (rep.has_errors()) {
                res.status = BatchItemStatus::Failed;
                res.summary = "Errors: " + std::to_string(rep.error_count) +
                              ", Warnings: " + std::to_string(rep.warning_count);
                for (const auto& d : rep.diagnostics) {
                    if (d.severity == core::DiagnosticSeverity::Error) {
                        res.error_detail = d.message;
                        break;
                    }
                }
            } else {
                res.status = BatchItemStatus::Succeeded;
                res.summary = "Valid (" + std::to_string(doc_res.value->entries().size()) +
                              " entries, " + std::to_string(rep.warning_count) + " warnings)";
            }
            return res;
        }

        case BatchOperationKind::Backup: {
            std::lock_guard lock(backup_index_mutex);
            BackupManager bm(spec.backup_directory);
            auto bkp = bm.create_backup(archive_path, BackupKind::Manual, "Batch backup");
            if (!bkp.ok()) {
                res.status = BatchItemStatus::Failed;
                res.summary = bkp.error->message;
                res.error_detail = bkp.error->detail;
                return res;
            }
            res.status = BatchItemStatus::Succeeded;
            res.backup_id = bkp.value->id;
            res.summary = "Backup created: " + bkp.value->id;
            return res;
        }

        case BatchOperationKind::ExportAll: {
            auto doc_res = ArchiveDocument::open(archive_path);
            if (!doc_res.ok()) {
                res.status = BatchItemStatus::Failed;
                res.summary = doc_res.error->message;
                res.error_detail = doc_res.error->detail;
                return res;
            }
            const std::string folder_name = archive_path.stem().string().empty()
                ? std::string("archive")
                : archive_path.stem().string();
            const std::filesystem::path sub_dir = spec.output_directory / folder_name;
            std::error_code ec;
            std::filesystem::create_directories(sub_dir, ec);
            if (ec) {
                res.status = BatchItemStatus::Failed;
                res.summary = "Failed to create export subfolder.";
                res.error_detail = ec.message();
                return res;
            }
            core::ExtractionOptions eopts;
            eopts.overwrite_existing = true;
            eopts.cancel_flag = cancel_flag;
            auto ex = doc_res.value->export_all(sub_dir, eopts);
            if (!ex.ok()) {
                res.status = BatchItemStatus::Failed;
                res.summary = ex.error->message;
                res.error_detail = ex.error->detail;
                return res;
            }
            if (ex.value->canceled) {
                res.status = BatchItemStatus::Canceled;
                res.summary = "Export canceled after " + std::to_string(ex.value->succeeded) + " entry(s).";
                return res;
            }
            if (!ex.value->failures.empty()) {
                res.status = BatchItemStatus::Failed;
                res.summary = "Exported " + std::to_string(ex.value->succeeded) + ", failed " +
                              std::to_string(ex.value->failures.size());
                res.error_detail = ex.value->failures[0].entry_name + ": " + ex.value->failures[0].reason;
                return res;
            }
            res.status = BatchItemStatus::Succeeded;
            res.summary = "Exported " + std::to_string(ex.value->succeeded) + " entry(s) to " + folder_name;
            return res;
        }

        case BatchOperationKind::ImportFiles:
        case BatchOperationKind::ReplaceMatching: {
            if (spec.input_files.empty()) {
                res.status = BatchItemStatus::Failed;
                res.summary = "No input files specified for batch modification.";
                return res;
            }
            auto doc_res = ArchiveDocument::open(archive_path);
            if (!doc_res.ok()) {
                res.status = BatchItemStatus::Failed;
                res.summary = doc_res.error->message;
                res.error_detail = doc_res.error->detail;
                return res;
            }
            auto& doc = *doc_res.value;
            if (doc.validation_report().has_errors()) {
                res.status = BatchItemStatus::Failed;
                res.summary = "Skipped rebuild: archive has structural validation errors.";
                return res;
            }

            std::size_t modified_count = 0;
            if (spec.operation == BatchOperationKind::ImportFiles) {
                const auto pol = (spec.duplicate_policy == DuplicateImportPolicy::Ask)
                    ? DuplicateImportPolicy::Replace
                    : spec.duplicate_policy;
                auto imp = doc.stage_import(spec.input_files, pol);
                if (!imp.ok()) {
                    res.status = BatchItemStatus::Failed;
                    res.summary = imp.error->message;
                    return res;
                }
                if (!imp.value->rejected.empty()) {
                    res.status = BatchItemStatus::Failed;
                    res.summary = "Input file rejected: " + imp.value->rejected[0].reason;
                    res.error_detail = platform::to_utf8(imp.value->rejected[0].source_path);
                    return res;
                }
                modified_count = imp.value->imported_count + imp.value->replaced_count + imp.value->renamed_count;
            } else {
                for (const auto& f : spec.input_files) {
                    const std::string lower = core::to_ascii_lower(platform::to_utf8(f.filename()));
                    for (const auto& e : doc.entries()) {
                        if (core::to_ascii_lower(e.name) == lower) {
                            auto rep = doc.stage_replace(e.id, f);
                            if (!rep.ok()) {
                                res.status = BatchItemStatus::Failed;
                                res.summary = rep.error->message;
                                return res;
                            }
                            ++modified_count;
                        }
                    }
                }
            }

            if (modified_count == 0U) {
                res.status = BatchItemStatus::Succeeded;
                res.summary = "No matching changes needed (archive untouched).";
                return res;
            }

            std::optional<BackupRecord> bkp_rec;
            if (spec.create_auto_backup) {
                std::lock_guard lock(backup_index_mutex);
                BackupManager bm(spec.backup_directory);
                auto bkp = bm.create_backup(archive_path, BackupKind::Automatic, "Batch pre-rebuild backup");
                if (!bkp.ok()) {
                    res.status = BatchItemStatus::Failed;
                    res.summary = "Pre-rebuild backup failed; original preserved.";
                    res.error_detail = bkp.error->message;
                    return res;
                }
                bkp_rec = std::move(*bkp.value);
                res.backup_id = bkp_rec->id;
            }

            core::BuildOptions bopts;
            bopts.cancel_flag = cancel_flag;
            auto commit = doc.commit_rebuild(nullptr, false, bopts);
            if (!commit.ok()) {
                res.status = (cancel_flag && cancel_flag->load(std::memory_order_relaxed))
                    ? BatchItemStatus::Canceled
                    : BatchItemStatus::Failed;
                res.summary = commit.error->message;
                res.error_detail = commit.error->detail;
                return res;
            }

            res.status = BatchItemStatus::Succeeded;
            res.summary = "Rebuilt (" + std::to_string(modified_count) + " change(s))";
            return res;
        }
    }

    res.status = BatchItemStatus::Failed;
    res.summary = "Unsupported batch operation.";
    return res;
}

} // namespace

BatchProcessor::~BatchProcessor() {
    cancel();
    join_coordinator();
}

void BatchProcessor::join_coordinator() {
    if (coordinator_.joinable()) {
        coordinator_.join();
    }
}

BatchSummary BatchProcessor::snapshot() const {
    std::lock_guard lock(mutex_);
    return state_;
}

void BatchProcessor::cancel() {
    cancel_flag_.store(true, std::memory_order_relaxed);
}

BatchSummary BatchProcessor::run_sync(const BatchJobSpec& spec, const std::atomic<bool>* cancel_flag) {
    const auto unique_archives = deduplicate_paths(spec.archive_paths);
    BatchSummary summary;
    summary.operation = spec.operation;
    summary.total = unique_archives.size();
    summary.items.resize(unique_archives.size());
    for (std::size_t i = 0; i < unique_archives.size(); ++i) {
        summary.items[i].archive_path = unique_archives[i];
    }

    std::atomic<std::size_t> next_index{0};
    std::mutex backup_mutex;
    std::mutex results_mutex;

    const std::size_t nthreads = std::clamp<std::size_t>(
        spec.worker_threads, 1U, std::max<std::size_t>(1U, std::min<std::size_t>(4U, unique_archives.size())));

    std::vector<std::thread> workers;
    workers.reserve(nthreads);
    for (std::size_t w = 0; w < nthreads; ++w) {
        workers.emplace_back([&]() {
            while (true) {
                const std::size_t idx = next_index.fetch_add(1U, std::memory_order_relaxed);
                if (idx >= unique_archives.size()) {
                    break;
                }
                auto item_res = execute_single_item(spec, unique_archives[idx], backup_mutex, cancel_flag);
                std::lock_guard lock(results_mutex);
                summary.items[idx] = std::move(item_res);
                ++summary.completed;
                if (summary.items[idx].status == BatchItemStatus::Succeeded) ++summary.succeeded;
                else if (summary.items[idx].status == BatchItemStatus::Failed) ++summary.failed;
                else if (summary.items[idx].status == BatchItemStatus::Canceled) ++summary.canceled_count;
            }
        });
    }
    for (auto& t : workers) {
        t.join();
    }

    summary.canceled = (cancel_flag && cancel_flag->load(std::memory_order_relaxed));
    summary.finished = true;
    return summary;
}

void BatchProcessor::start(BatchJobSpec spec) {
    if (running_.exchange(true)) {
        return;
    }
    join_coordinator();
    cancel_flag_.store(false, std::memory_order_relaxed);

    const auto unique_archives = deduplicate_paths(spec.archive_paths);
    {
        std::lock_guard lock(mutex_);
        state_ = BatchSummary{};
        state_.running = true;
        state_.operation = spec.operation;
        state_.total = unique_archives.size();
        state_.items.resize(unique_archives.size());
        for (std::size_t i = 0; i < unique_archives.size(); ++i) {
            state_.items[i].archive_path = unique_archives[i];
            state_.items[i].status = BatchItemStatus::Pending;
        }
    }

    coordinator_ = std::thread([this, spec, unique_archives]() {
        std::atomic<std::size_t> next_index{0};
        std::mutex backup_mutex;

        const std::size_t nthreads = std::clamp<std::size_t>(
            spec.worker_threads, 1U, std::max<std::size_t>(1U, std::min<std::size_t>(4U, unique_archives.size())));

        std::vector<std::thread> workers;
        workers.reserve(nthreads);
        for (std::size_t w = 0; w < nthreads; ++w) {
            workers.emplace_back([&]() {
                while (true) {
                    const std::size_t idx = next_index.fetch_add(1U, std::memory_order_relaxed);
                    if (idx >= unique_archives.size()) {
                        break;
                    }
                    {
                        std::lock_guard lock(mutex_);
                        state_.items[idx].status = BatchItemStatus::Running;
                    }
                    auto item_res = execute_single_item(spec, unique_archives[idx], backup_mutex, &cancel_flag_);
                    {
                        std::lock_guard lock(mutex_);
                        state_.items[idx] = std::move(item_res);
                        ++state_.completed;
                        if (state_.items[idx].status == BatchItemStatus::Succeeded) ++state_.succeeded;
                        else if (state_.items[idx].status == BatchItemStatus::Failed) ++state_.failed;
                        else if (state_.items[idx].status == BatchItemStatus::Canceled) ++state_.canceled_count;
                    }
                }
            });
        }
        for (auto& t : workers) {
            t.join();
        }

        {
            std::lock_guard lock(mutex_);
            state_.running = false;
            state_.finished = true;
            state_.canceled = cancel_flag_.load(std::memory_order_relaxed);
        }
        running_.store(false, std::memory_order_relaxed);
    });
}

core::Result<void> BatchProcessor::export_summary_json(
    const BatchSummary& summary,
    const std::filesystem::path& dest_file) {
    nlohmann::json j;
    j["operation"] = batch_operation_label(summary.operation);
    j["total"] = summary.total;
    j["succeeded"] = summary.succeeded;
    j["failed"] = summary.failed;
    j["canceled"] = summary.canceled_count;

    nlohmann::json items = nlohmann::json::array();
    for (const auto& it : summary.items) {
        items.push_back({
            {"archive", platform::to_utf8(it.archive_path)},
            {"status", batch_status_label(it.status)},
            {"summary", it.summary},
            {"error_detail", it.error_detail},
            {"backup_id", it.backup_id}
        });
    }
    j["items"] = std::move(items);

    const auto tmp = platform::make_unique_temp_path(dest_file, ".json.tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return core::Result<void>::failure("Failed to open batch report output file.");
        }
        out << j.dump(2) << '\n';
        out.flush();
        if (!out) {
            out.close();
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return core::Result<void>::failure("Failed to write batch summary JSON.");
        }
    }
    return platform::atomic_replace_file(tmp, dest_file);
}

} // namespace imgstudio::app
