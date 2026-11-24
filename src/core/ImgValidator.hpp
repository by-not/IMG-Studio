#pragma once

#include "core/ImgReader.hpp"
#include "core/ImgTypes.hpp"

#include <atomic>
#include <filesystem>
#include <functional>

namespace imgstudio::core {

struct ValidationOptions {
    bool verify_payload_readability = false;
    std::function<void(std::size_t current, std::size_t total)> progress_cb;
    const std::atomic<bool>* cancel_flag = nullptr;
};

class ImgValidator {
public:
    [[nodiscard]] static ValidationReport validate(
        const ImgArchiveData& archive,
        const ValidationOptions& options = {});
};

} // namespace imgstudio::core
