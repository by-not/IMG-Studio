#include "core/BinaryIO.hpp"
#include "core/ImgTypes.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace imgstudio::core {

std::optional<std::uint16_t> bytes_to_sectors_checked(std::uint64_t byte_size) noexcept {
    if (byte_size == 0ULL) {
        return std::uint16_t{0};
    }
    const std::uint64_t sectors = sectors_needed_u64(byte_size);
    if (sectors > kMaxEntrySectors) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(sectors);
}

std::uint64_t sectors_needed_u64(std::uint64_t byte_size) noexcept {
    if (byte_size == 0ULL) {
        return 0ULL;
    }
    const std::uint64_t whole = byte_size / kSectorSize;
    const std::uint64_t rem = byte_size % kSectorSize;
    return rem == 0ULL ? whole : (whole + 1ULL);
}

namespace {

constexpr std::array<std::uint32_t, 64> kSha256K = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U
};

[[nodiscard]] constexpr std::uint32_t rotr(std::uint32_t x, std::uint32_t n) noexcept {
    return (x >> n) | (x << (32U - n));
}

} // namespace

Sha256::Sha256() noexcept {
    state_ = {
        0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
        0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U
    };
}

void Sha256::transform(const std::uint8_t block[64]) noexcept {
    std::uint32_t w[64]{};
    for (std::size_t i = 0; i < 16; ++i) {
        const std::size_t j = i * 4U;
        w[i] = (static_cast<std::uint32_t>(block[j]) << 24U) |
               (static_cast<std::uint32_t>(block[j + 1U]) << 16U) |
               (static_cast<std::uint32_t>(block[j + 2U]) << 8U) |
               static_cast<std::uint32_t>(block[j + 3U]);
    }
    for (std::size_t i = 16; i < 64; ++i) {
        const std::uint32_t s0 = rotr(w[i - 15], 7U) ^ rotr(w[i - 15], 18U) ^ (w[i - 15] >> 3U);
        const std::uint32_t s1 = rotr(w[i - 2], 17U) ^ rotr(w[i - 2], 19U) ^ (w[i - 2] >> 10U);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = state_[0];
    std::uint32_t b = state_[1];
    std::uint32_t c = state_[2];
    std::uint32_t d = state_[3];
    std::uint32_t e = state_[4];
    std::uint32_t f = state_[5];
    std::uint32_t g = state_[6];
    std::uint32_t h = state_[7];

    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t S1 = rotr(e, 6U) ^ rotr(e, 11U) ^ rotr(e, 25U);
        const std::uint32_t ch = (e & f) ^ ((~e) & g);
        const std::uint32_t temp1 = h + S1 + ch + kSha256K[i] + w[i];
        const std::uint32_t S0 = rotr(a, 2U) ^ rotr(a, 13U) ^ rotr(a, 22U);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t temp2 = S0 + maj;

        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    state_[0] += a;
    state_[1] += b;
    state_[2] += c;
    state_[3] += d;
    state_[4] += e;
    state_[5] += f;
    state_[6] += g;
    state_[7] += h;
}

void Sha256::update(std::span<const std::uint8_t> data) noexcept {
    if (finalized_ || data.empty()) {
        return;
    }

    std::size_t offset = 0;
    total_bits_ += static_cast<std::uint64_t>(data.size()) * 8ULL;

    if (buffer_len_ > 0) {
        const std::size_t space = 64U - buffer_len_;
        const std::size_t take = data.size() < space ? data.size() : space;
        std::memcpy(buffer_.data() + buffer_len_, data.data(), take);
        buffer_len_ += take;
        offset += take;
        if (buffer_len_ == 64U) {
            transform(buffer_.data());
            buffer_len_ = 0;
        }
    }

    while (offset + 64U <= data.size()) {
        transform(data.data() + offset);
        offset += 64U;
    }

    if (offset < data.size()) {
        const std::size_t rem = data.size() - offset;
        std::memcpy(buffer_.data(), data.data() + offset, rem);
        buffer_len_ = rem;
    }
}

void Sha256::finalize(std::array<std::uint8_t, 32>& out_digest) noexcept {
    if (!finalized_) {
        buffer_[buffer_len_++] = 0x80U;
        if (buffer_len_ > 56U) {
            while (buffer_len_ < 64U) {
                buffer_[buffer_len_++] = 0x00U;
            }
            transform(buffer_.data());
            buffer_len_ = 0;
        }
        while (buffer_len_ < 56U) {
            buffer_[buffer_len_++] = 0x00U;
        }
        for (int i = 7; i >= 0; --i) {
            buffer_[buffer_len_++] = static_cast<std::uint8_t>((total_bits_ >> (i * 8)) & 0xFFU);
        }
        transform(buffer_.data());
        finalized_ = true;
    }

    for (std::size_t i = 0; i < 8U; ++i) {
        out_digest[i * 4U + 0U] = static_cast<std::uint8_t>((state_[i] >> 24U) & 0xFFU);
        out_digest[i * 4U + 1U] = static_cast<std::uint8_t>((state_[i] >> 16U) & 0xFFU);
        out_digest[i * 4U + 2U] = static_cast<std::uint8_t>((state_[i] >> 8U) & 0xFFU);
        out_digest[i * 4U + 3U] = static_cast<std::uint8_t>(state_[i] & 0xFFU);
    }
}

std::string Sha256::finalize_hex() {
    std::array<std::uint8_t, 32> digest{};
    finalize(digest);
    static constexpr char kHex[] = "0123456789abcdef";
    std::string out;
    out.resize(64);
    for (std::size_t i = 0; i < 32U; ++i) {
        out[i * 2U] = kHex[(digest[i] >> 4U) & 0x0FU];
        out[i * 2U + 1U] = kHex[digest[i] & 0x0FU];
    }
    return out;
}

std::string Sha256::hash_bytes_hex(std::span<const std::uint8_t> data) {
    Sha256 sha;
    sha.update(data);
    return sha.finalize_hex();
}

} // namespace imgstudio::core
