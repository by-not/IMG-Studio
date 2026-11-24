#pragma once

#include "core/ImgTypes.hpp"

#include <filesystem>
#include <string>

namespace imgstudio::platform {

[[nodiscard]] core::Result<void> atomic_replace_file(
    const std::filesystem::path& temp_file,
    const std::filesystem::path& target_file);

[[nodiscard]] core::Result<void> copy_file_verified(
    const std::filesystem::path& source_file,
    const std::filesystem::path& target_file);

[[nodiscard]] std::filesystem::path make_unique_temp_path(
    const std::filesystem::path& target_file,
    const std::string& tag = ".tmp");

} // namespace imgstudio::platform
