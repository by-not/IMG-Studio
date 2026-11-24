#include "app/OperationHistory.hpp"

namespace imgstudio::app {

void OperationHistory::clear() {
    records_.clear();
    cursor_ = 0;
}

void OperationHistory::push(
    OperationKind kind,
    std::string description,
    std::vector<DocumentEntry> before_snapshot,
    std::vector<DocumentEntry> after_snapshot) {
    if (cursor_ < records_.size()) {
        records_.erase(records_.begin() + static_cast<std::ptrdiff_t>(cursor_), records_.end());
    }
    records_.push_back(HistoryEntry{
        kind,
        std::move(description),
        std::move(before_snapshot),
        std::move(after_snapshot)
    });
    cursor_ = records_.size();
}

const HistoryEntry* OperationHistory::step_undo() {
    if (!can_undo()) {
        return nullptr;
    }
    --cursor_;
    return &records_[cursor_];
}

const HistoryEntry* OperationHistory::step_redo() {
    if (!can_redo()) {
        return nullptr;
    }
    const HistoryEntry* item = &records_[cursor_];
    ++cursor_;
    return item;
}

const char* change_state_label(EntryChangeState state) noexcept {
    switch (state) {
        case EntryChangeState::Unchanged: return "Clean";
        case EntryChangeState::Imported: return "Added";
        case EntryChangeState::Replaced: return "Replaced";
        case EntryChangeState::Renamed: return "Renamed";
        case EntryChangeState::Deleted: return "Deleted";
    }
    return "Unknown";
}

const char* operation_kind_label(OperationKind kind) noexcept {
    switch (kind) {
        case OperationKind::Import: return "Import";
        case OperationKind::Replace: return "Replace";
        case OperationKind::Delete: return "Delete";
        case OperationKind::Rename: return "Rename";
    }
    return "Unknown";
}

} // namespace imgstudio::app
