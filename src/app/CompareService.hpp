#pragma once

#include "core/ImgCompare.hpp"
#include "core/ImgTypes.hpp"

#include <atomic>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace imgstudio::app {

struct CompareProgress {
    bool running = false;
    std::size_t current = 0;
    std::size_t total = 0;
    std::string current_item;
};

struct CompareCompletion {
    bool ok = false;
    std::string message;
    std::string detail;
    std::optional<core::ArchiveComparisonResult> result;
};

class CompareService {
public:
    ~CompareService();

    [[nodiscard]] bool is_busy() const noexcept { return busy_.load(std::memory_order_relaxed); }
    [[nodiscard]] CompareProgress progress_snapshot() const;
    void cancel();
    void start_compare(const std::filesystem::path& archive_a, const std::filesystem::path& archive_b);
    std::optional<CompareCompletion> poll_completion();

    [[nodiscard]] static core::Result<void> export_report_json(
        const core::ArchiveComparisonResult& result,
        const std::filesystem::path& dest_file);

    [[nodiscard]] static core::Result<void> export_report_text(
        const core::ArchiveComparisonResult& result,
        const std::filesystem::path& dest_file);

private:
    void join_worker();

    std::atomic<bool> busy_{false};
    std::atomic<bool> cancel_flag_{false};
    mutable std::mutex mutex_;
    CompareProgress progress_{};
    std::optional<CompareCompletion> completion_;
    std::thread worker_;
};

} // namespace imgstudio::app
