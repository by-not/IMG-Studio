#pragma once

#include "core/ImgTypes.hpp"
#include "core/rw/TxdTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace imgstudio::core::rw {

[[nodiscard]] std::size_t compute_mip_byte_size(
    TxdPixelFormat fmt,
    std::uint16_t width,
    std::uint16_t height,
    std::uint8_t depth);

[[nodiscard]] Result<std::vector<std::uint8_t>> decode_txd_mip_to_rgba8(
    TxdPixelFormat fmt,
    std::uint16_t width,
    std::uint16_t height,
    std::uint8_t depth,
    bool has_alpha,
    std::span<const std::uint8_t> raw_pixels,
    std::span<const std::uint8_t> palette_rgba);

} // namespace imgstudio::core::rw
