#pragma once

#include "core/ImgTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace imgstudio::app {

enum class EntryChangeState : std::uint8_t {
    Unchanged = 0,
    Imported,
    Replaced,
    Renamed,
    Deleted
};

struct DocumentEntry {
    std::uint64_t id = 0;
    std::optional<std::uint32_t> original_index;

    std::string name;
    std::string original_name;
    std::string extension;
    core::FileCategory category = core::FileCategory::Other;

    std::uint32_t offset_sectors = 0;
    std::uint16_t streaming_size = 0;
    std::uint16_t archive_size = 0;
    std::array<char, core::kMaxFilenameBytes> raw_name{};
    bool preserve_raw_name = false;

    std::uint64_t original_offset_bytes = 0;
    std::uint64_t original_effective_bytes = 0;

    bool uses_external_file = false;
    std::filesystem::path external_path;
    std::uint64_t external_size_bytes = 0;

    EntryChangeState change_state = EntryChangeState::Unchanged;

    [[nodiscard]] std::uint16_t effective_sectors() const noexcept {
        return archive_size != 0U ? archive_size : streaming_size;
    }

    [[nodiscard]] std::uint64_t display_bytes() const noexcept {
        if (uses_external_file) {
            return external_size_bytes;
        }
        return static_cast<std::uint64_t>(effective_sectors()) * core::kSectorSize;
    }

    [[nodiscard]] bool is_modified() const noexcept {
        return change_state != EntryChangeState::Unchanged;
    }
};

enum class OperationKind : std::uint8_t {
    Import = 0,
    Replace,
    Delete,
    Rename
};

struct HistoryEntry {
    OperationKind kind = OperationKind::Import;
    std::string description;
    std::vector<DocumentEntry> before_snapshot;
    std::vector<DocumentEntry> after_snapshot;
};

class OperationHistory {
public:
    void clear();
    void push(
        OperationKind kind,
        std::string description,
        std::vector<DocumentEntry> before_snapshot,
        std::vector<DocumentEntry> after_snapshot);

    [[nodiscard]] bool can_undo() const noexcept { return cursor_ > 0; }
    [[nodiscard]] bool can_redo() const noexcept { return cursor_ < records_.size(); }
    [[nodiscard]] std::size_t size() const noexcept { return records_.size(); }
    [[nodiscard]] std::size_t cursor() const noexcept { return cursor_; }
    [[nodiscard]] const std::vector<HistoryEntry>& records() const noexcept { return records_; }

    const HistoryEntry* step_undo();
    const HistoryEntry* step_redo();

private:
    std::vector<HistoryEntry> records_;
    std::size_t cursor_ = 0;
};

[[nodiscard]] const char* change_state_label(EntryChangeState state) noexcept;
[[nodiscard]] const char* operation_kind_label(OperationKind kind) noexcept;

} // namespace imgstudio::app
