#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace imgstudio::core::rw {

inline constexpr std::uint32_t kRwStruct         = 0x0001U;
inline constexpr std::uint32_t kRwString         = 0x0002U;
inline constexpr std::uint32_t kRwExtension      = 0x0003U;
inline constexpr std::uint32_t kRwCamera         = 0x0005U;
inline constexpr std::uint32_t kRwTexture        = 0x0006U;
inline constexpr std::uint32_t kRwMaterial       = 0x0007U;
inline constexpr std::uint32_t kRwMatList        = 0x0008U;
inline constexpr std::uint32_t kRwAtomicSect     = 0x0009U;
inline constexpr std::uint32_t kRwPlaneSect      = 0x000AU;
inline constexpr std::uint32_t kRwWorld          = 0x000BU;
inline constexpr std::uint32_t kRwSpline         = 0x000CU;
inline constexpr std::uint32_t kRwMatrix         = 0x000DU;
inline constexpr std::uint32_t kRwFrameList      = 0x000EU;
inline constexpr std::uint32_t kRwGeometry       = 0x000FU;
inline constexpr std::uint32_t kRwClump          = 0x0010U;
inline constexpr std::uint32_t kRwLight          = 0x0012U;
inline constexpr std::uint32_t kRwUnicodeString  = 0x0013U;
inline constexpr std::uint32_t kRwAtomic         = 0x0014U;
inline constexpr std::uint32_t kRwTextureNative  = 0x0015U;
inline constexpr std::uint32_t kRwTexDictionary  = 0x0016U;
inline constexpr std::uint32_t kRwGeometryList   = 0x001AU;
inline constexpr std::uint32_t kRwSkinPlg        = 0x0116U;
inline constexpr std::uint32_t kRwBinMeshPlg     = 0x050EU;
inline constexpr std::uint32_t kRwFrameName      = 0x0253F2FEU;

inline constexpr std::size_t kRwChunkHeaderSize = 12U;

struct RwChunkHeader {
    std::uint32_t type = 0;
    std::uint32_t size = 0;
    std::uint32_t library_id = 0;
    std::uint32_t rw_version = 0;
};

[[nodiscard]] std::uint32_t decode_rw_version(std::uint32_t library_id) noexcept;
[[nodiscard]] std::string format_rw_version(std::uint32_t rw_version);

class RwCursor {
public:
    explicit RwCursor(std::span<const std::uint8_t> data) noexcept
        : data_(data) {}

    [[nodiscard]] std::size_t size() const noexcept { return data_.size(); }
    [[nodiscard]] std::size_t tell() const noexcept { return pos_; }
    [[nodiscard]] std::size_t remaining() const noexcept { return data_.size() - pos_; }
    [[nodiscard]] bool at_end() const noexcept { return pos_ >= data_.size(); }

    [[nodiscard]] bool skip(std::size_t bytes) noexcept;
    [[nodiscard]] bool read_u8(std::uint8_t& out) noexcept;
    [[nodiscard]] bool read_u16(std::uint16_t& out) noexcept;
    [[nodiscard]] bool read_i32(std::int32_t& out) noexcept;
    [[nodiscard]] bool read_u32(std::uint32_t& out) noexcept;
    [[nodiscard]] bool read_f32(float& out) noexcept;
    [[nodiscard]] bool read_fixed_string(std::size_t field_len, std::string& out);
    [[nodiscard]] std::optional<std::span<const std::uint8_t>> read_span(std::size_t len) noexcept;
    [[nodiscard]] std::optional<RwChunkHeader> read_chunk_header() noexcept;
    [[nodiscard]] std::optional<RwCursor> read_chunk_payload(RwChunkHeader& out_header) noexcept;

private:
    std::span<const std::uint8_t> data_;
    std::size_t pos_ = 0;
};

} // namespace imgstudio::core::rw
