#include "app/ArchiveDocument.hpp"
#include "core/BinaryIO.hpp"
#include "core/ImgFilename.hpp"
#include "platform/AtomicFileOps.hpp"
#include "platform/Platform.hpp"

#include <algorithm>
#include <unordered_set>

namespace imgstudio::app {

ArchiveDocument::ArchiveDocument(core::ImgArchive archive)
    : archive_(std::move(archive)) {
    populate_entries_from_archive();
}

void ArchiveDocument::populate_entries_from_archive() {
    entries_.clear();
    history_.clear();
    next_id_ = 1;

    const auto& raw_entries = archive_.entries();
    entries_.reserve(raw_entries.size());
    for (const auto& e : raw_entries) {
        DocumentEntry d;
        d.id = next_id_++;
        d.original_index = e.index;
        d.name = e.name;
        d.original_name = e.name;
        d.extension = e.extension;
        d.category = e.category;
        d.offset_sectors = e.offset_sectors;
        d.streaming_size = e.streaming_size;
        d.archive_size = e.archive_size;
        d.raw_name = e.raw_name;
        d.preserve_raw_name = true;
        d.original_offset_bytes = e.offset_bytes();
        d.original_effective_bytes = e.effective_bytes();
        d.change_state = EntryChangeState::Unchanged;
        entries_.push_back(std::move(d));
    }
}

core::Result<ArchiveDocument> ArchiveDocument::open(
    const std::filesystem::path& path,
    const core::ValidationOptions& val_opts) {
    auto res = core::ImgArchive::open(path, val_opts);
    if (!res.ok()) {
        return core::Result<ArchiveDocument>::failure(res.error->message, res.error->detail);
    }
    return core::Result<ArchiveDocument>::success(ArchiveDocument(std::move(*res.value)));
}

core::Result<ArchiveDocument> ArchiveDocument::create_new(const std::filesystem::path& path) {
    core::RebuildPlan empty_plan;
    const auto tmp = platform::make_unique_temp_path(path, ".newtmp");
    auto build_res = core::ImgBuilder::build_to_file(empty_plan, tmp);
    if (!build_res.ok()) {
        return core::Result<ArchiveDocument>::failure(build_res.error->message, build_res.error->detail);
    }
    auto rep = platform::atomic_replace_file(tmp, path);
    if (!rep.ok()) {
        std::error_code ec;
        std::filesystem::remove(tmp, ec);
        return core::Result<ArchiveDocument>::failure(rep.error->message, rep.error->detail);
    }
    return open(path);
}

bool ArchiveDocument::has_pending_changes() const noexcept {
    return std::any_of(entries_.begin(), entries_.end(), [](const DocumentEntry& e) {
        return e.is_modified();
    });
}

std::size_t ArchiveDocument::pending_change_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        entries_.begin(),
        entries_.end(),
        [](const DocumentEntry& e) { return e.is_modified(); }));
}

std::size_t ArchiveDocument::active_entry_count() const noexcept {
    return static_cast<std::size_t>(std::count_if(
        entries_.begin(),
        entries_.end(),
        [](const DocumentEntry& e) { return e.change_state != EntryChangeState::Deleted; }));
}

const DocumentEntry* ArchiveDocument::find_by_id(std::uint64_t entry_id) const noexcept {
    for (const auto& e : entries_) {
        if (e.id == entry_id) {
            return &e;
        }
    }
    return nullptr;
}

std::string ArchiveDocument::make_unique_auto_name(std::string_view desired_name) const {
    std::unordered_set<std::string> active_lower;
    active_lower.reserve(entries_.size());
    for (const auto& e : entries_) {
        if (e.change_state != EntryChangeState::Deleted) {
            active_lower.insert(core::to_ascii_lower(e.name));
        }
    }

    const std::filesystem::path p = platform::from_utf8(std::string(desired_name));
    const std::string ext = p.extension().string();
    const std::string stem = p.stem().string();

    for (int idx = 1; idx < 10000; ++idx) {
        const std::string suffix = "_" + std::to_string(idx);
        const std::size_t max_stem_len = (core::kMaxFilenameLength > ext.size() + suffix.size())
            ? (core::kMaxFilenameLength - ext.size() - suffix.size())
            : 1U;
        std::string trimmed_stem = stem.substr(0, std::min(stem.size(), max_stem_len));
        const std::string candidate = trimmed_stem + suffix + ext;
        if (core::validate_entry_filename(candidate).valid &&
            active_lower.find(core::to_ascii_lower(candidate)) == active_lower.end()) {
            return candidate;
        }
    }
    return {};
}

core::Result<ImportReport> ArchiveDocument::stage_import(
    const std::vector<std::filesystem::path>& files,
    DuplicateImportPolicy policy) {
    ImportReport report;
    if (files.empty()) {
        return core::Result<ImportReport>::success(report);
    }

    auto before = entries_;
    bool changed = false;
    std::error_code ec;

    for (const auto& src : files) {
        if (!std::filesystem::exists(src, ec) || !std::filesystem::is_regular_file(src, ec)) {
            report.rejected.push_back(ImportRejectedItem{src, "File does not exist or is not a regular file."});
            continue;
        }

        const std::string base_name = platform::to_utf8(src.filename());
        const auto fn_val = core::validate_entry_filename(base_name);
        if (!fn_val.valid) {
            report.rejected.push_back(ImportRejectedItem{src, fn_val.reason});
            continue;
        }

        const std::uint64_t fsize = std::filesystem::file_size(src, ec);
        if (ec) {
            report.rejected.push_back(ImportRejectedItem{src, "Cannot read file size: " + ec.message()});
            continue;
        }
        const auto sectors_opt = core::bytes_to_sectors_checked(fsize);
        if (!sectors_opt.has_value()) {
            report.rejected.push_back(ImportRejectedItem{
                src,
                "File exceeds VER2 entry maximum (65,535 sectors / 134,215,680 bytes)."
            });
            continue;
        }

        const std::string lower = core::to_ascii_lower(base_name);
        auto dup_it = std::find_if(entries_.begin(), entries_.end(), [&](const DocumentEntry& e) {
            return e.change_state != EntryChangeState::Deleted &&
                   core::to_ascii_lower(e.name) == lower;
        });

        if (dup_it != entries_.end()) {
            if (policy == DuplicateImportPolicy::Ask) {
                report.unresolved_duplicates.push_back(src);
                continue;
            }
            if (policy == DuplicateImportPolicy::Skip) {
                ++report.skipped_count;
                continue;
            }
            if (policy == DuplicateImportPolicy::Replace) {
                dup_it->uses_external_file = true;
                dup_it->external_path = src;
                dup_it->external_size_bytes = fsize;
                dup_it->streaming_size = *sectors_opt;
                dup_it->archive_size = 0U;
                dup_it->preserve_raw_name = false;
                if (dup_it->change_state != EntryChangeState::Imported) {
                    dup_it->change_state = EntryChangeState::Replaced;
                }
                ++report.replaced_count;
                changed = true;
                continue;
            }
            if (policy == DuplicateImportPolicy::AutoRename) {
                const std::string unique_name = make_unique_auto_name(base_name);
                if (unique_name.empty()) {
                    report.rejected.push_back(ImportRejectedItem{src, "Unable to generate a unique <=23-byte name."});
                    continue;
                }
                DocumentEntry d;
                d.id = next_id_++;
                d.name = unique_name;
                d.original_name = unique_name;
                d.extension = core::extract_extension_lower(unique_name);
                d.category = core::classify_extension(d.extension);
                d.streaming_size = *sectors_opt;
                d.archive_size = 0U;
                d.raw_name = core::encode_raw_filename(unique_name);
                d.uses_external_file = true;
                d.external_path = src;
                d.external_size_bytes = fsize;
                d.change_state = EntryChangeState::Imported;
                entries_.push_back(std::move(d));
                ++report.renamed_count;
                changed = true;
                continue;
            }
        }

        DocumentEntry d;
        d.id = next_id_++;
        d.name = base_name;
        d.original_name = base_name;
        d.extension = core::extract_extension_lower(base_name);
        d.category = core::classify_extension(d.extension);
        d.streaming_size = *sectors_opt;
        d.archive_size = 0U;
        d.raw_name = core::encode_raw_filename(base_name);
        d.uses_external_file = true;
        d.external_path = src;
        d.external_size_bytes = fsize;
        d.change_state = EntryChangeState::Imported;
        entries_.push_back(std::move(d));
        ++report.imported_count;
        changed = true;
    }

    if (changed) {
        const std::size_t total_affected = report.imported_count + report.replaced_count + report.renamed_count;
        history_.push(
            OperationKind::Import,
            "Imported/updated " + std::to_string(total_affected) + " entry(s)",
            std::move(before),
            entries_);
    }

    return core::Result<ImportReport>::success(std::move(report));
}

core::Result<void> ArchiveDocument::stage_replace(
    std::uint64_t entry_id,
    const std::filesystem::path& source_file) {
    auto it = std::find_if(entries_.begin(), entries_.end(), [entry_id](const DocumentEntry& e) {
        return e.id == entry_id;
    });
    if (it == entries_.end() || it->change_state == EntryChangeState::Deleted) {
        return core::Result<void>::failure("Target entry is missing or deleted.");
    }

    std::error_code ec;
    if (!std::filesystem::exists(source_file, ec) || !std::filesystem::is_regular_file(source_file, ec)) {
        return core::Result<void>::failure("Replacement file does not exist.", platform::to_utf8(source_file));
    }

    const std::uint64_t fsize = std::filesystem::file_size(source_file, ec);
    if (ec) {
        return core::Result<void>::failure("Failed to read replacement file size.", ec.message());
    }
    const auto sectors_opt = core::bytes_to_sectors_checked(fsize);
    if (!sectors_opt.has_value()) {
        return core::Result<void>::failure(
            "Replacement file exceeds VER2 entry maximum (65,535 sectors).");
    }

    auto before = entries_;
    it->uses_external_file = true;
    it->external_path = source_file;
    it->external_size_bytes = fsize;
    it->streaming_size = *sectors_opt;
    it->archive_size = 0U;
    it->preserve_raw_name = false;
    if (it->change_state != EntryChangeState::Imported) {
        it->change_state = EntryChangeState::Replaced;
    }

    history_.push(
        OperationKind::Replace,
        "Replaced " + it->name + " with " + platform::to_utf8(source_file.filename()),
        std::move(before),
        entries_);

    return core::Result<void>::success();
}

core::Result<std::size_t> ArchiveDocument::stage_delete(const std::vector<std::uint64_t>& entry_ids) {
    if (entry_ids.empty()) {
        return core::Result<std::size_t>::success(0);
    }

    std::unordered_set<std::uint64_t> id_set(entry_ids.begin(), entry_ids.end());
    auto before = entries_;
    std::size_t removed_count = 0;

    for (auto it = entries_.begin(); it != entries_.end();) {
        if (id_set.count(it->id) == 0U || it->change_state == EntryChangeState::Deleted) {
            ++it;
            continue;
        }
        ++removed_count;
        if (!it->original_index.has_value()) {
            it = entries_.erase(it);
        } else {
            it->change_state = EntryChangeState::Deleted;
            ++it;
        }
    }

    if (removed_count > 0U) {
        history_.push(
            OperationKind::Delete,
            "Marked " + std::to_string(removed_count) + " entry(s) for deletion",
            std::move(before),
            entries_);
    }

    return core::Result<std::size_t>::success(removed_count);
}

core::Result<void> ArchiveDocument::stage_rename(std::uint64_t entry_id, const std::string& new_name) {
    const auto fn_val = core::validate_entry_filename(new_name);
    if (!fn_val.valid) {
        return core::Result<void>::failure("Invalid filename.", fn_val.reason);
    }

    auto it = std::find_if(entries_.begin(), entries_.end(), [entry_id](const DocumentEntry& e) {
        return e.id == entry_id;
    });
    if (it == entries_.end() || it->change_state == EntryChangeState::Deleted) {
        return core::Result<void>::failure("Target entry is missing or deleted.");
    }

    const std::string new_lower = core::to_ascii_lower(new_name);
    for (const auto& other : entries_) {
        if (other.id != entry_id &&
            other.change_state != EntryChangeState::Deleted &&
            core::to_ascii_lower(other.name) == new_lower) {
            return core::Result<void>::failure(
                "Another active entry already has that name (case-insensitive).",
                other.name);
        }
    }

    if (it->name == new_name) {
        return core::Result<void>::success();
    }

    auto before = entries_;
    const std::string prev_name = it->name;
    it->name = new_name;
    it->extension = core::extract_extension_lower(new_name);
    it->category = core::classify_extension(it->extension);
    it->raw_name = core::encode_raw_filename(new_name);
    it->preserve_raw_name = false;

    if (it->change_state == EntryChangeState::Unchanged) {
        it->change_state = EntryChangeState::Renamed;
    } else if (it->change_state == EntryChangeState::Renamed && it->name == it->original_name) {
        it->change_state = EntryChangeState::Unchanged;
        it->preserve_raw_name = true;
    }

    history_.push(
        OperationKind::Rename,
        "Renamed " + prev_name + " -> " + new_name,
        std::move(before),
        entries_);

    return core::Result<void>::success();
}

bool ArchiveDocument::undo() {
    const auto* item = history_.step_undo();
    if (!item) {
        return false;
    }
    entries_ = item->before_snapshot;
    return true;
}

bool ArchiveDocument::redo() {
    const auto* item = history_.step_redo();
    if (!item) {
        return false;
    }
    entries_ = item->after_snapshot;
    return true;
}

void ArchiveDocument::discard_pending_changes() {
    populate_entries_from_archive();
}

core::Result<core::ExtractionSummary> ArchiveDocument::export_entries(
    const std::vector<std::uint64_t>& entry_ids,
    const std::filesystem::path& target_dir,
    const core::ExtractionOptions& options) const {
    std::unordered_set<std::uint64_t> id_set(entry_ids.begin(), entry_ids.end());
    std::vector<core::ExtractionItem> items;
    items.reserve(entry_ids.size());

    for (const auto& e : entries_) {
        if (id_set.count(e.id) == 0U || e.change_state == EntryChangeState::Deleted) {
            continue;
        }
        items.push_back(core::ExtractionItem{
            e.name,
            e.uses_external_file,
            e.external_path,
            e.original_offset_bytes,
            e.uses_external_file ? e.external_size_bytes : e.original_effective_bytes
        });
    }

    return core::ImgExtractor::extract_items(archive_.path(), items, target_dir, options);
}

core::Result<core::ExtractionSummary> ArchiveDocument::export_all(
    const std::filesystem::path& target_dir,
    const core::ExtractionOptions& options) const {
    std::vector<core::ExtractionItem> items;
    items.reserve(entries_.size());
    for (const auto& e : entries_) {
        if (e.change_state == EntryChangeState::Deleted) {
            continue;
        }
        items.push_back(core::ExtractionItem{
            e.name,
            e.uses_external_file,
            e.external_path,
            e.original_offset_bytes,
            e.uses_external_file ? e.external_size_bytes : e.original_effective_bytes
        });
    }
    return core::ImgExtractor::extract_items(archive_.path(), items, target_dir, options);
}

core::Result<core::RebuildPlan> ArchiveDocument::make_rebuild_plan() const {
    core::RebuildPlan plan;
    plan.original_archive_path = archive_.path();
    plan.entries.reserve(entries_.size());

    for (const auto& e : entries_) {
        if (e.change_state == EntryChangeState::Deleted) {
            continue;
        }
        core::BuildEntrySpec spec;
        spec.name = e.name;
        spec.raw_name = e.raw_name;
        spec.preserve_raw_name = e.preserve_raw_name;

        if (e.uses_external_file) {
            spec.source_kind = core::BuildSourceKind::ExternalFile;
            spec.external_path = e.external_path;
            spec.preserve_size_metadata = false;
        } else {
            spec.source_kind = core::BuildSourceKind::OriginalArchiveRange;
            spec.original_offset_bytes = e.original_offset_bytes;
            spec.original_size_bytes = e.original_effective_bytes;
            spec.preserved_streaming_size = e.streaming_size;
            spec.preserved_archive_size = e.archive_size;
            spec.preserve_size_metadata = true;
        }
        plan.entries.push_back(std::move(spec));
    }

    return core::Result<core::RebuildPlan>::success(std::move(plan));
}

core::Result<CommitResult> ArchiveDocument::commit_rebuild(
    BackupManager* backup_manager,
    bool create_auto_backup,
    const core::BuildOptions& build_options) {
    auto plan_res = make_rebuild_plan();
    if (!plan_res.ok()) {
        return core::Result<CommitResult>::failure(plan_res.error->message, plan_res.error->detail);
    }

    CommitResult commit_res;
    commit_res.target_path = archive_.path();

    if (create_auto_backup && backup_manager != nullptr) {
        auto bkp = backup_manager->create_backup(
            archive_.path(),
            BackupKind::Automatic,
            "Automatic backup before rebuild (" + std::to_string(pending_change_count()) + " staged change(s))");
        if (!bkp.ok()) {
            return core::Result<CommitResult>::failure(
                "Pre-rebuild automatic backup failed; commit aborted to keep original archive safe.",
                bkp.error->message + (bkp.error->detail.empty() ? "" : " (" + bkp.error->detail + ")"));
        }
        commit_res.auto_backup = std::move(*bkp.value);
    }

    const auto temp_path = platform::make_unique_temp_path(archive_.path(), ".rebuild.tmp");
    auto build_res = core::ImgBuilder::build_to_file(*plan_res.value, temp_path, build_options);
    if (!build_res.ok()) {
        std::error_code ec;
        std::filesystem::remove(temp_path, ec);
        return core::Result<CommitResult>::failure(build_res.error->message, build_res.error->detail);
    }

    auto replace_res = platform::atomic_replace_file(temp_path, archive_.path());
    if (!replace_res.ok()) {
        std::error_code ec;
        std::filesystem::remove(temp_path, ec);
        return core::Result<CommitResult>::failure(
            "Rebuilt archive was verified, but replacing target file failed (original preserved).",
            replace_res.error->message + (replace_res.error->detail.empty() ? "" : " (" + replace_res.error->detail + ")"));
    }

    commit_res.build_result = std::move(*build_res.value);
    commit_res.build_result.output_path = archive_.path();

    auto reload_res = reload_from_disk();
    if (!reload_res.ok()) {
        return core::Result<CommitResult>::failure(
            "Archive committed to disk, but reloading directory failed.",
            reload_res.error->message);
    }

    return core::Result<CommitResult>::success(std::move(commit_res));
}

core::Result<void> ArchiveDocument::reload_from_disk(const core::ValidationOptions& val_opts) {
    auto res = core::ImgArchive::open(archive_.path(), val_opts);
    if (!res.ok()) {
        return core::Result<void>::failure(res.error->message, res.error->detail);
    }
    archive_ = std::move(*res.value);
    populate_entries_from_archive();
    return core::Result<void>::success();
}

core::ValidationReport ArchiveDocument::revalidate(const core::ValidationOptions& val_opts) {
    return archive_.revalidate(val_opts);
}

} // namespace imgstudio::app
