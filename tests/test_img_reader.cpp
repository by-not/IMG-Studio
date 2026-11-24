#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.hpp"
#include "core/ImgArchive.hpp"
#include "core/ImgReader.hpp"

#include <vector>

using namespace imgstudio::core;
using namespace imgstudio::tests;

TEST_CASE("Read valid synthetic VER2 archive", "[reader]") {
    TempDirGuard tmp("reader_valid");
    const auto img_path = tmp.path() / "valid.img";

    write_synthetic_img(img_path, {
        {"infernus.dff", make_pattern_bytes(4096, 1)},
        {"infernus.txd", make_pattern_bytes(5000, 2)},
        {"empty.col", {}}
    });

    auto res = ImgArchive::open(img_path);
    REQUIRE(res.ok());
    const auto& arch = *res.value;
    REQUIRE(arch.entries().size() == 3);
    REQUIRE(arch.entries()[0].name == "infernus.dff");
    REQUIRE(arch.entries()[0].extension == ".dff");
    REQUIRE(arch.entries()[0].category == FileCategory::DffModel);
    REQUIRE(arch.entries()[0].offset_sectors == 1);
    REQUIRE(arch.entries()[0].streaming_size == 2);
    REQUIRE(arch.entries()[0].archive_size == 0);

    REQUIRE(arch.entries()[1].name == "infernus.txd");
    REQUIRE(arch.entries()[1].offset_sectors == 3);
    REQUIRE(arch.entries()[1].streaming_size == 3); // ceil(5000/2048)

    REQUIRE(arch.entries()[2].name == "empty.col");
    REQUIRE(arch.entries()[2].streaming_size == 0);
    REQUIRE(arch.validation_report().is_valid_structure());
}

TEST_CASE("Read empty VER2 archive (0 entries)", "[reader]") {
    TempDirGuard tmp("reader_empty");
    const auto img_path = tmp.path() / "empty.img";
    write_synthetic_img(img_path, {});

    auto res = ImgArchive::open(img_path);
    REQUIRE(res.ok());
    REQUIRE(res.value->entries().empty());
    REQUIRE(res.value->file_size_bytes() == kSectorSize);
    REQUIRE(res.value->validation_report().is_valid_structure());
}

TEST_CASE("Preserve nonzero archive_size and raw metadata on read", "[reader]") {
    TempDirGuard tmp("reader_archive_size");
    const auto img_path = tmp.path() / "arch_size.img";

    write_synthetic_img(img_path, {
        {"custom.dat", make_pattern_bytes(4096, 7), 1, 2, true}
    });

    auto res = ImgArchive::open(img_path);
    REQUIRE(res.ok());
    const auto& e = res.value->entries()[0];
    REQUIRE(e.streaming_size == 1);
    REQUIRE(e.archive_size == 2);
    REQUIRE(e.effective_sectors() == 2);
    REQUIRE(e.effective_bytes() == 4096);
}

TEST_CASE("Reject missing file and handle truncated header safely", "[reader]") {
    TempDirGuard tmp("reader_invalid");
    REQUIRE(!ImgReader::read(tmp.path() / "missing.img").ok());

    const auto tiny = write_raw_file(tmp.path() / "tiny.img", {'V', 'E', 'R'});
    auto tiny_res = ImgArchive::open(tiny);
    REQUIRE(tiny_res.ok());
    REQUIRE(tiny_res.value->validation_report().has_errors());
    REQUIRE(tiny_res.value->validation_report().diagnostics[0].code == DiagnosticCode::TruncatedHeader);
}

TEST_CASE("Prevent giant OOM reservation when declared count exceeds file size", "[reader]") {
    TempDirGuard tmp("reader_oom_guard");
    std::vector<std::uint8_t> raw = {'V', 'E', 'R', '2', 0xFF, 0xFF, 0xFF, 0x7F};
    const auto bad = write_raw_file(tmp.path() / "bad_count.img", raw);

    auto res = ImgArchive::open(bad);
    REQUIRE(res.ok());
    REQUIRE(res.value->data().directory_truncated);
    REQUIRE(res.value->entries().empty());
    REQUIRE(res.value->validation_report().has_errors());
}
