#pragma once

#include "core/ImgTypes.hpp"

#include <atomic>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace imgstudio::core {

struct ExtractionItem {
    std::string output_name;
    bool from_external_file = false;
    std::filesystem::path external_path;
    std::uint64_t archive_offset_bytes = 0;
    std::uint64_t size_bytes = 0;
};

struct ExtractionFailure {
    std::string entry_name;
    std::string reason;
};

struct ExtractionSummary {
    std::size_t requested = 0;
    std::size_t succeeded = 0;
    std::size_t skipped = 0;
    bool canceled = false;
    std::vector<ExtractionFailure> failures;
};

struct ExtractionOptions {
    bool overwrite_existing = true;
    bool sanitize_unsafe_names = false;
    std::function<void(std::size_t current, std::size_t total, std::string_view current_name)> progress_cb;
    const std::atomic<bool>* cancel_flag = nullptr;
};

class ImgExtractor {
public:
    [[nodiscard]] static Result<void> extract_entry_to_file(
        const std::filesystem::path& archive_path,
        const ImgEntry& entry,
        const std::filesystem::path& dest_file,
        bool overwrite_existing = true);

    [[nodiscard]] static Result<ExtractionSummary> extract_items(
        const std::filesystem::path& archive_path,
        const std::vector<ExtractionItem>& items,
        const std::filesystem::path& target_dir,
        const ExtractionOptions& options = {});
};

} // namespace imgstudio::core
