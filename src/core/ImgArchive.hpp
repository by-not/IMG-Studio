#pragma once

#include "core/ImgReader.hpp"
#include "core/ImgTypes.hpp"
#include "core/ImgValidator.hpp"

#include <atomic>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace imgstudio::core {

class ImgArchive {
public:
    [[nodiscard]] static Result<ImgArchive> open(
        const std::filesystem::path& path,
        const ValidationOptions& val_opts = {});

    [[nodiscard]] const std::filesystem::path& path() const noexcept { return data_.source_path; }
    [[nodiscard]] const ImgArchiveData& data() const noexcept { return data_; }
    [[nodiscard]] const std::vector<ImgEntry>& entries() const noexcept { return data_.entries; }
    [[nodiscard]] std::uint64_t file_size_bytes() const noexcept { return data_.file_size_bytes; }
    [[nodiscard]] const ValidationReport& validation_report() const noexcept { return report_; }

    ValidationReport revalidate(const ValidationOptions& val_opts = {});
    void set_validation_report(ValidationReport report) { report_ = std::move(report); }

    [[nodiscard]] std::optional<std::uint32_t> find_index_case_insensitive(
        std::string_view name) const;

private:
    ImgArchiveData data_{};
    ValidationReport report_{};
};

} // namespace imgstudio::core
