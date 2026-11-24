#pragma once

#include "core/ImgTypes.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace imgstudio::core {

struct ImgArchiveData {
    std::filesystem::path source_path;
    std::uint64_t file_size_bytes = 0;
    std::array<std::uint8_t, 4> magic{};
    std::uint32_t declared_entry_count = 0;
    bool header_present = false;
    bool directory_truncated = false;
    std::vector<ImgEntry> entries;
};

class ImgReader {
public:
    [[nodiscard]] static Result<ImgArchiveData> read(const std::filesystem::path& path);
};

} // namespace imgstudio::core
