#include "core/ImgReader.hpp"
#include "core/BinaryIO.hpp"
#include "core/ImgFilename.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace imgstudio::core {

Result<ImgArchiveData> ImgReader::read(const std::filesystem::path& path) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        return Result<ImgArchiveData>::failure(
            "Archive file does not exist.",
            path.string());
    }
    if (!std::filesystem::is_regular_file(path, ec)) {
        return Result<ImgArchiveData>::failure(
            "Path is not a regular file.",
            path.string());
    }

    const std::uint64_t file_size = std::filesystem::file_size(path, ec);
    if (ec) {
        return Result<ImgArchiveData>::failure(
            "Failed to query archive file size.",
            ec.message());
    }

    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        return Result<ImgArchiveData>::failure(
            "Failed to open archive for reading.",
            path.string());
    }

    ImgArchiveData data;
    data.source_path = path;
    data.file_size_bytes = file_size;

    if (file_size < kHeaderSize) {
        std::array<std::uint8_t, kHeaderSize> partial{};
        in.read(reinterpret_cast<char*>(partial.data()), static_cast<std::streamsize>(file_size));
        for (std::size_t i = 0; i < 4U && i < file_size; ++i) {
            data.magic[i] = partial[i];
        }
        data.header_present = false;
        return Result<ImgArchiveData>::success(std::move(data));
    }

    std::array<std::uint8_t, kHeaderSize> header_bytes{};
    in.read(reinterpret_cast<char*>(header_bytes.data()), static_cast<std::streamsize>(kHeaderSize));
    if (!in || static_cast<std::size_t>(in.gcount()) != kHeaderSize) {
        return Result<ImgArchiveData>::failure(
            "I/O error while reading IMG header.",
            path.string());
    }

    data.header_present = true;
    std::copy_n(header_bytes.begin(), 4U, data.magic.begin());
    data.declared_entry_count = read_u32_le(std::span<const std::uint8_t, 4>(header_bytes.data() + 4U, 4U));

    if (data.magic != kVer2Magic) {
        return Result<ImgArchiveData>::success(std::move(data));
    }

    const std::uint64_t available_after_header = file_size - kHeaderSize;
    const std::uint64_t max_possible_entries = available_after_header / kDirectoryEntrySize;
    const auto required_dir_bytes =
        checked_mul_u64(static_cast<std::uint64_t>(data.declared_entry_count), kDirectoryEntrySize);

    std::uint32_t entries_to_read = data.declared_entry_count;
    if (!required_dir_bytes.has_value() || *required_dir_bytes > available_after_header) {
        data.directory_truncated = true;
        entries_to_read = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(data.declared_entry_count, max_possible_entries));
    }

    data.entries.reserve(entries_to_read);

    constexpr std::uint32_t kBatchEntries = 1024U;
    std::vector<std::uint8_t> batch_buf(static_cast<std::size_t>(kBatchEntries) * kDirectoryEntrySize);

    std::uint32_t read_so_far = 0;
    while (read_so_far < entries_to_read) {
        const std::uint32_t chunk_entries = std::min<std::uint32_t>(kBatchEntries, entries_to_read - read_so_far);
        const std::size_t chunk_bytes = static_cast<std::size_t>(chunk_entries) * kDirectoryEntrySize;

        in.read(reinterpret_cast<char*>(batch_buf.data()), static_cast<std::streamsize>(chunk_bytes));
        if (!in || static_cast<std::size_t>(in.gcount()) != chunk_bytes) {
            data.directory_truncated = true;
            break;
        }

        for (std::uint32_t i = 0; i < chunk_entries; ++i) {
            const std::uint8_t* ptr = batch_buf.data() + static_cast<std::size_t>(i) * kDirectoryEntrySize;

            ImgEntry entry;
            entry.index = read_so_far + i;
            entry.offset_sectors = read_u32_le(std::span<const std::uint8_t, 4>(ptr, 4U));
            entry.streaming_size = read_u16_le(std::span<const std::uint8_t, 2>(ptr + 4U, 2U));
            entry.archive_size = read_u16_le(std::span<const std::uint8_t, 2>(ptr + 6U, 2U));
            std::memcpy(entry.raw_name.data(), ptr + 8U, kMaxFilenameBytes);

            const auto decoded = decode_raw_filename(entry.raw_name);
            entry.name = decoded.name;
            entry.has_null_terminator = decoded.has_null_terminator;
            entry.has_trailing_garbage = decoded.has_trailing_garbage;
            entry.extension = extract_extension_lower(entry.name);
            entry.category = classify_extension(entry.extension);

            data.entries.push_back(std::move(entry));
        }

        read_so_far += chunk_entries;
    }

    return Result<ImgArchiveData>::success(std::move(data));
}

} // namespace imgstudio::core
