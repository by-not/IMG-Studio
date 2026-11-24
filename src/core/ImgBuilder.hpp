#pragma once

#include "core/ImgTypes.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace imgstudio::core {

enum class BuildSourceKind : std::uint8_t {
    OriginalArchiveRange = 0,
    ExternalFile
};

struct BuildEntrySpec {
    std::string name;
    std::array<char, kMaxFilenameBytes> raw_name{};
    bool preserve_raw_name = false;

    BuildSourceKind source_kind = BuildSourceKind::OriginalArchiveRange;
    std::uint64_t original_offset_bytes = 0;
    std::uint64_t original_size_bytes = 0;
    std::filesystem::path external_path;

    std::uint16_t preserved_streaming_size = 0;
    std::uint16_t preserved_archive_size = 0;
    bool preserve_size_metadata = false;
};

struct RebuildPlan {
    std::filesystem::path original_archive_path;
    std::vector<BuildEntrySpec> entries;
};

struct BuildOptions {
    std::function<void(std::size_t current, std::size_t total, std::string_view entry_name)> progress_cb;
    const std::atomic<bool>* cancel_flag = nullptr;
};

struct BuildResult {
    std::filesystem::path output_path;
    std::uint32_t entry_count = 0;
    std::uint64_t total_output_bytes = 0;
    bool verified = false;
};

class ImgBuilder {
public:
    [[nodiscard]] static Result<BuildResult> build_to_file(
        const RebuildPlan& plan,
        const std::filesystem::path& temp_output_path,
        const BuildOptions& options = {});
};

} // namespace imgstudio::core
