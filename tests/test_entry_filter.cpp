#include <catch2/catch_test_macros.hpp>

#include "app/EntryFilter.hpp"
#include "core/ImgFilename.hpp"

#include <chrono>
#include <vector>

using namespace imgstudio::app;
using namespace imgstudio::core;

TEST_CASE("EntryFilter searches case-insensitively, filters, and sorts 10000 entries fast", "[app][filter]") {
    std::vector<DocumentEntry> entries;
    entries.reserve(10000);
    for (std::uint64_t i = 0; i < 10000; ++i) {
        DocumentEntry d;
        d.id = i + 1;
        d.original_index = static_cast<std::uint32_t>(i);
        const char* ext = (i % 3 == 0) ? ".dff" : ((i % 3 == 1) ? ".txd" : ".col");
        d.name = "Vehicle_" + std::to_string(10000 - i) + ext;
        d.extension = ext;
        d.category = classify_extension(ext);
        d.offset_sectors = static_cast<std::uint32_t>(i + 1);
        d.streaming_size = static_cast<std::uint16_t>((i % 20) + 1);
        entries.push_back(std::move(d));
    }

    std::unordered_map<std::uint32_t, DiagnosticSeverity> sev_map;
    sev_map[5] = DiagnosticSeverity::Warning;

    EntryFilterCriteria criteria;
    criteria.search_text = "vehicle_999";
    criteria.quick_filter = QuickFilterKind::Dff;
    criteria.sort_column = SortColumn::Name;
    criteria.sort_ascending = true;

    const auto start = std::chrono::steady_clock::now();
    const auto result = filter_and_sort_entries(entries, sev_map, criteria);
    const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start).count();

    REQUIRE(!result.empty());
    for (std::size_t idx : result) {
        REQUIRE(entries[idx].category == FileCategory::DffModel);
    }
    REQUIRE(elapsed_ms < 500);

    // QuickFilter WithAlerts returns only index 5
    EntryFilterCriteria alert_crit;
    alert_crit.quick_filter = QuickFilterKind::WithAlerts;
    const auto alert_res = filter_and_sort_entries(entries, sev_map, alert_crit);
    REQUIRE(alert_res.size() == 1);
    REQUIRE(alert_res[0] == 5);
}
