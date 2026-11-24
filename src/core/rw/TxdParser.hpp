#pragma once

#include "core/ImgTypes.hpp"
#include "core/rw/TxdTypes.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>

namespace imgstudio::core::rw {

class TxdParser {
public:
    [[nodiscard]] static Result<TxdDictionary> parse_bytes(
        std::span<const std::uint8_t> bytes,
        std::string source_name = {});

    [[nodiscard]] static Result<TxdDictionary> parse_file(
        const std::filesystem::path& path);
};

} // namespace imgstudio::core::rw
