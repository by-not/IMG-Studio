#pragma once

#include "app/AppSettings.hpp"
#include "core/ImgTypes.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace imgstudio::app {

enum class BatchOperationKind : std::uint8_t {
    Validate = 0,
    Backup,
    ExportAll,
    ImportFiles,
    ReplaceMatching
};

enum class BatchItemStatus : std::uint8_t {
    Pending = 0,
    Running,
    Succeeded,
    Failed,
    Canceled
};

struct BatchJobSpec {
    BatchOperationKind operation = BatchOperationKind::Validate;
    std::vector<std::filesystem::path> archive_paths;
    std::filesystem::path output_directory;
    std::vector<std::filesystem::path> input_files;
    std::filesystem::path backup_directory;
    bool create_auto_backup = true;
    bool verify_payload_readability = false;
    DuplicateImportPolicy duplicate_policy = DuplicateImportPolicy::Replace;
    std::size_t worker_threads = 2U;
};

struct BatchItemResult {
    std::filesystem::path archive_path;
    BatchItemStatus status = BatchItemStatus::Pending;
    std::string summary;
    std::string error_detail;
    std::string backup_id;
};

struct BatchSummary {
    bool running = false;
    bool finished = false;
    bool canceled = false;
    BatchOperationKind operation = BatchOperationKind::Validate;
    std::size_t total = 0;
    std::size_t completed = 0;
    std::size_t succeeded = 0;
    std::size_t failed = 0;
    std::size_t canceled_count = 0;
    std::vector<BatchItemResult> items;
};

[[nodiscard]] const char* batch_operation_label(BatchOperationKind op) noexcept;
[[nodiscard]] const char* batch_status_label(BatchItemStatus st) noexcept;

class BatchProcessor {
public:
    ~BatchProcessor();

    [[nodiscard]] bool is_running() const noexcept { return running_.load(std::memory_order_relaxed); }
    [[nodiscard]] BatchSummary snapshot() const;
    void start(BatchJobSpec spec);
    void cancel();

    [[nodiscard]] static BatchSummary run_sync(const BatchJobSpec& spec, const std::atomic<bool>* cancel_flag = nullptr);

    [[nodiscard]] static core::Result<void> export_summary_json(
        const BatchSummary& summary,
        const std::filesystem::path& dest_file);

private:
    void join_coordinator();

    std::atomic<bool> running_{false};
    std::atomic<bool> cancel_flag_{false};
    mutable std::mutex mutex_;
    BatchSummary state_{};
    std::thread coordinator_;
};

} // namespace imgstudio::app
