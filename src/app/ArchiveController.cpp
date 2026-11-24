#include "app/ArchiveController.hpp"
#include "platform/AtomicFileOps.hpp"
#include "platform/Platform.hpp"

#include <algorithm>

namespace imgstudio::app {

ArchiveController::ArchiveController()
    : settings_path_(platform::get_system_paths().settings_file)
    , backup_manager_(platform::get_system_paths().default_backup_dir) {
    settings_ = AppSettingsStore::load_or_default(settings_path_);
    if (!settings_.backup_directory.empty()) {
        backup_manager_.set_backup_directory(settings_.backup_directory);
    }
}

ArchiveController::~ArchiveController() {
    cancel_current_task();
    join_worker();
}

void ArchiveController::join_worker() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

void ArchiveController::save_settings() const {
    (void)AppSettingsStore::save(settings_, settings_path_);
}

ArchiveDocument* ArchiveController::active_document() noexcept {
    if (!active_tab_.has_value() || *active_tab_ >= tabs_.size()) {
        return nullptr;
    }
    return tabs_[*active_tab_].get();
}

const ArchiveDocument* ArchiveController::active_document() const noexcept {
    if (!active_tab_.has_value() || *active_tab_ >= tabs_.size()) {
        return nullptr;
    }
    return tabs_[*active_tab_].get();
}

void ArchiveController::set_active_tab(std::size_t index) {
    if (index < tabs_.size()) {
        active_tab_ = index;
    }
}

bool ArchiveController::close_tab(std::size_t index) {
    if (index >= tabs_.size()) {
        return false;
    }
    tabs_.erase(tabs_.begin() + static_cast<std::ptrdiff_t>(index));
    if (tabs_.empty()) {
        active_tab_ = std::nullopt;
    } else if (active_tab_.has_value()) {
        if (*active_tab_ >= tabs_.size()) {
            active_tab_ = tabs_.size() - 1U;
        }
    }
    return true;
}

bool ArchiveController::any_unsaved_changes() const noexcept {
    for (const auto& t : tabs_) {
        if (t && t->has_pending_changes()) {
            return true;
        }
    }
    return false;
}

bool ArchiveController::is_busy() const noexcept {
    return busy_.load(std::memory_order_relaxed);
}

AsyncTaskProgress ArchiveController::progress_snapshot() const {
    std::lock_guard lock(mutex_);
    return progress_;
}

void ArchiveController::cancel_current_task() {
    cancel_flag_.store(true, std::memory_order_relaxed);
}

void ArchiveController::update_progress(
    std::string label,
    std::string detail,
    std::size_t current,
    std::size_t total) {
    std::lock_guard lock(mutex_);
    progress_.running = true;
    progress_.label = std::move(label);
    progress_.detail = std::move(detail);
    progress_.current = current;
    progress_.total = total;
}

void ArchiveController::push_completion(AsyncCompletion comp) {
    std::lock_guard lock(mutex_);
    progress_.running = false;
    completed_queue_.push_back(std::move(comp));
    busy_.store(false, std::memory_order_relaxed);
}

std::vector<AsyncCompletion> ArchiveController::poll_completions() {
    std::vector<AsyncCompletion> local;
    {
        std::lock_guard lock(mutex_);
        local.swap(completed_queue_);
    }

    for (auto& comp : local) {
        if (!comp.ok) {
            continue;
        }
        if ((comp.kind == AsyncCompletion::Kind::OpenedArchive ||
             comp.kind == AsyncCompletion::Kind::CreatedArchive) &&
            comp.opened_doc.has_value()) {
            const std::string new_utf8 = platform::to_utf8(comp.opened_doc->path());
            bool found_existing = false;
            for (std::size_t i = 0; i < tabs_.size(); ++i) {
                if (platform::to_utf8(tabs_[i]->path()) == new_utf8) {
                    tabs_[i] = std::make_unique<ArchiveDocument>(std::move(*comp.opened_doc));
                    active_tab_ = i;
                    found_existing = true;
                    break;
                }
            }
            if (!found_existing) {
                tabs_.push_back(std::make_unique<ArchiveDocument>(std::move(*comp.opened_doc)));
                active_tab_ = tabs_.size() - 1U;
            }
            settings_.add_recent_file(tabs_[*active_tab_]->path());
            save_settings();
        } else if (comp.kind == AsyncCompletion::Kind::CommittedRebuild &&
                   comp.opened_doc.has_value() &&
                   comp.tab_index.has_value() &&
                   *comp.tab_index < tabs_.size()) {
            tabs_[*comp.tab_index] = std::make_unique<ArchiveDocument>(std::move(*comp.opened_doc));
        } else if (comp.kind == AsyncCompletion::Kind::ValidatedArchive &&
                   comp.validation_report.has_value() &&
                   comp.tab_index.has_value() &&
                   *comp.tab_index < tabs_.size()) {
            tabs_[*comp.tab_index]->set_validation_report(*comp.validation_report);
        } else if (comp.kind == AsyncCompletion::Kind::RestoredBackup &&
                   comp.opened_doc.has_value() &&
                   comp.tab_index.has_value() &&
                   *comp.tab_index < tabs_.size()) {
            tabs_[*comp.tab_index] = std::make_unique<ArchiveDocument>(std::move(*comp.opened_doc));
        }
    }

    return local;
}

core::Result<void> ArchiveController::open_archive_sync(const std::filesystem::path& path) {
    core::ValidationOptions val_opts;
    val_opts.verify_payload_readability = settings_.verify_payloads_on_open;
    auto res = ArchiveDocument::open(path, val_opts);
    if (!res.ok()) {
        return core::Result<void>::failure(res.error->message, res.error->detail);
    }

    const std::string new_utf8 = platform::to_utf8(res.value->path());
    for (std::size_t i = 0; i < tabs_.size(); ++i) {
        if (platform::to_utf8(tabs_[i]->path()) == new_utf8) {
            tabs_[i] = std::make_unique<ArchiveDocument>(std::move(*res.value));
            active_tab_ = i;
            return core::Result<void>::success();
        }
    }
    tabs_.push_back(std::make_unique<ArchiveDocument>(std::move(*res.value)));
    active_tab_ = tabs_.size() - 1U;
    settings_.add_recent_file(tabs_[*active_tab_]->path());
    save_settings();
    return core::Result<void>::success();
}

void ArchiveController::start_open_archive(const std::filesystem::path& path) {
    if (busy_.exchange(true)) {
        return;
    }
    join_worker();
    cancel_flag_.store(false, std::memory_order_relaxed);
    const bool verify_payloads = settings_.verify_payloads_on_open;

    worker_ = std::thread([this, path, verify_payloads]() {
        update_progress("Opening archive", platform::to_utf8(path.filename()), 0, 0);
        core::ValidationOptions val_opts;
        val_opts.verify_payload_readability = verify_payloads;
        val_opts.cancel_flag = &cancel_flag_;
        val_opts.progress_cb = [this](std::size_t cur, std::size_t tot) {
            update_progress("Verifying payload readability", "Checking entry sectors", cur, tot);
        };

        auto res = ArchiveDocument::open(path, val_opts);
        AsyncCompletion comp;
        comp.kind = AsyncCompletion::Kind::OpenedArchive;
        if (res.ok()) {
            comp.ok = true;
            comp.message = "Opened " + platform::to_utf8(path.filename());
            comp.opened_doc = std::move(*res.value);
        } else {
            comp.ok = false;
            comp.message = res.error->message;
            comp.detail = res.error->detail;
        }
        push_completion(std::move(comp));
    });
}

void ArchiveController::start_create_archive(const std::filesystem::path& path) {
    if (busy_.exchange(true)) {
        return;
    }
    join_worker();
    cancel_flag_.store(false, std::memory_order_relaxed);

    worker_ = std::thread([this, path]() {
        update_progress("Creating archive", platform::to_utf8(path.filename()), 0, 1);
        auto res = ArchiveDocument::create_new(path);
        AsyncCompletion comp;
        comp.kind = AsyncCompletion::Kind::CreatedArchive;
        if (res.ok()) {
            comp.ok = true;
            comp.message = "Created empty archive " + platform::to_utf8(path.filename());
            comp.opened_doc = std::move(*res.value);
        } else {
            comp.ok = false;
            comp.message = res.error->message;
            comp.detail = res.error->detail;
        }
        push_completion(std::move(comp));
    });
}

void ArchiveController::start_commit_active(bool create_auto_backup) {
    if (!active_tab_.has_value() || *active_tab_ >= tabs_.size()) {
        return;
    }
    if (busy_.exchange(true)) {
        return;
    }
    join_worker();
    cancel_flag_.store(false, std::memory_order_relaxed);

    const std::size_t tab_idx = *active_tab_;
    const auto plan_res = tabs_[tab_idx]->make_rebuild_plan();
    const auto target_path = tabs_[tab_idx]->path();
    const std::size_t pending_cnt = tabs_[tab_idx]->pending_change_count();
    const auto backup_dir = backup_manager_.backup_directory();

    worker_ = std::thread([this, tab_idx, plan_res, target_path, pending_cnt, backup_dir, create_auto_backup]() {
        AsyncCompletion comp;
        comp.kind = AsyncCompletion::Kind::CommittedRebuild;
        comp.tab_index = tab_idx;

        if (!plan_res.ok()) {
            comp.ok = false;
            comp.message = plan_res.error->message;
            comp.detail = plan_res.error->detail;
            push_completion(std::move(comp));
            return;
        }

        CommitResult c_res;
        c_res.target_path = target_path;

        if (create_auto_backup) {
            update_progress("Creating pre-commit backup", platform::to_utf8(target_path.filename()), 0, 1);
            BackupManager bm(backup_dir);
            auto bkp = bm.create_backup(
                target_path,
                BackupKind::Automatic,
                "Automatic pre-commit backup (" + std::to_string(pending_cnt) + " change(s))");
            if (!bkp.ok()) {
                comp.ok = false;
                comp.message = "Automatic backup failed; rebuild aborted.";
                comp.detail = bkp.error->message;
                push_completion(std::move(comp));
                return;
            }
            c_res.auto_backup = std::move(*bkp.value);
        }

        const auto temp_path = platform::make_unique_temp_path(target_path, ".rebuild.tmp");
        core::BuildOptions opts;
        opts.cancel_flag = &cancel_flag_;
        opts.progress_cb = [this](std::size_t cur, std::size_t tot, std::string_view name) {
            update_progress("Rebuilding IMG archive", std::string(name), cur, tot);
        };

        auto bres = core::ImgBuilder::build_to_file(*plan_res.value, temp_path, opts);
        if (!bres.ok()) {
            comp.ok = false;
            comp.message = bres.error->message;
            comp.detail = bres.error->detail;
            push_completion(std::move(comp));
            return;
        }

        update_progress("Committing rebuilt archive", platform::to_utf8(target_path.filename()), 1, 1);
        auto rep = platform::atomic_replace_file(temp_path, target_path);
        if (!rep.ok()) {
            std::error_code ec;
            std::filesystem::remove(temp_path, ec);
            comp.ok = false;
            comp.message = "Failed to replace target archive (original preserved).";
            comp.detail = rep.error->message + (rep.error->detail.empty() ? "" : " (" + rep.error->detail + ")");
            push_completion(std::move(comp));
            return;
        }

        c_res.build_result = std::move(*bres.value);
        c_res.build_result.output_path = target_path;

        auto reopened = ArchiveDocument::open(target_path);
        if (!reopened.ok()) {
            comp.ok = false;
            comp.message = "Rebuild succeeded, but reloading archive failed.";
            comp.detail = reopened.error->message;
            push_completion(std::move(comp));
            return;
        }

        comp.ok = true;
        comp.message = "Rebuilt and verified " + platform::to_utf8(target_path.filename());
        comp.commit_result = std::move(c_res);
        comp.opened_doc = std::move(*reopened.value);
        push_completion(std::move(comp));
    });
}

void ArchiveController::start_export_active(
    std::vector<std::uint64_t> entry_ids,
    bool export_all,
    const std::filesystem::path& target_dir) {
    if (!active_tab_.has_value() || *active_tab_ >= tabs_.size()) {
        return;
    }
    if (busy_.exchange(true)) {
        return;
    }
    join_worker();
    cancel_flag_.store(false, std::memory_order_relaxed);

    const auto& doc = *tabs_[*active_tab_];
    const auto archive_path = doc.path();
    std::vector<core::ExtractionItem> items;
    if (export_all) {
        items.reserve(doc.entries().size());
        for (const auto& e : doc.entries()) {
            if (e.change_state == EntryChangeState::Deleted) continue;
            items.push_back(core::ExtractionItem{
                e.name, e.uses_external_file, e.external_path,
                e.original_offset_bytes,
                e.uses_external_file ? e.external_size_bytes : e.original_effective_bytes
            });
        }
    } else {
        for (const auto& e : doc.entries()) {
            if (e.change_state == EntryChangeState::Deleted) continue;
            if (std::find(entry_ids.begin(), entry_ids.end(), e.id) != entry_ids.end()) {
                items.push_back(core::ExtractionItem{
                    e.name, e.uses_external_file, e.external_path,
                    e.original_offset_bytes,
                    e.uses_external_file ? e.external_size_bytes : e.original_effective_bytes
                });
            }
        }
    }

    worker_ = std::thread([this, archive_path, items = std::move(items), target_dir]() {
        core::ExtractionOptions opts;
        opts.overwrite_existing = true;
        opts.cancel_flag = &cancel_flag_;
        opts.progress_cb = [this](std::size_t cur, std::size_t tot, std::string_view name) {
            update_progress("Exporting entries", std::string(name), cur, tot);
        };

        auto res = core::ImgExtractor::extract_items(archive_path, items, target_dir, opts);
        AsyncCompletion comp;
        comp.kind = AsyncCompletion::Kind::ExportedEntries;
        if (res.ok()) {
            comp.ok = true;
            comp.extraction_summary = std::move(*res.value);
            comp.message = "Exported " + std::to_string(comp.extraction_summary->succeeded) +
                           " of " + std::to_string(comp.extraction_summary->requested) + " entry(s)";
        } else {
            comp.ok = false;
            comp.message = res.error->message;
            comp.detail = res.error->detail;
        }
        push_completion(std::move(comp));
    });
}

void ArchiveController::start_validate_active(bool verify_payload_readability) {
    if (!active_tab_.has_value() || *active_tab_ >= tabs_.size()) {
        return;
    }
    if (busy_.exchange(true)) {
        return;
    }
    join_worker();
    cancel_flag_.store(false, std::memory_order_relaxed);

    const std::size_t tab_idx = *active_tab_;
    const auto archive_path = tabs_[tab_idx]->path();

    worker_ = std::thread([this, tab_idx, archive_path, verify_payload_readability]() {
        update_progress("Validating archive", platform::to_utf8(archive_path.filename()), 0, 0);
        core::ValidationOptions val_opts;
        val_opts.verify_payload_readability = verify_payload_readability;
        val_opts.cancel_flag = &cancel_flag_;
        val_opts.progress_cb = [this](std::size_t cur, std::size_t tot) {
            update_progress("Validating entry payloads", "Reading sectors", cur, tot);
        };

        auto res = ArchiveDocument::open(archive_path, val_opts);
        AsyncCompletion comp;
        comp.kind = AsyncCompletion::Kind::ValidatedArchive;
        comp.tab_index = tab_idx;
        if (res.ok()) {
            comp.ok = true;
            comp.validation_report = res.value->validation_report();
            comp.message = "Validation finished (" +
                           std::to_string(comp.validation_report->error_count) + " errors, " +
                           std::to_string(comp.validation_report->warning_count) + " warnings)";
        } else {
            comp.ok = false;
            comp.message = res.error->message;
            comp.detail = res.error->detail;
        }
        push_completion(std::move(comp));
    });
}

void ArchiveController::start_manual_backup_active(const std::string& reason) {
    if (!active_tab_.has_value() || *active_tab_ >= tabs_.size()) {
        return;
    }
    if (busy_.exchange(true)) {
        return;
    }
    join_worker();
    cancel_flag_.store(false, std::memory_order_relaxed);

    const auto archive_path = tabs_[*active_tab_]->path();
    const auto backup_dir = backup_manager_.backup_directory();

    worker_ = std::thread([this, archive_path, backup_dir, reason]() {
        update_progress("Creating manual backup", platform::to_utf8(archive_path.filename()), 0, 1);
        BackupManager bm(backup_dir);
        auto res = bm.create_backup(archive_path, BackupKind::Manual, reason);
        AsyncCompletion comp;
        comp.kind = AsyncCompletion::Kind::CreatedBackup;
        if (res.ok()) {
            comp.ok = true;
            comp.backup_record = std::move(*res.value);
            comp.message = "Manual backup created: " + comp.backup_record->id;
        } else {
            comp.ok = false;
            comp.message = res.error->message;
            comp.detail = res.error->detail;
        }
        push_completion(std::move(comp));
    });
}

void ArchiveController::start_restore_backup(
    const BackupRecord& record,
    std::optional<std::size_t> reload_tab) {
    if (busy_.exchange(true)) {
        return;
    }
    join_worker();
    cancel_flag_.store(false, std::memory_order_relaxed);
    const auto backup_dir = backup_manager_.backup_directory();

    worker_ = std::thread([this, record, reload_tab, backup_dir]() {
        update_progress("Restoring backup", record.id, 0, 1);
        BackupManager bm(backup_dir);
        auto res = bm.restore_backup(record);
        AsyncCompletion comp;
        comp.kind = AsyncCompletion::Kind::RestoredBackup;
        comp.tab_index = reload_tab;
        if (res.ok()) {
            comp.ok = true;
            comp.message = "Restored backup " + record.id;
            if (reload_tab.has_value()) {
                auto reopened = ArchiveDocument::open(record.original_archive_path);
                if (reopened.ok()) {
                    comp.opened_doc = std::move(*reopened.value);
                }
            }
        } else {
            comp.ok = false;
            comp.message = res.error->message;
            comp.detail = res.error->detail;
        }
        push_completion(std::move(comp));
    });
}

} // namespace imgstudio::app
