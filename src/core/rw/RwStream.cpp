#include "core/rw/RwStream.hpp"
#include "core/BinaryIO.hpp"

#include <cstdio>
#include <cstring>

namespace imgstudio::core::rw {

std::uint32_t decode_rw_version(std::uint32_t library_id) noexcept {
    if ((library_id & 0xFFFF0000U) != 0U) {
        return (((library_id >> 14U) & 0x3FF00U) + 0x30000U) |
               ((library_id >> 16U) & 0x3FU);
    }
    return library_id << 8U;
}

std::string format_rw_version(std::uint32_t rw_version) {
    if (rw_version == 0U) {
        return "Unknown";
    }
    const unsigned major = (rw_version >> 16U) & 0xFFU;
    const unsigned minor = (rw_version >> 12U) & 0x0FU;
    const unsigned rev   = (rw_version >> 8U) & 0x0FU;
    const unsigned build = rw_version & 0xFFU;
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%u.%u.%u.%u", major, minor, rev, build);
    return buf;
}

bool RwCursor::skip(std::size_t bytes) noexcept {
    if (bytes > remaining()) {
        return false;
    }
    pos_ += bytes;
    return true;
}

bool RwCursor::read_u8(std::uint8_t& out) noexcept {
    if (remaining() < 1U) {
        return false;
    }
    out = data_[pos_++];
    return true;
}

bool RwCursor::read_u16(std::uint16_t& out) noexcept {
    if (remaining() < 2U) {
        return false;
    }
    out = read_u16_le(std::span<const std::uint8_t, 2>(data_.data() + pos_, 2U));
    pos_ += 2U;
    return true;
}

bool RwCursor::read_u32(std::uint32_t& out) noexcept {
    if (remaining() < 4U) {
        return false;
    }
    out = read_u32_le(std::span<const std::uint8_t, 4>(data_.data() + pos_, 4U));
    pos_ += 4U;
    return true;
}

bool RwCursor::read_i32(std::int32_t& out) noexcept {
    std::uint32_t raw = 0;
    if (!read_u32(raw)) {
        return false;
    }
    std::memcpy(&out, &raw, sizeof(out));
    return true;
}

bool RwCursor::read_f32(float& out) noexcept {
    std::uint32_t raw = 0;
    if (!read_u32(raw)) {
        return false;
    }
    std::memcpy(&out, &raw, sizeof(out));
    return true;
}

bool RwCursor::read_fixed_string(std::size_t field_len, std::string& out) {
    auto span_opt = read_span(field_len);
    if (!span_opt.has_value()) {
        return false;
    }
    std::size_t len = 0;
    while (len < field_len && (*span_opt)[len] != 0U) {
        ++len;
    }
    out.assign(reinterpret_cast<const char*>(span_opt->data()), len);
    return true;
}

std::optional<std::span<const std::uint8_t>> RwCursor::read_span(std::size_t len) noexcept {
    if (len > remaining()) {
        return std::nullopt;
    }
    auto sub = data_.subspan(pos_, len);
    pos_ += len;
    return sub;
}

std::optional<RwChunkHeader> RwCursor::read_chunk_header() noexcept {
    if (remaining() < kRwChunkHeaderSize) {
        return std::nullopt;
    }
    RwChunkHeader hdr;
    if (!read_u32(hdr.type) || !read_u32(hdr.size) || !read_u32(hdr.library_id)) {
        return std::nullopt;
    }
    hdr.rw_version = decode_rw_version(hdr.library_id);
    return hdr;
}

std::optional<RwCursor> RwCursor::read_chunk_payload(RwChunkHeader& out_header) noexcept {
    auto hdr_opt = read_chunk_header();
    if (!hdr_opt.has_value()) {
        return std::nullopt;
    }
    out_header = *hdr_opt;
    auto span_opt = read_span(static_cast<std::size_t>(out_header.size));
    if (!span_opt.has_value()) {
        return std::nullopt;
    }
    return RwCursor(*span_opt);
}

} // namespace imgstudio::core::rw
