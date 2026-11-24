#pragma once

#include "core/ImgTypes.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace imgstudio::core {

enum class CompareDiffKind : std::uint8_t {
    Unchanged = 0,
    Added,
    Removed,
    Modified,
    MetadataDiff,
    Ambiguous
};

struct ComparedEntrySide {
    bool present = false;
    std::string name;
    std::uint32_t index = 0;
    std::uint32_t offset_sectors = 0;
    std::uint16_t streaming_size = 0;
    std::uint16_t archive_size = 0;
    std::uint64_t effective_bytes = 0;
    std::string sha256_hex;
    bool hash_ok = false;
    bool duplicate_in_archive = false;
};

struct CompareEntryResult {
    CompareDiffKind kind = CompareDiffKind::Unchanged;
    std::string display_name;
    ComparedEntrySide side_a;
    ComparedEntrySide side_b;
    std::string detail;
};

struct CompareSummaryCounts {
    std::size_t archive_a_entries = 0;
    std::size_t archive_b_entries = 0;
    std::size_t unchanged = 0;
    std::size_t added = 0;
    std::size_t removed = 0;
    std::size_t modified = 0;
    std::size_t metadata_diff = 0;
    std::size_t ambiguous = 0;
};

struct ArchiveComparisonResult {
    std::filesystem::path archive_a_path;
    std::filesystem::path archive_b_path;
    std::uint64_t archive_a_size_bytes = 0;
    std::uint64_t archive_b_size_bytes = 0;
    CompareSummaryCounts counts{};
    std::vector<CompareEntryResult> entries;
    bool canceled = false;
};

struct CompareOptions {
    std::function<void(std::size_t current, std::size_t total, std::string_view item_name)> progress_cb;
    const std::atomic<bool>* cancel_flag = nullptr;
};

[[nodiscard]] const char* compare_diff_label(CompareDiffKind kind) noexcept;

class ImgCompare {
public:
    [[nodiscard]] static Result<ArchiveComparisonResult> compare(
        const std::filesystem::path& archive_a,
        const std::filesystem::path& archive_b,
        const CompareOptions& options = {});
};

} // namespace imgstudio::core
