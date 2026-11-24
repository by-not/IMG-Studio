#include "core/ImgCompare.hpp"
#include "core/BinaryIO.hpp"
#include "core/ImgFilename.hpp"
#include "core/ImgReader.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <unordered_map>

namespace imgstudio::core {

const char* compare_diff_label(CompareDiffKind kind) noexcept {
    switch (kind) {
        case CompareDiffKind::Unchanged: return "Unchanged";
        case CompareDiffKind::Added: return "Added";
        case CompareDiffKind::Removed: return "Removed";
        case CompareDiffKind::Modified: return "Modified";
        case CompareDiffKind::MetadataDiff: return "Metadata Diff";
        case CompareDiffKind::Ambiguous: return "Ambiguous";
    }
    return "Unknown";
}

namespace {

bool stream_hash_entry(
    std::ifstream& in,
    std::uint64_t file_size,
    const ImgEntry& entry,
    std::string& out_hex) {
    const std::uint64_t off = entry.offset_bytes();
    const std::uint64_t len = entry.effective_bytes();
    if (off > file_size || len > file_size - off) {
        return false;
    }
    if (len == 0ULL) {
        out_hex = Sha256::hash_bytes_hex({});
        return true;
    }

    in.clear();
    in.seekg(static_cast<std::streamoff>(off), std::ios::beg);
    if (!in) {
        return false;
    }

    Sha256 sha;
    std::array<std::uint8_t, 65536> buf{};
    std::uint64_t rem = len;
    while (rem > 0ULL) {
        const std::size_t chunk = static_cast<std::size_t>(std::min<std::uint64_t>(rem, buf.size()));
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(chunk));
        if (!in || static_cast<std::size_t>(in.gcount()) != chunk) {
            return false;
        }
        sha.update(std::span<const std::uint8_t>(buf.data(), chunk));
        rem -= chunk;
    }
    out_hex = sha.finalize_hex();
    return true;
}

struct SideIndexItem {
    std::vector<std::size_t> entry_indices;
};

using NameIndexMap = std::unordered_map<std::string, SideIndexItem>;

NameIndexMap index_entries(const ImgArchiveData& data, std::vector<std::string>& ordered_keys) {
    NameIndexMap map;
    map.reserve(data.entries.size());
    for (std::size_t i = 0; i < data.entries.size(); ++i) {
        const std::string key = to_ascii_lower(data.entries[i].name);
        auto it = map.find(key);
        if (it == map.end()) {
            ordered_keys.push_back(key);
            map.emplace(key, SideIndexItem{{i}});
        } else {
            it->second.entry_indices.push_back(i);
        }
    }
    return map;
}

ComparedEntrySide build_side_info(
    const ImgArchiveData& data,
    std::ifstream& stream,
    std::size_t entry_idx,
    bool is_duplicate) {
    const auto& e = data.entries[entry_idx];
    ComparedEntrySide side;
    side.present = true;
    side.name = e.name;
    side.index = e.index;
    side.offset_sectors = e.offset_sectors;
    side.streaming_size = e.streaming_size;
    side.archive_size = e.archive_size;
    side.effective_bytes = e.effective_bytes();
    side.duplicate_in_archive = is_duplicate;
    side.hash_ok = stream_hash_entry(stream, data.file_size_bytes, e, side.sha256_hex);
    return side;
}

} // namespace

Result<ArchiveComparisonResult> ImgCompare::compare(
    const std::filesystem::path& archive_a,
    const std::filesystem::path& archive_b,
    const CompareOptions& options) {
    auto ra = ImgReader::read(archive_a);
    if (!ra.ok()) {
        return Result<ArchiveComparisonResult>::failure(
            "Failed to read Archive A.",
            ra.error->message);
    }
    if (!ra.value->header_present || ra.value->magic != kVer2Magic) {
        return Result<ArchiveComparisonResult>::failure(
            "Archive A is not a valid VER2 IMG archive.");
    }

    auto rb = ImgReader::read(archive_b);
    if (!rb.ok()) {
        return Result<ArchiveComparisonResult>::failure(
            "Failed to read Archive B.",
            rb.error->message);
    }
    if (!rb.value->header_present || rb.value->magic != kVer2Magic) {
        return Result<ArchiveComparisonResult>::failure(
            "Archive B is not a valid VER2 IMG archive.");
    }

    std::ifstream stream_a(archive_a, std::ios::binary);
    if (!stream_a.is_open()) {
        return Result<ArchiveComparisonResult>::failure("Failed to open Archive A stream.");
    }
    std::ifstream stream_b(archive_b, std::ios::binary);
    if (!stream_b.is_open()) {
        return Result<ArchiveComparisonResult>::failure("Failed to open Archive B stream.");
    }

    std::vector<std::string> keys_a;
    std::vector<std::string> keys_b;
    const auto map_a = index_entries(*ra.value, keys_a);
    const auto map_b = index_entries(*rb.value, keys_b);

    std::vector<std::string> all_keys = keys_a;
    for (const auto& kb : keys_b) {
        if (map_a.find(kb) == map_a.end()) {
            all_keys.push_back(kb);
        }
    }

    ArchiveComparisonResult out;
    out.archive_a_path = archive_a;
    out.archive_b_path = archive_b;
    out.archive_a_size_bytes = ra.value->file_size_bytes;
    out.archive_b_size_bytes = rb.value->file_size_bytes;
    out.counts.archive_a_entries = ra.value->entries.size();
    out.counts.archive_b_entries = rb.value->entries.size();
    out.entries.reserve(all_keys.size());

    const std::size_t total = all_keys.size();
    for (std::size_t i = 0; i < total; ++i) {
        if (options.cancel_flag && options.cancel_flag->load(std::memory_order_relaxed)) {
            out.canceled = true;
            break;
        }

        const std::string& key = all_keys[i];
        const auto ita = map_a.find(key);
        const auto itb = map_b.find(key);

        CompareEntryResult item;
        if (ita != map_a.end() && itb == map_b.end()) {
            const bool dup_a = ita->second.entry_indices.size() > 1U;
            item.side_a = build_side_info(*ra.value, stream_a, ita->second.entry_indices[0], dup_a);
            item.display_name = item.side_a.name;
            if (dup_a || key.empty()) {
                item.kind = CompareDiffKind::Ambiguous;
                item.detail = dup_a ? "Duplicate entries exist in Archive A." : "Empty entry name in Archive A.";
                ++out.counts.ambiguous;
            } else {
                item.kind = CompareDiffKind::Removed;
                item.detail = "Present only in Archive A.";
                ++out.counts.removed;
            }
        } else if (ita == map_a.end() && itb != map_b.end()) {
            const bool dup_b = itb->second.entry_indices.size() > 1U;
            item.side_b = build_side_info(*rb.value, stream_b, itb->second.entry_indices[0], dup_b);
            item.display_name = item.side_b.name;
            if (dup_b || key.empty()) {
                item.kind = CompareDiffKind::Ambiguous;
                item.detail = dup_b ? "Duplicate entries exist in Archive B." : "Empty entry name in Archive B.";
                ++out.counts.ambiguous;
            } else {
                item.kind = CompareDiffKind::Added;
                item.detail = "Present only in Archive B.";
                ++out.counts.added;
            }
        } else {
            const bool dup_a = ita->second.entry_indices.size() > 1U;
            const bool dup_b = itb->second.entry_indices.size() > 1U;
            item.side_a = build_side_info(*ra.value, stream_a, ita->second.entry_indices[0], dup_a);
            item.side_b = build_side_info(*rb.value, stream_b, itb->second.entry_indices[0], dup_b);
            item.display_name = item.side_b.name.empty() ? item.side_a.name : item.side_b.name;

            if (dup_a || dup_b || !item.side_a.hash_ok || !item.side_b.hash_ok || key.empty()) {
                item.kind = CompareDiffKind::Ambiguous;
                if (dup_a || dup_b) {
                    item.detail = "Duplicate entry names make 1-to-1 comparison ambiguous.";
                } else if (!item.side_a.hash_ok || !item.side_b.hash_ok) {
                    item.detail = "One or both payloads are out-of-bounds or unreadable.";
                } else {
                    item.detail = "Empty entry name.";
                }
                ++out.counts.ambiguous;
            } else if (item.side_a.effective_bytes != item.side_b.effective_bytes ||
                       item.side_a.sha256_hex != item.side_b.sha256_hex) {
                item.kind = CompareDiffKind::Modified;
                item.detail = (item.side_a.effective_bytes != item.side_b.effective_bytes)
                    ? "Payload size and content differ."
                    : "Same sector size, different SHA-256 payload.";
                ++out.counts.modified;
            } else if (item.side_a.streaming_size != item.side_b.streaming_size ||
                       item.side_a.archive_size != item.side_b.archive_size ||
                       item.side_a.name != item.side_b.name) {
                item.kind = CompareDiffKind::MetadataDiff;
                item.detail = "Identical payload SHA-256; directory metadata or filename case differs.";
                ++out.counts.metadata_diff;
            } else {
                item.kind = CompareDiffKind::Unchanged;
                item.detail = "Identical payload and metadata.";
                ++out.counts.unchanged;
            }
        }

        if (options.progress_cb && ((i % 64U) == 0U || i + 1U == total)) {
            options.progress_cb(i + 1U, total, item.display_name);
        }
        out.entries.push_back(std::move(item));
    }

    return Result<ArchiveComparisonResult>::success(std::move(out));
}

} // namespace imgstudio::core
