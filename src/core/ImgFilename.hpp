#pragma once

#include "core/ImgTypes.hpp"

#include <array>
#include <filesystem>
#include <string>
#include <string_view>

namespace imgstudio::core {

struct FilenameValidationResult {
    bool valid = false;
    bool ascii_only = true;
    bool safe_for_windows_export = true;
    std::string reason;
};

[[nodiscard]] FilenameValidationResult validate_entry_filename(std::string_view name);
[[nodiscard]] std::string to_ascii_lower(std::string_view input);
[[nodiscard]] std::string extract_extension_lower(std::string_view filename);
[[nodiscard]] FileCategory classify_extension(std::string_view ext_lower) noexcept;
[[nodiscard]] bool is_windows_reserved_filename(std::string_view filename);

[[nodiscard]] std::array<char, kMaxFilenameBytes> encode_raw_filename(std::string_view name);

struct DecodedRawFilename {
    std::string name;
    bool has_null_terminator = true;
    bool has_trailing_garbage = false;
    bool ascii_only = true;
};

[[nodiscard]] DecodedRawFilename decode_raw_filename(const std::array<char, kMaxFilenameBytes>& raw);

[[nodiscard]] Result<std::filesystem::path> build_safe_export_path(
    const std::filesystem::path& target_dir,
    std::string_view entry_name,
    bool sanitize_unsafe = false);

} // namespace imgstudio::core
