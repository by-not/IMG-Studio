#pragma once

#include "app/AppSettings.hpp"
#include "app/ArchiveDocument.hpp"
#include "app/BackupManager.hpp"

#include <atomic>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace imgstudio::app {

struct AsyncTaskProgress {
    bool running = false;
    std::string label;
    std::string detail;
    std::size_t current = 0;
    std::size_t total = 0;
};

struct AsyncCompletion {
    enum class Kind : std::uint8_t {
        None = 0,
        OpenedArchive,
        CreatedArchive,
        CommittedRebuild,
        ExportedEntries,
        ValidatedArchive,
        CreatedBackup,
        RestoredBackup
    };

    Kind kind = Kind::None;
    bool ok = false;
    std::string message;
    std::string detail;
    std::optional<std::size_t> tab_index;
    std::optional<ArchiveDocument> opened_doc;
    std::optional<CommitResult> commit_result;
    std::optional<core::ExtractionSummary> extraction_summary;
    std::optional<core::ValidationReport> validation_report;
    std::optional<BackupRecord> backup_record;
};

class ArchiveController {
public:
    ArchiveController();
    ~ArchiveController();

    ArchiveController(const ArchiveController&) = delete;
    ArchiveController& operator=(const ArchiveController&) = delete;

    [[nodiscard]] AppSettings& settings() noexcept { return settings_; }
    [[nodiscard]] const AppSettings& settings() const noexcept { return settings_; }
    void save_settings() const;

    [[nodiscard]] BackupManager& backup_manager() noexcept { return backup_manager_; }
    [[nodiscard]] const BackupManager& backup_manager() const noexcept { return backup_manager_; }

    [[nodiscard]] const std::vector<std::unique_ptr<ArchiveDocument>>& tabs() const noexcept { return tabs_; }
    [[nodiscard]] std::optional<std::size_t> active_tab_index() const noexcept { return active_tab_; }
    [[nodiscard]] ArchiveDocument* active_document() noexcept;
    [[nodiscard]] const ArchiveDocument* active_document() const noexcept;

    void set_active_tab(std::size_t index);
    bool close_tab(std::size_t index);
    [[nodiscard]] bool any_unsaved_changes() const noexcept;

    [[nodiscard]] bool is_busy() const noexcept;
    [[nodiscard]] AsyncTaskProgress progress_snapshot() const;
    void cancel_current_task();

    void start_open_archive(const std::filesystem::path& path);
    void start_create_archive(const std::filesystem::path& path);
    void start_commit_active(bool create_auto_backup);
    void start_export_active(
        std::vector<std::uint64_t> entry_ids,
        bool export_all,
        const std::filesystem::path& target_dir);
    void start_validate_active(bool verify_payload_readability);
    void start_manual_backup_active(const std::string& reason = {});
    void start_restore_backup(const BackupRecord& record, std::optional<std::size_t> reload_tab);

    // Must be called on UI thread each frame to finalize async work and retrieve completion events
    std::vector<AsyncCompletion> poll_completions();

    // Synchronous helper for CLI / smoke test usage
    core::Result<void> open_archive_sync(const std::filesystem::path& path);

private:
    void update_progress(std::string label, std::string detail, std::size_t current, std::size_t total);
    void push_completion(AsyncCompletion comp);
    void join_worker();

    AppSettings settings_;
    std::filesystem::path settings_path_;
    BackupManager backup_manager_;

    std::vector<std::unique_ptr<ArchiveDocument>> tabs_;
    std::optional<std::size_t> active_tab_;

    mutable std::mutex mutex_;
    AsyncTaskProgress progress_{};
    std::vector<AsyncCompletion> completed_queue_;
    std::atomic<bool> cancel_flag_{false};
    std::atomic<bool> busy_{false};
    std::thread worker_;
};

} // namespace imgstudio::app
