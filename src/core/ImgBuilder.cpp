#include "core/ImgBuilder.hpp"
#include "core/BinaryIO.hpp"
#include "core/ImgFilename.hpp"
#include "core/ImgReader.hpp"
#include "core/ImgValidator.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <limits>
#include <unordered_set>

namespace imgstudio::core {

namespace {

struct PlannedEntryLayout {
    std::uint32_t offset_sectors = 0;
    std::uint16_t streaming_size = 0;
    std::uint16_t archive_size = 0;
    std::uint16_t effective_sectors = 0;
    std::uint64_t payload_bytes_to_copy = 0;
    std::array<char, kMaxFilenameBytes> raw_name{};
    std::string expected_padded_sha256;
};

void remove_file_quietly(const std::filesystem::path& p) noexcept {
    std::error_code ec;
    std::filesystem::remove(p, ec);
}

[[nodiscard]] Result<void> verify_built_archive(
    const std::filesystem::path& output_path,
    const std::vector<PlannedEntryLayout>& layouts) {
    auto read_res = ImgReader::read(output_path);
    if (!read_res.ok()) {
        return Result<void>::failure(
            "Post-build verification failed while re-reading output archive.",
            read_res.error->message);
    }

    const auto& data = *read_res.value;
    if (data.entries.size() != layouts.size()) {
        return Result<void>::failure(
            "Post-build verification failed: entry count mismatch.");
    }

    const ValidationReport report = ImgValidator::validate(data);
    if (!report.is_valid_structure()) {
        std::string first_err;
        for (const auto& d : report.diagnostics) {
            if (d.severity == DiagnosticSeverity::Error) {
                first_err = d.message;
                break;
            }
        }
        return Result<void>::failure(
            "Post-build verification detected structural errors.",
            first_err);
    }

    std::ifstream in(output_path, std::ios::binary);
    if (!in.is_open()) {
        return Result<void>::failure(
            "Post-build verification could not reopen output archive.",
            output_path.string());
    }

    std::array<std::uint8_t, 65536> buf{};
    for (std::size_t i = 0; i < layouts.size(); ++i) {
        const auto& expected = layouts[i];
        const auto& actual = data.entries[i];
        if (actual.offset_sectors != expected.offset_sectors ||
            actual.streaming_size != expected.streaming_size ||
            actual.archive_size != expected.archive_size ||
            actual.raw_name != expected.raw_name) {
            return Result<void>::failure(
                "Post-build verification failed: directory metadata mismatch.",
                actual.name);
        }

        const std::uint64_t sector_bytes = static_cast<std::uint64_t>(expected.effective_sectors) * kSectorSize;
        if (sector_bytes == 0ULL) {
            continue;
        }

        in.clear();
        in.seekg(static_cast<std::streamoff>(actual.offset_bytes()), std::ios::beg);
        if (!in) {
            return Result<void>::failure(
                "Post-build verification failed to seek to payload.",
                actual.name);
        }

        Sha256 sha;
        std::uint64_t remaining = sector_bytes;
        while (remaining > 0ULL) {
            const std::size_t chunk = static_cast<std::size_t>(
                std::min<std::uint64_t>(remaining, buf.size()));
            in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(chunk));
            if (!in || static_cast<std::size_t>(in.gcount()) != chunk) {
                return Result<void>::failure(
                    "Post-build verification encountered premature EOF.",
                    actual.name);
            }
            sha.update(std::span<const std::uint8_t>(buf.data(), chunk));
            remaining -= chunk;
        }

        if (sha.finalize_hex() != expected.expected_padded_sha256) {
            return Result<void>::failure(
                "Post-build verification failed: SHA-256 payload mismatch.",
                actual.name);
        }
    }

    return Result<void>::success();
}

} // namespace

Result<BuildResult> ImgBuilder::build_to_file(
    const RebuildPlan& plan,
    const std::filesystem::path& temp_output_path,
    const BuildOptions& options) {
    if (plan.entries.size() > std::numeric_limits<std::uint32_t>::max()) {
        return Result<BuildResult>::failure("Entry count exceeds 32-bit VER2 limit.");
    }

    const std::uint32_t entry_count = static_cast<std::uint32_t>(plan.entries.size());
    const auto dir_bytes_opt = checked_mul_u64(
        static_cast<std::uint64_t>(entry_count),
        kDirectoryEntrySize);
    if (!dir_bytes_opt.has_value()) {
        return Result<BuildResult>::failure("Directory table size overflow.");
    }
    const auto header_and_dir_opt = checked_add_u64(kHeaderSize, *dir_bytes_opt);
    if (!header_and_dir_opt.has_value()) {
        return Result<BuildResult>::failure("Header + directory size overflow.");
    }

    const std::uint64_t header_and_dir_bytes = *header_and_dir_opt;
    const std::uint64_t first_data_sector_u64 = sectors_needed_u64(header_and_dir_bytes);
    if (first_data_sector_u64 > std::numeric_limits<std::uint32_t>::max()) {
        return Result<BuildResult>::failure("Directory table exceeds 32-bit sector addressing.");
    }

    std::error_code ec;
    const bool needs_original = std::any_of(
        plan.entries.begin(),
        plan.entries.end(),
        [](const BuildEntrySpec& s) {
            return s.source_kind == BuildSourceKind::OriginalArchiveRange;
        });

    std::uint64_t original_size = 0;
    std::ifstream original_in;
    if (needs_original) {
        original_size = std::filesystem::file_size(plan.original_archive_path, ec);
        if (ec) {
            return Result<BuildResult>::failure(
                "Cannot stat original archive during rebuild.",
                ec.message());
        }
        original_in.open(plan.original_archive_path, std::ios::binary);
        if (!original_in.is_open()) {
            return Result<BuildResult>::failure(
                "Cannot open original archive during rebuild.",
                plan.original_archive_path.string());
        }
    }

    std::unordered_set<std::string> seen_names;
    seen_names.reserve(plan.entries.size());

    std::vector<PlannedEntryLayout> layouts;
    layouts.reserve(plan.entries.size());

    std::uint64_t cursor_sector = first_data_sector_u64;
    for (std::size_t i = 0; i < plan.entries.size(); ++i) {
        const auto& spec = plan.entries[i];
        const auto fn_val = validate_entry_filename(spec.name);
        if (!fn_val.valid) {
            return Result<BuildResult>::failure(
                "Invalid entry filename in rebuild plan.",
                spec.name + ": " + fn_val.reason);
        }

        const std::string lower = to_ascii_lower(spec.name);
        if (!seen_names.insert(lower).second) {
            return Result<BuildResult>::failure(
                "Duplicate entry filename in rebuild plan.",
                spec.name);
        }

        if (cursor_sector > std::numeric_limits<std::uint32_t>::max()) {
            return Result<BuildResult>::failure("Archive sector offset exceeded 32-bit limit.");
        }

        PlannedEntryLayout layout;
        layout.offset_sectors = static_cast<std::uint32_t>(cursor_sector);
        layout.raw_name = spec.preserve_raw_name ? spec.raw_name : encode_raw_filename(spec.name);

        if (spec.source_kind == BuildSourceKind::ExternalFile) {
            if (!std::filesystem::exists(spec.external_path, ec) ||
                !std::filesystem::is_regular_file(spec.external_path, ec)) {
                return Result<BuildResult>::failure(
                    "External staged file does not exist or is not a regular file.",
                    spec.external_path.string());
            }
            const std::uint64_t ext_size = std::filesystem::file_size(spec.external_path, ec);
            if (ec) {
                return Result<BuildResult>::failure(
                    "Failed to query staged file size.",
                    spec.external_path.string());
            }
            const auto sectors_opt = bytes_to_sectors_checked(ext_size);
            if (!sectors_opt.has_value()) {
                return Result<BuildResult>::failure(
                    "File exceeds maximum VER2 entry size (134,215,680 bytes / 65,535 sectors).",
                    spec.name);
            }
            layout.streaming_size = *sectors_opt;
            layout.archive_size = 0U;
            layout.effective_sectors = *sectors_opt;
            layout.payload_bytes_to_copy = ext_size;
        } else {
            const std::uint16_t eff_sectors = spec.preserve_size_metadata
                ? (spec.preserved_archive_size != 0U ? spec.preserved_archive_size : spec.preserved_streaming_size)
                : static_cast<std::uint16_t>(sectors_needed_u64(spec.original_size_bytes));
            const std::uint64_t req_bytes = static_cast<std::uint64_t>(eff_sectors) * kSectorSize;
            if (spec.original_offset_bytes > original_size ||
                req_bytes > original_size - spec.original_offset_bytes) {
                return Result<BuildResult>::failure(
                    "Original archive entry payload is out of bounds and cannot be safely copied.",
                    spec.name);
            }
            if (spec.preserve_size_metadata) {
                layout.streaming_size = spec.preserved_streaming_size;
                layout.archive_size = spec.preserved_archive_size;
            } else {
                layout.streaming_size = eff_sectors;
                layout.archive_size = 0U;
            }
            layout.effective_sectors = eff_sectors;
            layout.payload_bytes_to_copy = req_bytes;
        }

        const auto next_sector_opt = checked_add_u64(
            cursor_sector,
            static_cast<std::uint64_t>(layout.effective_sectors));
        if (!next_sector_opt.has_value() || *next_sector_opt > std::numeric_limits<std::uint32_t>::max()) {
            return Result<BuildResult>::failure(
                "Total archive sector count exceeds 32-bit VER2 limit.");
        }
        cursor_sector = *next_sector_opt;
        layouts.push_back(std::move(layout));
    }

    const auto total_bytes_opt = checked_mul_u64(cursor_sector, kSectorSize);
    if (!total_bytes_opt.has_value()) {
        return Result<BuildResult>::failure("Total output byte size overflow.");
    }

    std::ofstream out(temp_output_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return Result<BuildResult>::failure(
            "Failed to create temporary rebuild file.",
            temp_output_path.string());
    }

    // Write 8-byte VER2 header
    std::array<std::uint8_t, kHeaderSize> header{};
    std::copy_n(kVer2Magic.begin(), 4U, header.begin());
    write_u32_le(std::span<std::uint8_t, 4>(header.data() + 4U, 4U), entry_count);
    out.write(reinterpret_cast<const char*>(header.data()), static_cast<std::streamsize>(header.size()));
    if (!out) {
        out.close();
        remove_file_quietly(temp_output_path);
        return Result<BuildResult>::failure("Failed to write IMG header.");
    }

    // Write 32-byte directory entries
    std::array<std::uint8_t, kDirectoryEntrySize> raw_dir{};
    for (const auto& layout : layouts) {
        write_u32_le(std::span<std::uint8_t, 4>(raw_dir.data(), 4U), layout.offset_sectors);
        write_u16_le(std::span<std::uint8_t, 2>(raw_dir.data() + 4U, 2U), layout.streaming_size);
        write_u16_le(std::span<std::uint8_t, 2>(raw_dir.data() + 6U, 2U), layout.archive_size);
        std::memcpy(raw_dir.data() + 8U, layout.raw_name.data(), kMaxFilenameBytes);

        out.write(reinterpret_cast<const char*>(raw_dir.data()), static_cast<std::streamsize>(raw_dir.size()));
        if (!out) {
            out.close();
            remove_file_quietly(temp_output_path);
            return Result<BuildResult>::failure("Failed to write IMG directory entry.");
        }
    }

    // Pad directory area to 2048-byte sector boundary (for 0 entries, pads to 2048 bytes total)
    const std::uint64_t dir_padded_bytes = first_data_sector_u64 * kSectorSize;
    if (dir_padded_bytes > header_and_dir_bytes) {
        const std::size_t pad_bytes = static_cast<std::size_t>(dir_padded_bytes - header_and_dir_bytes);
        std::vector<char> zeros(pad_bytes, 0);
        out.write(zeros.data(), static_cast<std::streamsize>(zeros.size()));
        if (!out) {
            out.close();
            remove_file_quietly(temp_output_path);
            return Result<BuildResult>::failure("Failed to write directory sector padding.");
        }
    }

    // Stream entry payloads with 256 KiB buffer and compute SHA-256 of sector-aligned written bytes
    std::vector<std::uint8_t> copy_buf(262144U);
    std::array<char, kSectorSize> sector_zero_pad{};

    for (std::size_t i = 0; i < plan.entries.size(); ++i) {
        if (options.cancel_flag && options.cancel_flag->load(std::memory_order_relaxed)) {
            out.close();
            remove_file_quietly(temp_output_path);
            return Result<BuildResult>::failure("Rebuild canceled by user.");
        }

        const auto& spec = plan.entries[i];
        auto& layout = layouts[i];
        if (options.progress_cb) {
            options.progress_cb(i + 1U, plan.entries.size(), spec.name);
        }

        const std::uint64_t total_sector_bytes = static_cast<std::uint64_t>(layout.effective_sectors) * kSectorSize;
        if (total_sector_bytes == 0ULL) {
            layout.expected_padded_sha256 = Sha256::hash_bytes_hex({});
            continue;
        }

        Sha256 sha;
        std::ifstream ext_in;
        std::ifstream* src_stream = &original_in;

        if (spec.source_kind == BuildSourceKind::ExternalFile) {
            ext_in.open(spec.external_path, std::ios::binary);
            if (!ext_in.is_open()) {
                out.close();
                remove_file_quietly(temp_output_path);
                return Result<BuildResult>::failure(
                    "Failed to open staged external file during rebuild.",
                    spec.external_path.string());
            }
            src_stream = &ext_in;
        } else {
            original_in.clear();
            original_in.seekg(static_cast<std::streamoff>(spec.original_offset_bytes), std::ios::beg);
            if (!original_in) {
                out.close();
                remove_file_quietly(temp_output_path);
                return Result<BuildResult>::failure(
                    "Failed to seek original archive payload during rebuild.",
                    spec.name);
            }
        }

        std::uint64_t remaining = layout.payload_bytes_to_copy;
        while (remaining > 0ULL) {
            const std::size_t chunk = static_cast<std::size_t>(
                std::min<std::uint64_t>(remaining, copy_buf.size()));
            src_stream->read(reinterpret_cast<char*>(copy_buf.data()), static_cast<std::streamsize>(chunk));
            if (!(*src_stream) || static_cast<std::size_t>(src_stream->gcount()) != chunk) {
                out.close();
                remove_file_quietly(temp_output_path);
                return Result<BuildResult>::failure(
                    "Unexpected EOF or read error while copying payload during rebuild.",
                    spec.name);
            }
            out.write(reinterpret_cast<const char*>(copy_buf.data()), static_cast<std::streamsize>(chunk));
            if (!out) {
                out.close();
                remove_file_quietly(temp_output_path);
                return Result<BuildResult>::failure(
                    "Write error while writing payload during rebuild.",
                    spec.name);
            }
            sha.update(std::span<const std::uint8_t>(copy_buf.data(), chunk));
            remaining -= chunk;
        }

        if (total_sector_bytes > layout.payload_bytes_to_copy) {
            const std::size_t pad = static_cast<std::size_t>(total_sector_bytes - layout.payload_bytes_to_copy);
            out.write(sector_zero_pad.data(), static_cast<std::streamsize>(pad));
            if (!out) {
                out.close();
                remove_file_quietly(temp_output_path);
                return Result<BuildResult>::failure(
                    "Write error while padding entry to 2048-byte sector boundary.",
                    spec.name);
            }
            sha.update(std::span<const std::uint8_t>(
                reinterpret_cast<const std::uint8_t*>(sector_zero_pad.data()),
                pad));
        }

        layout.expected_padded_sha256 = sha.finalize_hex();
    }

    out.flush();
    if (!out) {
        out.close();
        remove_file_quietly(temp_output_path);
        return Result<BuildResult>::failure("Failed to flush rebuilt archive to disk.");
    }
    out.close();

    auto verify_res = verify_built_archive(temp_output_path, layouts);
    if (!verify_res.ok()) {
        remove_file_quietly(temp_output_path);
        return Result<BuildResult>::failure(
            verify_res.error->message,
            verify_res.error->detail);
    }

    BuildResult result;
    result.output_path = temp_output_path;
    result.entry_count = entry_count;
    result.total_output_bytes = *total_bytes_opt;
    result.verified = true;
    return Result<BuildResult>::success(std::move(result));
}

} // namespace imgstudio::core
