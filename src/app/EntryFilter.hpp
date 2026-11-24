#pragma once

#include "app/OperationHistory.hpp"
#include "core/ImgTypes.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace imgstudio::app {

enum class QuickFilterKind : std::uint8_t {
    All = 0,
    Dff,
    Txd,
    Col,
    Ifp,
    IplIde,
    Other,
    PendingChanges,
    WithAlerts
};

enum class SortColumn : std::uint8_t {
    Index = 0,
    Name,
    Extension,
    Category,
    OffsetSectors,
    StreamingSize,
    ArchiveSize,
    Status
};

struct EntryFilterCriteria {
    std::string search_text;
    std::string extension_filter;
    QuickFilterKind quick_filter = QuickFilterKind::All;
    bool show_deleted = true;
    SortColumn sort_column = SortColumn::Index;
    bool sort_ascending = true;
};

[[nodiscard]] std::unordered_map<std::uint32_t, core::DiagnosticSeverity>
build_entry_severity_map(const core::ValidationReport& report);

[[nodiscard]] std::vector<std::size_t> filter_and_sort_entries(
    const std::vector<DocumentEntry>& entries,
    const std::unordered_map<std::uint32_t, core::DiagnosticSeverity>& severity_map,
    const EntryFilterCriteria& criteria);

} // namespace imgstudio::app
