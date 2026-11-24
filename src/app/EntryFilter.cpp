#include "app/EntryFilter.hpp"
#include "core/ImgFilename.hpp"

#include <algorithm>

namespace imgstudio::app {

std::unordered_map<std::uint32_t, core::DiagnosticSeverity>
build_entry_severity_map(const core::ValidationReport& report) {
    std::unordered_map<std::uint32_t, core::DiagnosticSeverity> out;
    for (const auto& d : report.diagnostics) {
        if (!d.entry_index.has_value()) {
            continue;
        }
        auto it = out.find(*d.entry_index);
        if (it == out.end() || static_cast<int>(d.severity) > static_cast<int>(it->second)) {
            out[*d.entry_index] = d.severity;
        }
    }
    return out;
}

namespace {

[[nodiscard]] bool matches_quick_filter(
    const DocumentEntry& entry,
    const std::unordered_map<std::uint32_t, core::DiagnosticSeverity>& severity_map,
    QuickFilterKind qf) {
    switch (qf) {
        case QuickFilterKind::All:
            return true;
        case QuickFilterKind::Dff:
            return entry.category == core::FileCategory::DffModel;
        case QuickFilterKind::Txd:
            return entry.category == core::FileCategory::TxdTexture;
        case QuickFilterKind::Col:
            return entry.category == core::FileCategory::ColCollision;
        case QuickFilterKind::Ifp:
            return entry.category == core::FileCategory::IfpAnimation;
        case QuickFilterKind::IplIde:
            return entry.category == core::FileCategory::IplPlacement ||
                   entry.category == core::FileCategory::IdeDefinition;
        case QuickFilterKind::Other:
            return entry.category != core::FileCategory::DffModel &&
                   entry.category != core::FileCategory::TxdTexture &&
                   entry.category != core::FileCategory::ColCollision &&
                   entry.category != core::FileCategory::IfpAnimation &&
                   entry.category != core::FileCategory::IplPlacement &&
                   entry.category != core::FileCategory::IdeDefinition;
        case QuickFilterKind::PendingChanges:
            return entry.is_modified();
        case QuickFilterKind::WithAlerts:
            if (!entry.original_index.has_value()) {
                return false;
            }
            return severity_map.find(*entry.original_index) != severity_map.end();
    }
    return true;
}

} // namespace

std::vector<std::size_t> filter_and_sort_entries(
    const std::vector<DocumentEntry>& entries,
    const std::unordered_map<std::uint32_t, core::DiagnosticSeverity>& severity_map,
    const EntryFilterCriteria& criteria) {
    const std::string search_lower = core::to_ascii_lower(criteria.search_text);
    std::string ext_lower = core::to_ascii_lower(criteria.extension_filter);
    if (!ext_lower.empty() && ext_lower.front() != '.') {
        ext_lower.insert(ext_lower.begin(), '.');
    }

    std::vector<std::size_t> indices;
    indices.reserve(entries.size());

    for (std::size_t i = 0; i < entries.size(); ++i) {
        const auto& e = entries[i];
        if (!criteria.show_deleted && e.change_state == EntryChangeState::Deleted) {
            continue;
        }
        if (!matches_quick_filter(e, severity_map, criteria.quick_filter)) {
            continue;
        }
        if (!ext_lower.empty() && e.extension != ext_lower) {
            continue;
        }
        if (!search_lower.empty()) {
            const std::string name_lower = core::to_ascii_lower(e.name);
            if (name_lower.find(search_lower) == std::string::npos) {
                continue;
            }
        }
        indices.push_back(i);
    }

    auto severity_rank = [&](const DocumentEntry& e) -> int {
        if (e.is_modified()) {
            return 10 + static_cast<int>(e.change_state);
        }
        if (e.original_index.has_value()) {
            auto it = severity_map.find(*e.original_index);
            if (it != severity_map.end()) {
                return 1 + static_cast<int>(it->second);
            }
        }
        return 0;
    };

    std::stable_sort(indices.begin(), indices.end(), [&](std::size_t ia, std::size_t ib) {
        const auto& a = entries[ia];
        const auto& b = entries[ib];
        int cmp = 0;
        switch (criteria.sort_column) {
            case SortColumn::Index:
                cmp = (a.id < b.id) ? -1 : ((a.id > b.id) ? 1 : 0);
                break;
            case SortColumn::Name: {
                const std::string la = core::to_ascii_lower(a.name);
                const std::string lb = core::to_ascii_lower(b.name);
                cmp = (la < lb) ? -1 : ((la > lb) ? 1 : 0);
                break;
            }
            case SortColumn::Extension:
                cmp = (a.extension < b.extension) ? -1 : ((a.extension > b.extension) ? 1 : 0);
                break;
            case SortColumn::Category:
                cmp = (static_cast<int>(a.category) < static_cast<int>(b.category)) ? -1
                    : ((static_cast<int>(a.category) > static_cast<int>(b.category)) ? 1 : 0);
                break;
            case SortColumn::OffsetSectors:
                cmp = (a.offset_sectors < b.offset_sectors) ? -1
                    : ((a.offset_sectors > b.offset_sectors) ? 1 : 0);
                break;
            case SortColumn::StreamingSize:
                cmp = (a.streaming_size < b.streaming_size) ? -1
                    : ((a.streaming_size > b.streaming_size) ? 1 : 0);
                break;
            case SortColumn::ArchiveSize:
                cmp = (a.archive_size < b.archive_size) ? -1
                    : ((a.archive_size > b.archive_size) ? 1 : 0);
                break;
            case SortColumn::Status: {
                const int ra = severity_rank(a);
                const int rb = severity_rank(b);
                cmp = (ra < rb) ? -1 : ((ra > rb) ? 1 : 0);
                break;
            }
        }
        if (cmp == 0) {
            cmp = (a.id < b.id) ? -1 : ((a.id > b.id) ? 1 : 0);
        }
        return criteria.sort_ascending ? (cmp < 0) : (cmp > 0);
    });

    return indices;
}

} // namespace imgstudio::app
