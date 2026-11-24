#include "TestHelpers.hpp"
#include "core/BinaryIO.hpp"
#include "core/ImgFilename.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace imgstudio::tests {

TempDirGuard::TempDirGuard(const std::string& label) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    dir_ = std::filesystem::temp_directory_path() / ("imgstudio_test_" + label + "_" + std::to_string(stamp));
    std::filesystem::create_directories(dir_);
}

TempDirGuard::~TempDirGuard() {
    std::error_code ec;
    std::filesystem::remove_all(dir_, ec);
}

std::filesystem::path write_synthetic_img(
    const std::filesystem::path& dest,
    const std::vector<SyntheticEntrySpec>& entries) {
    const std::uint32_t count = static_cast<std::uint32_t>(entries.size());
    const std::uint64_t header_and_dir = core::kHeaderSize + static_cast<std::uint64_t>(count) * core::kDirectoryEntrySize;
    const std::uint64_t first_data_sector = core::sectors_needed_u64(header_and_dir);

    std::ofstream out(dest, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to create synthetic IMG file");
    }

    std::array<std::uint8_t, core::kHeaderSize> hdr{};
    std::copy_n(core::kVer2Magic.begin(), 4U, hdr.begin());
    core::write_u32_le(std::span<std::uint8_t, 4>(hdr.data() + 4U, 4U), count);
    out.write(reinterpret_cast<const char*>(hdr.data()), static_cast<std::streamsize>(hdr.size()));

    std::uint64_t cursor_sector = first_data_sector;
    std::array<std::uint8_t, core::kDirectoryEntrySize> dir_entry{};
    for (const auto& e : entries) {
        const std::uint16_t computed_sectors = static_cast<std::uint16_t>(
            core::sectors_needed_u64(e.payload.size()));
        const std::uint16_t s_size = e.use_override_sizes ? e.override_streaming_size : computed_sectors;
        const std::uint16_t a_size = e.use_override_sizes ? e.override_archive_size : 0U;
        const std::uint16_t eff = a_size != 0U ? a_size : s_size;

        core::write_u32_le(std::span<std::uint8_t, 4>(dir_entry.data(), 4U), static_cast<std::uint32_t>(cursor_sector));
        core::write_u16_le(std::span<std::uint8_t, 2>(dir_entry.data() + 4U, 2U), s_size);
        core::write_u16_le(std::span<std::uint8_t, 2>(dir_entry.data() + 6U, 2U), a_size);
        const auto raw_name = core::encode_raw_filename(e.name);
        std::memcpy(dir_entry.data() + 8U, raw_name.data(), core::kMaxFilenameBytes);

        out.write(reinterpret_cast<const char*>(dir_entry.data()), static_cast<std::streamsize>(dir_entry.size()));
        cursor_sector += eff;
    }

    const std::uint64_t dir_padded = first_data_sector * core::kSectorSize;
    if (dir_padded > header_and_dir) {
        std::vector<char> pad(static_cast<std::size_t>(dir_padded - header_and_dir), 0);
        out.write(pad.data(), static_cast<std::streamsize>(pad.size()));
    }

    std::array<char, core::kSectorSize> zero_pad{};
    for (const auto& e : entries) {
        const std::uint16_t eff = e.use_override_sizes
            ? (e.override_archive_size != 0U ? e.override_archive_size : e.override_streaming_size)
            : static_cast<std::uint16_t>(core::sectors_needed_u64(e.payload.size()));
        const std::uint64_t total_sector_bytes = static_cast<std::uint64_t>(eff) * core::kSectorSize;
        const std::size_t to_write = static_cast<std::size_t>(
            std::min<std::uint64_t>(e.payload.size(), total_sector_bytes));
        if (to_write > 0U) {
            out.write(reinterpret_cast<const char*>(e.payload.data()), static_cast<std::streamsize>(to_write));
        }
        if (total_sector_bytes > to_write) {
            out.write(zero_pad.data(), static_cast<std::streamsize>(total_sector_bytes - to_write));
        }
    }

    out.flush();
    return dest;
}

std::filesystem::path write_raw_file(
    const std::filesystem::path& dest,
    const std::vector<std::uint8_t>& bytes) {
    std::ofstream out(dest, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("Failed to create raw file");
    }
    if (!bytes.empty()) {
        out.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    }
    out.flush();
    return dest;
}

std::vector<std::uint8_t> read_raw_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) {
        throw std::runtime_error("Failed to open raw file for reading");
    }
    in.seekg(0, std::ios::end);
    const std::size_t sz = static_cast<std::size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    std::vector<std::uint8_t> out(sz);
    if (sz > 0U) {
        in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(sz));
    }
    return out;
}

std::vector<std::uint8_t> make_pattern_bytes(std::size_t size, std::uint8_t seed) {
    std::vector<std::uint8_t> out(size);
    for (std::size_t i = 0; i < size; ++i) {
        out[i] = static_cast<std::uint8_t>((seed + (i * 31U) + (i >> 5U)) & 0xFFU);
    }
    return out;
}

} // namespace imgstudio::tests
