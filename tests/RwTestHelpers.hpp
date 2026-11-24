#pragma once

#include "core/rw/DffTypes.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace imgstudio::tests {

inline constexpr std::uint32_t kRwSaLibraryId = 0x1803FFFFU; // RW 3.6.0.3 (GTA SA)

std::vector<std::uint8_t> wrap_rw_chunk(
    std::uint32_t type,
    const std::vector<std::uint8_t>& payload,
    std::uint32_t library_id = kRwSaLibraryId);

struct SyntheticDffSpec {
    std::string frame_name = "chassis";
    std::string texture_name = "body_tex";
    std::string mask_name;
    bool use_tristrip_binmesh = false;
    bool include_normals = true;
    bool native_geometry_flag = false;
};

std::vector<std::uint8_t> build_synthetic_dff(const SyntheticDffSpec& spec);

struct SyntheticTxdTextureSpec {
    std::string name;
    std::string mask_name;
    std::uint32_t platform_id = 9U; // PCD3D9
    std::uint16_t width = 4U;
    std::uint16_t height = 4U;
    std::uint8_t mipmap_count = 1U;
    // format_kind: 0=BGRA8888, 1=DXT1, 2=DXT3, 3=DXT5, 4=PAL8, 5=BGR565, 6=UNKNOWN_PLATFORM
    int format_kind = 0;
    bool has_alpha = true;
};

std::vector<std::uint8_t> build_synthetic_txd(const std::vector<SyntheticTxdTextureSpec>& textures);

} // namespace imgstudio::tests
