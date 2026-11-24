#include "app/CompareService.hpp"
#include "platform/AtomicFileOps.hpp"
#include "platform/Platform.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace imgstudio::app {

CompareService::~CompareService() {
    cancel();
    join_worker();
}

void CompareService::join_worker() {
    if (worker_.joinable()) {
        worker_.join();
    }
}

CompareProgress CompareService::progress_snapshot() const {
    std::lock_guard lock(mutex_);
    return progress_;
}

void CompareService::cancel() {
    cancel_flag_.store(true, std::memory_order_relaxed);
}

std::optional<CompareCompletion> CompareService::poll_completion() {
    std::lock_guard lock(mutex_);
    if (!completion_.has_value()) {
        return std::nullopt;
    }
    auto out = std::move(completion_);
    completion_.reset();
    return out;
}

void CompareService::start_compare(
    const std::filesystem::path& archive_a,
    const std::filesystem::path& archive_b) {
    if (busy_.exchange(true)) {
        return;
    }
    join_worker();
    cancel_flag_.store(false, std::memory_order_relaxed);

    {
        std::lock_guard lock(mutex_);
        progress_ = CompareProgress{true, 0, 0, "Reading IMG directories..."};
        completion_.reset();
    }

    worker_ = std::thread([this, archive_a, archive_b]() {
        core::CompareOptions opts;
        opts.cancel_flag = &cancel_flag_;
        opts.progress_cb = [this](std::size_t cur, std::size_t tot, std::string_view name) {
            std::lock_guard lock(mutex_);
            progress_.running = true;
            progress_.current = cur;
            progress_.total = tot;
            progress_.current_item.assign(name);
        };

        auto res = core::ImgCompare::compare(archive_a, archive_b, opts);
        CompareCompletion comp;
        if (res.ok()) {
            comp.ok = true;
            comp.result = std::move(*res.value);
            comp.message = comp.result->canceled
                ? "Comparison canceled"
                : ("Compared " + std::to_string(comp.result->entries.size()) + " unique entry name(s)");
        } else {
            comp.ok = false;
            comp.message = res.error->message;
            comp.detail = res.error->detail;
        }

        {
            std::lock_guard lock(mutex_);
            progress_.running = false;
            completion_ = std::move(comp);
        }
        busy_.store(false, std::memory_order_relaxed);
    });
}

core::Result<void> CompareService::export_report_json(
    const core::ArchiveComparisonResult& result,
    const std::filesystem::path& dest_file) {
    nlohmann::json j;
    j["archive_a"] = platform::to_utf8(result.archive_a_path);
    j["archive_b"] = platform::to_utf8(result.archive_b_path);
    j["archive_a_size_bytes"] = result.archive_a_size_bytes;
    j["archive_b_size_bytes"] = result.archive_b_size_bytes;
    j["canceled"] = result.canceled;
    j["summary"] = {
        {"archive_a_entries", result.counts.archive_a_entries},
        {"archive_b_entries", result.counts.archive_b_entries},
        {"unchanged", result.counts.unchanged},
        {"added", result.counts.added},
        {"removed", result.counts.removed},
        {"modified", result.counts.modified},
        {"metadata_diff", result.counts.metadata_diff},
        {"ambiguous", result.counts.ambiguous}
    };

    nlohmann::json entries = nlohmann::json::array();
    for (const auto& e : result.entries) {
        nlohmann::json item;
        item["name"] = e.display_name;
        item["status"] = core::compare_diff_label(e.kind);
        item["detail"] = e.detail;
        if (e.side_a.present) {
            item["a"] = {
                {"size_bytes", e.side_a.effective_bytes},
                {"streaming_size", e.side_a.streaming_size},
                {"archive_size", e.side_a.archive_size},
                {"sha256", e.side_a.sha256_hex}
            };
        }
        if (e.side_b.present) {
            item["b"] = {
                {"size_bytes", e.side_b.effective_bytes},
                {"streaming_size", e.side_b.streaming_size},
                {"archive_size", e.side_b.archive_size},
                {"sha256", e.side_b.sha256_hex}
            };
        }
        entries.push_back(std::move(item));
    }
    j["entries"] = std::move(entries);

    const auto tmp = platform::make_unique_temp_path(dest_file, ".json.tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return core::Result<void>::failure("Failed to open report output file.");
        }
        out << j.dump(2) << '\n';
        out.flush();
        if (!out) {
            out.close();
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return core::Result<void>::failure("Failed to flush comparison JSON report.");
        }
    }
    return platform::atomic_replace_file(tmp, dest_file);
}

core::Result<void> CompareService::export_report_text(
    const core::ArchiveComparisonResult& result,
    const std::filesystem::path& dest_file) {
    std::ostringstream oss;
    oss << "IMG Studio 2.0 - Archive Comparison Report\n"
        << "Archive A: " << platform::to_utf8(result.archive_a_path)
        << " (" << result.counts.archive_a_entries << " entries, "
        << result.archive_a_size_bytes << " bytes)\n"
        << "Archive B: " << platform::to_utf8(result.archive_b_path)
        << " (" << result.counts.archive_b_entries << " entries, "
        << result.archive_b_size_bytes << " bytes)\n\n"
        << "Summary:\n"
        << "  Unchanged:     " << result.counts.unchanged << "\n"
        << "  Added:         " << result.counts.added << "\n"
        << "  Removed:       " << result.counts.removed << "\n"
        << "  Modified:      " << result.counts.modified << "\n"
        << "  Metadata Diff: " << result.counts.metadata_diff << "\n"
        << "  Ambiguous:     " << result.counts.ambiguous << "\n\n"
        << "Differences:\n";

    for (const auto& e : result.entries) {
        if (e.kind == core::CompareDiffKind::Unchanged) continue;
        oss << "[" << core::compare_diff_label(e.kind) << "] " << e.display_name
            << " — " << e.detail << "\n";
    }

    const auto tmp = platform::make_unique_temp_path(dest_file, ".txt.tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return core::Result<void>::failure("Failed to open text report output file.");
        }
        const std::string body = oss.str();
        out.write(body.data(), static_cast<std::streamsize>(body.size()));
        out.flush();
        if (!out) {
            out.close();
            std::error_code ec;
            std::filesystem::remove(tmp, ec);
            return core::Result<void>::failure("Failed to write comparison text report.");
        }
    }
    return platform::atomic_replace_file(tmp, dest_file);
}

} // namespace imgstudio::app
