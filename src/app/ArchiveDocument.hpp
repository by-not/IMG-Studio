#pragma once

#include "app/AppSettings.hpp"
#include "app/BackupManager.hpp"
#include "app/EntryFilter.hpp"
#include "app/OperationHistory.hpp"
#include "core/ImgArchive.hpp"
#include "core/ImgBuilder.hpp"
#include "core/ImgExtractor.hpp"

#include <atomic>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace imgstudio::app {

struct ImportRejectedItem {
    std::filesystem::path source_path;
    std::string reason;
};

struct ImportReport {
    std::size_t imported_count = 0;
    std::size_t replaced_count = 0;
    std::size_t renamed_count = 0;
    std::size_t skipped_count = 0;
    std::vector<ImportRejectedItem> rejected;
    std::vector<std::filesystem::path> unresolved_duplicates;
};

struct CommitResult {
    std::filesystem::path target_path;
    std::optional<BackupRecord> auto_backup;
    core::BuildResult build_result;
};

class ArchiveDocument {
public:
    [[nodiscard]] static core::Result<ArchiveDocument> open(
        const std::filesystem::path& path,
        const core::ValidationOptions& val_opts = {});

    [[nodiscard]] static core::Result<ArchiveDocument> create_new(
        const std::filesystem::path& path);

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return archive_.path(); }
    [[nodiscard]] std::uint64_t file_size_bytes() const noexcept { return archive_.file_size_bytes(); }
    [[nodiscard]] const core::ValidationReport& validation_report() const noexcept {
        return archive_.validation_report();
    }
    [[nodiscard]] const std::vector<DocumentEntry>& entries() const noexcept { return entries_; }
    [[nodiscard]] const OperationHistory& history() const noexcept { return history_; }

    [[nodiscard]] bool has_pending_changes() const noexcept;
    [[nodiscard]] std::size_t pending_change_count() const noexcept;
    [[nodiscard]] std::size_t active_entry_count() const noexcept;

    [[nodiscard]] core::Result<ImportReport> stage_import(
        const std::vector<std::filesystem::path>& files,
        DuplicateImportPolicy policy);

    [[nodiscard]] core::Result<void> stage_replace(
        std::uint64_t entry_id,
        const std::filesystem::path& source_file);

    [[nodiscard]] core::Result<std::size_t> stage_delete(
        const std::vector<std::uint64_t>& entry_ids);

    [[nodiscard]] core::Result<void> stage_rename(
        std::uint64_t entry_id,
        const std::string& new_name);

    bool undo();
    bool redo();
    void discard_pending_changes();

    [[nodiscard]] core::Result<core::ExtractionSummary> export_entries(
        const std::vector<std::uint64_t>& entry_ids,
        const std::filesystem::path& target_dir,
        const core::ExtractionOptions& options = {}) const;

    [[nodiscard]] core::Result<core::ExtractionSummary> export_all(
        const std::filesystem::path& target_dir,
        const core::ExtractionOptions& options = {}) const;

    [[nodiscard]] core::Result<core::RebuildPlan> make_rebuild_plan() const;

    [[nodiscard]] core::Result<CommitResult> commit_rebuild(
        BackupManager* backup_manager,
        bool create_auto_backup,
        const core::BuildOptions& build_options = {});

    [[nodiscard]] core::Result<void> reload_from_disk(const core::ValidationOptions& val_opts = {});
    core::ValidationReport revalidate(const core::ValidationOptions& val_opts = {});
    void set_validation_report(core::ValidationReport report) { archive_.set_validation_report(std::move(report)); }

    [[nodiscard]] const DocumentEntry* find_by_id(std::uint64_t entry_id) const noexcept;

private:
    explicit ArchiveDocument(core::ImgArchive archive);
    void populate_entries_from_archive();
    [[nodiscard]] std::string make_unique_auto_name(std::string_view desired_name) const;

    core::ImgArchive archive_;
    std::vector<DocumentEntry> entries_;
    OperationHistory history_;
    std::uint64_t next_id_ = 1;
};

} // namespace imgstudio::app
