#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.hpp"
#include "core/BinaryIO.hpp"
#include "core/ImgArchive.hpp"
#include "core/ImgValidator.hpp"

#include <algorithm>
#include <cstring>

using namespace imgstudio::core;
using namespace imgstudio::tests;

TEST_CASE("Validator detects invalid signature", "[validator]") {
    TempDirGuard tmp("val_sig");
    const auto bad = write_raw_file(tmp.path() / "bad_sig.img", {'B', 'A', 'D', '!', 0, 0, 0, 0});
    auto res = ImgArchive::open(bad);
    REQUIRE(res.ok());
    REQUIRE(res.value->validation_report().has_errors());
    REQUIRE(res.value->validation_report().diagnostics[0].code == DiagnosticCode::InvalidSignature);
}

TEST_CASE("Validator detects out-of-bounds, overlapping entries, and directory overlap", "[validator]") {
    TempDirGuard tmp("val_overlap");
    const auto p = tmp.path() / "corrupt.img";

    // Build manually a 2-entry IMG with overlapping payloads and out-of-bounds
    std::vector<std::uint8_t> raw(4096, 0);
    std::copy_n(kVer2Magic.begin(), 4U, raw.begin());
    write_u32_le(std::span<std::uint8_t, 4>(raw.data() + 4U, 4U), 3U);

    // Entry 0: starts at sector 0 (overlaps header/directory!), size 1 sector
    write_u32_le(std::span<std::uint8_t, 4>(raw.data() + 8U, 4U), 0U);
    write_u16_le(std::span<std::uint8_t, 2>(raw.data() + 12U, 2U), 1U);
    std::memcpy(raw.data() + 16U, "bad_dir.dff", 11U);

    // Entry 1: starts at sector 1, size 2 sectors (extends to byte 6144 > 4096)
    write_u32_le(std::span<std::uint8_t, 4>(raw.data() + 40U, 4U), 1U);
    write_u16_le(std::span<std::uint8_t, 2>(raw.data() + 44U, 2U), 2U);
    std::memcpy(raw.data() + 48U, "oob_a.txd", 9U);

    // Entry 2: starts at sector 1 (overlaps Entry 1), duplicate name case-insensitively
    write_u32_le(std::span<std::uint8_t, 4>(raw.data() + 72U, 4U), 1U);
    write_u16_le(std::span<std::uint8_t, 2>(raw.data() + 76U, 2U), 1U);
    std::memcpy(raw.data() + 80U, "OOB_A.TXD", 9U);

    write_raw_file(p, raw);

    ValidationOptions vopts;
    vopts.verify_payload_readability = true;
    auto res = ImgArchive::open(p, vopts);
    REQUIRE(res.ok());
    const auto& rep = res.value->validation_report();
    REQUIRE(rep.has_errors());

    bool has_dir_overlap = false;
    bool has_oob = false;
    bool has_prev_overlap = false;
    bool has_dup = false;
    for (const auto& d : rep.diagnostics) {
        if (d.code == DiagnosticCode::EntryOverlapsDirectory) has_dir_overlap = true;
        if (d.code == DiagnosticCode::EntryOutOfBounds) has_oob = true;
        if (d.code == DiagnosticCode::EntryOverlapsPrevious) has_prev_overlap = true;
        if (d.code == DiagnosticCode::DuplicateEntryName) has_dup = true;
    }
    REQUIRE(has_dir_overlap);
    REQUIRE(has_oob);
    REQUIRE(has_prev_overlap);
    REQUIRE(has_dup);
}

TEST_CASE("Validator detects missing null terminator and trailing garbage", "[validator]") {
    TempDirGuard tmp("val_names");
    const auto p = tmp.path() / "names.img";

    std::vector<std::uint8_t> raw(4096, 0);
    std::copy_n(kVer2Magic.begin(), 4U, raw.begin());
    write_u32_le(std::span<std::uint8_t, 4>(raw.data() + 4U, 4U), 2U);

    // Entry 0: 24 non-null chars -> MissingNullTerminator
    write_u32_le(std::span<std::uint8_t, 4>(raw.data() + 8U, 4U), 1U);
    write_u16_le(std::span<std::uint8_t, 2>(raw.data() + 12U, 2U), 1U);
    std::memset(raw.data() + 16U, 'A', 24U);

    // Entry 1: valid name with non-zero trailing byte after null -> Info TrailingGarbageAfterNull
    write_u32_le(std::span<std::uint8_t, 4>(raw.data() + 40U, 4U), 1U);
    write_u16_le(std::span<std::uint8_t, 2>(raw.data() + 44U, 2U), 1U);
    std::memcpy(raw.data() + 48U, "ok.dff", 7U);
    raw[48U + 15U] = 'Z';

    write_raw_file(p, raw);

    auto res = ImgArchive::open(p);
    REQUIRE(res.ok());
    const auto& rep = res.value->validation_report();

    bool saw_missing_null = false;
    bool saw_trailing_garbage = false;
    for (const auto& d : rep.diagnostics) {
        if (d.code == DiagnosticCode::MissingNullTerminator) saw_missing_null = true;
        if (d.code == DiagnosticCode::TrailingGarbageAfterNull && d.severity == DiagnosticSeverity::Info) {
            saw_trailing_garbage = true;
        }
    }
    REQUIRE(saw_missing_null);
    REQUIRE(saw_trailing_garbage);
}
