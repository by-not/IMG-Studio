#include "core/ImgValidator.hpp"
#include "core/BinaryIO.hpp"
#include "core/ImgFilename.hpp"

#include <algorithm>
#include <array>
#include <fstream>
#include <unordered_map>
#include <vector>

namespace imgstudio::core {

namespace {

struct Extent {
    std::uint64_t start_byte = 0;
    std::uint64_t end_byte = 0;
    std::uint32_t entry_index = 0;
    std::string entry_name;
};

} // namespace

ValidationReport ImgValidator::validate(
    const ImgArchiveData& archive,
    const ValidationOptions& options) {
    ValidationReport report;
    report.metrics.file_size_bytes = archive.file_size_bytes;
    report.metrics.declared_entry_count = archive.declared_entry_count;
    report.metrics.parsed_entry_count = static_cast<std::uint32_t>(archive.entries.size());

    if (!archive.header_present) {
        report.add(ValidationDiagnostic{
            DiagnosticSeverity::Error,
            DiagnosticCode::TruncatedHeader,
            std::nullopt,
            {},
            "File is smaller than the required 8-byte VER2 header."
        });
        return report;
    }

    if (archive.magic != kVer2Magic) {
        report.add(ValidationDiagnostic{
            DiagnosticSeverity::Error,
            DiagnosticCode::InvalidSignature,
            std::nullopt,
            {},
            "Invalid IMG signature (expected 'VER2')."
        });
        return report;
    }

    if (archive.file_size_bytes > kGtaSa2GiBThreshold) {
        report.add(ValidationDiagnostic{
            DiagnosticSeverity::Warning,
            DiagnosticCode::LargeArchiveAbove2GiB,
            std::nullopt,
            {},
            "Archive exceeds 2 GiB; classic GTA San Andreas 32-bit signed offsets may fail in-game without limit adjusters."
        });
    }

    const auto dir_bytes_opt = checked_mul_u64(
        static_cast<std::uint64_t>(archive.declared_entry_count),
        kDirectoryEntrySize);
    if (!dir_bytes_opt.has_value()) {
        report.add(ValidationDiagnostic{
            DiagnosticSeverity::Error,
            DiagnosticCode::ArithmeticOverflow,
            std::nullopt,
            {},
            "Declared entry count overflows directory size calculation."
        });
        return report;
    }

    const auto total_dir_end_opt = checked_add_u64(kHeaderSize, *dir_bytes_opt);
    if (!total_dir_end_opt.has_value()) {
        report.add(ValidationDiagnostic{
            DiagnosticSeverity::Error,
            DiagnosticCode::ArithmeticOverflow,
            std::nullopt,
            {},
            "Header + directory size overflows 64-bit bounds."
        });
        return report;
    }

    const std::uint64_t dir_end_bytes = *total_dir_end_opt;
    const std::uint64_t dir_sectors = sectors_needed_u64(dir_end_bytes);
    report.metrics.directory_bytes = dir_end_bytes;
    report.metrics.directory_padded_bytes = dir_sectors * kSectorSize;

    if (archive.directory_truncated || dir_end_bytes > archive.file_size_bytes) {
        report.add(ValidationDiagnostic{
            DiagnosticSeverity::Error,
            DiagnosticCode::TruncatedDirectory,
            std::nullopt,
            {},
            "Directory table extends past end of file; declared entry count exceeds available directory bytes."
        });
    }

    std::unordered_map<std::string, std::uint32_t> seen_names;
    seen_names.reserve(archive.entries.size());

    std::vector<Extent> extents;
    extents.reserve(archive.entries.size());

    for (const auto& entry : archive.entries) {
        if (!entry.has_null_terminator) {
            report.add(ValidationDiagnostic{
                DiagnosticSeverity::Error,
                DiagnosticCode::MissingNullTerminator,
                entry.index,
                entry.name,
                "24-byte directory filename field lacks a null terminator."
            });
        } else if (entry.has_trailing_garbage) {
            report.add(ValidationDiagnostic{
                DiagnosticSeverity::Info,
                DiagnosticCode::TrailingGarbageAfterNull,
                entry.index,
                entry.name,
                "Directory filename buffer contains non-zero bytes after null terminator."
            });
        }

        const auto fn_val = validate_entry_filename(entry.name);
        if (!fn_val.valid) {
            report.add(ValidationDiagnostic{
                fn_val.ascii_only ? DiagnosticSeverity::Warning : DiagnosticSeverity::Error,
                fn_val.ascii_only ? DiagnosticCode::InvalidFilename : DiagnosticCode::NonAsciiFilename,
                entry.index,
                entry.name,
                fn_val.reason
            });
        }

        const std::string lower_name = to_ascii_lower(entry.name);
        if (!lower_name.empty()) {
            auto [it, inserted] = seen_names.emplace(lower_name, entry.index);
            if (!inserted) {
                ++report.metrics.duplicate_name_count;
                report.add(ValidationDiagnostic{
                    DiagnosticSeverity::Warning,
                    DiagnosticCode::DuplicateEntryName,
                    entry.index,
                    entry.name,
                    "Duplicate entry name (case-insensitive match with entry #" + std::to_string(it->second) + ")."
                });
            }
        }

        if (entry.archive_size != 0U) {
            ++report.metrics.entries_with_nonzero_archive_size;
            if (entry.streaming_size == 0U) {
                report.add(ValidationDiagnostic{
                    DiagnosticSeverity::Info,
                    DiagnosticCode::SuspiciousSizeMetadata,
                    entry.index,
                    entry.name,
                    "Entry has archive_size > 0 while streaming_size == 0 (uncommon metadata combination)."
                });
            } else if (entry.archive_size != entry.streaming_size) {
                report.add(ValidationDiagnostic{
                    DiagnosticSeverity::Info,
                    DiagnosticCode::SuspiciousSizeMetadata,
                    entry.index,
                    entry.name,
                    "archive_size differs from streaming_size; both fields are preserved."
                });
            }
        }

        const std::uint16_t eff_sectors = entry.effective_sectors();
        if (eff_sectors == 0U) {
            report.add(ValidationDiagnostic{
                DiagnosticSeverity::Warning,
                DiagnosticCode::ZeroSizeEntry,
                entry.index,
                entry.name,
                "Entry occupies 0 sectors."
            });
        }

        const auto start_opt = checked_mul_u64(static_cast<std::uint64_t>(entry.offset_sectors), kSectorSize);
        const auto len_opt = checked_mul_u64(static_cast<std::uint64_t>(eff_sectors), kSectorSize);
        if (!start_opt.has_value() || !len_opt.has_value()) {
            report.add(ValidationDiagnostic{
                DiagnosticSeverity::Error,
                DiagnosticCode::ArithmeticOverflow,
                entry.index,
                entry.name,
                "Entry offset or size calculation overflowed 64-bit integer."
            });
            continue;
        }

        const auto end_opt = checked_add_u64(*start_opt, *len_opt);
        if (!end_opt.has_value()) {
            report.add(ValidationDiagnostic{
                DiagnosticSeverity::Error,
                DiagnosticCode::ArithmeticOverflow,
                entry.index,
                entry.name,
                "Entry end offset overflowed 64-bit integer."
            });
            continue;
        }

        const std::uint64_t start_byte = *start_opt;
        const std::uint64_t end_byte = *end_opt;
        report.metrics.total_payload_bytes += *len_opt;

        if (eff_sectors > 0U && start_byte < dir_end_bytes) {
            report.add(ValidationDiagnostic{
                DiagnosticSeverity::Error,
                DiagnosticCode::EntryOverlapsDirectory,
                entry.index,
                entry.name,
                "Entry payload starts inside the IMG header/directory table."
            });
        } else if (eff_sectors > 0U && start_byte < report.metrics.directory_padded_bytes) {
            report.add(ValidationDiagnostic{
                DiagnosticSeverity::Warning,
                DiagnosticCode::EntryOverlapsDirectory,
                entry.index,
                entry.name,
                "Entry payload starts inside the padding sector of the directory table."
            });
        }

        if (end_byte > archive.file_size_bytes || start_byte > archive.file_size_bytes) {
            report.add(ValidationDiagnostic{
                DiagnosticSeverity::Error,
                DiagnosticCode::EntryOutOfBounds,
                entry.index,
                entry.name,
                "Entry payload extends beyond archive end of file."
            });
        }
        if (eff_sectors > 0U) {
            extents.push_back(Extent{start_byte, end_byte, entry.index, entry.name});
        }
    }

    std::sort(extents.begin(), extents.end(), [](const Extent& a, const Extent& b) {
        if (a.start_byte != b.start_byte) {
            return a.start_byte < b.start_byte;
        }
        return a.entry_index < b.entry_index;
    });

    std::uint64_t prev_end = report.metrics.directory_padded_bytes;
    for (std::size_t i = 0; i < extents.size(); ++i) {
        const auto& cur = extents[i];
        if (i > 0U) {
            const auto& prev = extents[i - 1U];
            if (cur.start_byte < prev.end_byte) {
                report.add(ValidationDiagnostic{
                    DiagnosticSeverity::Error,
                    DiagnosticCode::EntryOverlapsPrevious,
                    cur.entry_index,
                    cur.entry_name,
                    "Payload overlaps with entry #" + std::to_string(prev.entry_index) +
                        " (" + prev.entry_name + ")."
                });
            } else if (cur.start_byte > prev.end_byte && cur.start_byte <= archive.file_size_bytes) {
                report.metrics.wasted_gap_bytes += (cur.start_byte - prev.end_byte);
                report.metrics.is_fragmented = true;
            }
        } else if (cur.start_byte > prev_end && cur.start_byte <= archive.file_size_bytes) {
            report.metrics.wasted_gap_bytes += (cur.start_byte - prev_end);
            report.metrics.is_fragmented = true;
        }
        prev_end = std::min(std::max(prev_end, cur.end_byte), archive.file_size_bytes);
    }

    if (archive.file_size_bytes > prev_end) {
        report.metrics.wasted_gap_bytes += (archive.file_size_bytes - prev_end);
        if (!archive.entries.empty()) {
            report.metrics.is_fragmented = true;
        }
    }

    if (options.verify_payload_readability && !archive.entries.empty()) {
        std::ifstream in(archive.source_path, std::ios::binary);
        if (!in.is_open()) {
            report.add(ValidationDiagnostic{
                DiagnosticSeverity::Error,
                DiagnosticCode::IoFailure,
                std::nullopt,
                {},
                "Failed to open archive file for payload readability check."
            });
            return report;
        }

        std::array<char, 65536> buf{};
        const std::size_t total = archive.entries.size();
        for (std::size_t idx = 0; idx < total; ++idx) {
            if (options.cancel_flag && options.cancel_flag->load(std::memory_order_relaxed)) {
                break;
            }
            if (options.progress_cb) {
                options.progress_cb(idx + 1U, total);
            }

            const auto& entry = archive.entries[idx];
            const std::uint64_t len = entry.effective_bytes();
            const std::uint64_t off = entry.offset_bytes();
            if (len == 0ULL || off >= archive.file_size_bytes || off + len > archive.file_size_bytes) {
                continue;
            }

            in.clear();
            in.seekg(static_cast<std::streamoff>(off), std::ios::beg);
            if (!in) {
                report.add(ValidationDiagnostic{
                    DiagnosticSeverity::Error,
                    DiagnosticCode::UnreadablePayload,
                    entry.index,
                    entry.name,
                    "Seek failed while reading entry payload."
                });
                continue;
            }

            std::uint64_t remaining = len;
            while (remaining > 0ULL) {
                const std::size_t to_read = static_cast<std::size_t>(
                    std::min<std::uint64_t>(remaining, buf.size()));
                in.read(buf.data(), static_cast<std::streamsize>(to_read));
                if (!in || static_cast<std::size_t>(in.gcount()) != to_read) {
                    report.add(ValidationDiagnostic{
                        DiagnosticSeverity::Error,
                        DiagnosticCode::UnreadablePayload,
                        entry.index,
                        entry.name,
                        "Read error before reaching declared entry payload end."
                    });
                    break;
                }
                remaining -= to_read;
            }
        }
    }

    return report;
}

} // namespace imgstudio::core
