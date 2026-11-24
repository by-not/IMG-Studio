#include "core/ImgArchive.hpp"
#include "core/ImgFilename.hpp"
#include "core/ImgValidator.hpp"

namespace imgstudio::core {

Result<ImgArchive> ImgArchive::open(
    const std::filesystem::path& path,
    const ValidationOptions& val_opts) {
    auto read_res = ImgReader::read(path);
    if (!read_res.ok()) {
        return Result<ImgArchive>::failure(read_res.error->message, read_res.error->detail);
    }

    ImgArchive archive;
    archive.data_ = std::move(*read_res.value);
    archive.report_ = ImgValidator::validate(archive.data_, val_opts);
    return Result<ImgArchive>::success(std::move(archive));
}

ValidationReport ImgArchive::revalidate(const ValidationOptions& val_opts) {
    report_ = ImgValidator::validate(data_, val_opts);
    return report_;
}

std::optional<std::uint32_t> ImgArchive::find_index_case_insensitive(std::string_view name) const {
    const std::string needle = to_ascii_lower(name);
    for (const auto& entry : data_.entries) {
        if (to_ascii_lower(entry.name) == needle) {
            return entry.index;
        }
    }
    return std::nullopt;
}

} // namespace imgstudio::core
