#pragma once

#include "core/ImgTypes.hpp"
#include "core/rw/DffTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace imgstudio::core::rw {

class DffParser {
public:
    [[nodiscard]] static Result<DffModel> parse_bytes(
        std::span<const std::uint8_t> bytes,
        std::string source_name = {});

    [[nodiscard]] static Result<DffModel> parse_file(
        const std::filesystem::path& path);
};

} // namespace imgstudio::core::rw
