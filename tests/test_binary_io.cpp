#include <catch2/catch_test_macros.hpp>

#include "core/BinaryIO.hpp"
#include "core/ImgFilename.hpp"

#include <array>
#include <cstdint>
#include <limits>
#include <vector>

using namespace imgstudio::core;

TEST_CASE("Explicit little-endian read and write round-trip", "[binary_io]") {
    std::array<std::uint8_t, 2> b16{};
    write_u16_le(b16, 0xABCDU);
    REQUIRE(b16[0] == 0xCDU);
    REQUIRE(b16[1] == 0xABU);
    REQUIRE(read_u16_le(b16) == 0xABCDU);

    std::array<std::uint8_t, 4> b32{};
    write_u32_le(b32, 0x12345678U);
    REQUIRE(b32[0] == 0x78U);
    REQUIRE(b32[1] == 0x56U);
    REQUIRE(b32[2] == 0x34U);
    REQUIRE(b32[3] == 0x12U);
    REQUIRE(read_u32_le(b32) == 0x12345678U);
}

TEST_CASE("Checked arithmetic and sector rounding", "[binary_io]") {
    REQUIRE(checked_add_u64(10, 20) == 30);
    REQUIRE(!checked_add_u64(std::numeric_limits<std::uint64_t>::max(), 1).has_value());

    REQUIRE(checked_mul_u64(0, std::numeric_limits<std::uint64_t>::max()) == 0);
    REQUIRE(!checked_mul_u64(std::numeric_limits<std::uint64_t>::max() / 2 + 1, 2).has_value());

    REQUIRE(sectors_needed_u64(0) == 0);
    REQUIRE(sectors_needed_u64(1) == 1);
    REQUIRE(sectors_needed_u64(2048) == 1);
    REQUIRE(sectors_needed_u64(2049) == 2);

    REQUIRE(bytes_to_sectors_checked(0) == 0);
    REQUIRE(bytes_to_sectors_checked(kMaxEntryPayloadBytes) == kMaxEntrySectors);
    REQUIRE(!bytes_to_sectors_checked(kMaxEntryPayloadBytes + 1ULL).has_value());
}

TEST_CASE("SHA-256 known test vector", "[binary_io]") {
    const std::string abc = "abc";
    const auto hash = Sha256::hash_bytes_hex(
        std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(abc.data()), abc.size()));
    REQUIRE(hash == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");

    const auto empty_hash = Sha256::hash_bytes_hex({});
    REQUIRE(empty_hash == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST_CASE("VER2 filename validation and safe export paths", "[filename]") {
    REQUIRE(validate_entry_filename("sultan.dff").valid);
    REQUIRE(validate_entry_filename("1234567890123456789.txd").valid); // 23 chars
    REQUIRE(!validate_entry_filename("12345678901234567890.txd").valid); // 24 chars
    REQUIRE(!validate_entry_filename("").valid);
    REQUIRE(!validate_entry_filename("../evil.dff").valid);
    REQUIRE(!validate_entry_filename("dir/file.dff").valid);
    REQUIRE(!validate_entry_filename("CON.dff").valid);
    REQUIRE(!validate_entry_filename("m" "\xc3" "\xb3" "d.dff").valid);

    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "imgstudio_fn_test";
    REQUIRE(build_safe_export_path(dir, "infernus.dff", false).ok());
    REQUIRE(!build_safe_export_path(dir, "../escape.dff", false).ok());
    auto sanitized = build_safe_export_path(dir, "CON.dff", true);
    REQUIRE(sanitized.ok());
    REQUIRE(sanitized.value->filename().string() == "_CON.dff");
}
