#pragma once

#include "core/ImgTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace imgstudio::tests {

struct SyntheticEntrySpec {
    std::string name;
    std::vector<std::uint8_t> payload;
    std::uint16_t override_streaming_size = 0;
    std::uint16_t override_archive_size = 0;
    bool use_override_sizes = false;
};

class TempDirGuard {
public:
    explicit TempDirGuard(const std::string& label);
    ~TempDirGuard();

    TempDirGuard(const TempDirGuard&) = delete;
    TempDirGuard& operator=(const TempDirGuard&) = delete;

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return dir_; }

private:
    std::filesystem::path dir_;
};

std::filesystem::path write_synthetic_img(
    const std::filesystem::path& dest,
    const std::vector<SyntheticEntrySpec>& entries);

std::filesystem::path write_raw_file(
    const std::filesystem::path& dest,
    const std::vector<std::uint8_t>& bytes);

std::vector<std::uint8_t> read_raw_file(const std::filesystem::path& path);

std::vector<std::uint8_t> make_pattern_bytes(std::size_t size, std::uint8_t seed);

} // namespace imgstudio::tests
