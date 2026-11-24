#include "core/ImgExtractor.hpp"
#include "core/ImgFilename.hpp"

#include <algorithm>
#include <array>
#include <fstream>

namespace imgstudio::core {

namespace {

[[nodiscard]] Result<void> stream_copy_range(
    std::ifstream& in,
    std::uint64_t offset,
    std::uint64_t length,
    const std::filesystem::path& dest_file) {
    std::ofstream out(dest_file, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return Result<void>::failure("Failed to create output file.", dest_file.string());
    }

    if (length == 0ULL) {
        out.flush();
        return out ? Result<void>::success()
                   : Result<void>::failure("Failed to flush empty output file.", dest_file.string());
    }

    in.clear();
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!in) {
        return Result<void>::failure("Failed to seek to source payload offset.", dest_file.string());
    }

    std::array<char, 65536> buf{};
    std::uint64_t remaining = length;
    while (remaining > 0ULL) {
        const std::size_t chunk = static_cast<std::size_t>(
            std::min<std::uint64_t>(remaining, buf.size()));
        in.read(buf.data(), static_cast<std::streamsize>(chunk));
        if (!in || static_cast<std::size_t>(in.gcount()) != chunk) {
            return Result<void>::failure("Unexpected EOF or read error while reading payload.", dest_file.string());
        }
        out.write(buf.data(), static_cast<std::streamsize>(chunk));
        if (!out) {
            return Result<void>::failure("Write error while writing extracted file.", dest_file.string());
        }
        remaining -= chunk;
    }

    out.flush();
    if (!out) {
        return Result<void>::failure("Failed to flush extracted file to disk.", dest_file.string());
    }

    return Result<void>::success();
}

} // namespace

Result<void> ImgExtractor::extract_entry_to_file(
    const std::filesystem::path& archive_path,
    const ImgEntry& entry,
    const std::filesystem::path& dest_file,
    bool overwrite_existing) {
    std::error_code ec;
    if (!overwrite_existing && std::filesystem::exists(dest_file, ec)) {
        return Result<void>::failure("Destination file already exists.", dest_file.string());
    }

    const std::uint64_t file_size = std::filesystem::file_size(archive_path, ec);
    if (ec) {
        return Result<void>::failure("Failed to query archive size.", ec.message());
    }

    const std::uint64_t off = entry.offset_bytes();
    const std::uint64_t len = entry.effective_bytes();
    if (off > file_size || len > file_size - off) {
        return Result<void>::failure(
            "Entry bounds exceed archive file size.",
            entry.name);
    }

    std::ifstream in(archive_path, std::ios::binary);
    if (!in.is_open()) {
        return Result<void>::failure("Failed to open archive for extraction.", archive_path.string());
    }

    return stream_copy_range(in, off, len, dest_file);
}

Result<ExtractionSummary> ImgExtractor::extract_items(
    const std::filesystem::path& archive_path,
    const std::vector<ExtractionItem>& items,
    const std::filesystem::path& target_dir,
    const ExtractionOptions& options) {
    std::error_code ec;
    if (!std::filesystem::exists(target_dir, ec) || !std::filesystem::is_directory(target_dir, ec)) {
        return Result<ExtractionSummary>::failure(
            "Target extraction directory does not exist.",
            target_dir.string());
    }

    const bool needs_archive = std::any_of(items.begin(), items.end(), [](const ExtractionItem& item) {
        return !item.from_external_file;
    });

    std::uint64_t archive_size = 0;
    std::ifstream archive_in;
    if (needs_archive) {
        archive_size = std::filesystem::file_size(archive_path, ec);
        if (ec) {
            return Result<ExtractionSummary>::failure("Failed to query archive size.", ec.message());
        }
        archive_in.open(archive_path, std::ios::binary);
        if (!archive_in.is_open()) {
            return Result<ExtractionSummary>::failure(
                "Failed to open archive for extraction.",
                archive_path.string());
        }
    }

    ExtractionSummary summary;
    summary.requested = items.size();

    for (std::size_t i = 0; i < items.size(); ++i) {
        if (options.cancel_flag && options.cancel_flag->load(std::memory_order_relaxed)) {
            summary.canceled = true;
            break;
        }

        const auto& item = items[i];
        if (options.progress_cb) {
            options.progress_cb(i + 1U, items.size(), item.output_name);
        }

        auto path_res = build_safe_export_path(target_dir, item.output_name, options.sanitize_unsafe_names);
        if (!path_res.ok()) {
            summary.failures.push_back(ExtractionFailure{
                item.output_name,
                path_res.error->message + (path_res.error->detail.empty() ? "" : " (" + path_res.error->detail + ")")
            });
            continue;
        }

        const std::filesystem::path& dest_file = *path_res.value;
        if (!options.overwrite_existing && std::filesystem::exists(dest_file, ec)) {
            ++summary.skipped;
            continue;
        }

        if (item.from_external_file) {
            const std::uint64_t ext_size = std::filesystem::file_size(item.external_path, ec);
            if (ec) {
                summary.failures.push_back(ExtractionFailure{
                    item.output_name,
                    "External staged source file cannot be read: " + ec.message()
                });
                continue;
            }
            std::ifstream ext_in(item.external_path, std::ios::binary);
            if (!ext_in.is_open()) {
                summary.failures.push_back(ExtractionFailure{
                    item.output_name,
                    "Failed to open external staged file."
                });
                continue;
            }
            auto copy_res = stream_copy_range(ext_in, 0ULL, ext_size, dest_file);
            if (!copy_res.ok()) {
                summary.failures.push_back(ExtractionFailure{
                    item.output_name,
                    copy_res.error->message
                });
                continue;
            }
            ++summary.succeeded;
        } else {
            if (item.archive_offset_bytes > archive_size ||
                item.size_bytes > archive_size - item.archive_offset_bytes) {
                summary.failures.push_back(ExtractionFailure{
                    item.output_name,
                    "Entry payload extends beyond archive end of file."
                });
                continue;
            }
            auto copy_res = stream_copy_range(
                archive_in,
                item.archive_offset_bytes,
                item.size_bytes,
                dest_file);
            if (!copy_res.ok()) {
                summary.failures.push_back(ExtractionFailure{
                    item.output_name,
                    copy_res.error->message
                });
                continue;
            }
            ++summary.succeeded;
        }
    }

    return Result<ExtractionSummary>::success(std::move(summary));
}

} // namespace imgstudio::core
