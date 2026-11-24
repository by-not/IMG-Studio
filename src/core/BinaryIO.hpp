#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace imgstudio::core {

[[nodiscard]] constexpr std::uint16_t read_u16_le(std::span<const std::uint8_t, 2> bytes) noexcept {
    return static_cast<std::uint16_t>(bytes[0]) |
           static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8U);
}

[[nodiscard]] constexpr std::uint32_t read_u32_le(std::span<const std::uint8_t, 4> bytes) noexcept {
    return static_cast<std::uint32_t>(bytes[0]) |
           (static_cast<std::uint32_t>(bytes[1]) << 8U) |
           (static_cast<std::uint32_t>(bytes[2]) << 16U) |
           (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

constexpr void write_u16_le(std::span<std::uint8_t, 2> out, std::uint16_t value) noexcept {
    out[0] = static_cast<std::uint8_t>(value & 0xFFU);
    out[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
}

constexpr void write_u32_le(std::span<const std::uint8_t, 4> /*unused*/) = delete;

constexpr void write_u32_le(std::span<std::uint8_t, 4> out, std::uint32_t value) noexcept {
    out[0] = static_cast<std::uint8_t>(value & 0xFFU);
    out[1] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
    out[2] = static_cast<std::uint8_t>((value >> 16U) & 0xFFU);
    out[3] = static_cast<std::uint8_t>((value >> 24U) & 0xFFU);
}

[[nodiscard]] constexpr std::optional<std::uint64_t> checked_add_u64(
    std::uint64_t a, std::uint64_t b) noexcept {
    if (a > UINT64_MAX - b) {
        return std::nullopt;
    }
    return a + b;
}

[[nodiscard]] constexpr std::optional<std::uint64_t> checked_mul_u64(
    std::uint64_t a, std::uint64_t b) noexcept {
    if (a == 0ULL || b == 0ULL) {
        return 0ULL;
    }
    if (a > UINT64_MAX / b) {
        return std::nullopt;
    }
    return a * b;
}

[[nodiscard]] std::optional<std::uint16_t> bytes_to_sectors_checked(std::uint64_t byte_size) noexcept;
[[nodiscard]] std::uint64_t sectors_needed_u64(std::uint64_t byte_size) noexcept;

class Sha256 {
public:
    Sha256() noexcept;
    void update(std::span<const std::uint8_t> data) noexcept;
    void finalize(std::array<std::uint8_t, 32>& out_digest) noexcept;
    [[nodiscard]] std::string finalize_hex();

    [[nodiscard]] static std::string hash_bytes_hex(std::span<const std::uint8_t> data);

private:
    void transform(const std::uint8_t block[64]) noexcept;

    std::array<std::uint32_t, 8> state_{};
    std::array<std::uint8_t, 64> buffer_{};
    std::uint64_t total_bits_ = 0;
    std::size_t buffer_len_ = 0;
    bool finalized_ = false;
};

} // namespace imgstudio::core
