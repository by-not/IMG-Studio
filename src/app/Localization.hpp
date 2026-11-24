#pragma once

#include "core/ImgTypes.hpp"

#include <filesystem>
#include <initializer_list>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace imgstudio::app {

struct LocaleMetadata {
    std::string code;
    std::string native_name;
    std::string english_name;
};

struct LocaleLoadDiagnostic {
    std::string filename;
    std::string message;
};

class LocalizationManager {
public:
    core::Result<void> load_from_directory(const std::filesystem::path& locales_dir);

    void set_builtin_fallback_strings(std::unordered_map<std::string, std::string> strings);

    bool set_language(const std::string& requested_code);

    [[nodiscard]] std::string current_language() const;
    [[nodiscard]] std::string requested_language() const;
    [[nodiscard]] std::vector<LocaleMetadata> available_locales() const;

    [[nodiscard]] const std::string& tr(std::string_view key) const;
    [[nodiscard]] const char* tr_c(std::string_view key) const;
    [[nodiscard]] std::string trf(
        std::string_view key,
        std::initializer_list<std::pair<std::string_view, std::string>> args) const;

    [[nodiscard]] std::vector<std::string> missing_keys() const;
    [[nodiscard]] std::vector<LocaleLoadDiagnostic> load_diagnostics() const;
    void clear_diagnostics();

    [[nodiscard]] static std::string substitute_placeholders(
        std::string_view pattern,
        std::initializer_list<std::pair<std::string_view, std::string>> args);

private:
    struct LoadedLocale {
        LocaleMetadata meta;
        std::unordered_map<std::string, std::string> strings;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, LoadedLocale> locales_;
    std::unordered_map<std::string, std::string> builtin_en_;
    std::string requested_lang_ = "auto";
    std::string active_lang_ = "en";
    mutable std::unordered_set<std::string> missing_keys_;
    mutable std::unordered_map<std::string, std::string> missing_cache_;
    std::vector<LocaleLoadDiagnostic> load_diagnostics_;
};

class Localization {
public:
    [[nodiscard]] static LocalizationManager& instance();
};

[[nodiscard]] inline const std::string& tr(std::string_view key) {
    return Localization::instance().tr(key);
}

[[nodiscard]] inline const char* tr_c(std::string_view key) {
    return Localization::instance().tr_c(key);
}

[[nodiscard]] inline std::string trf(
    std::string_view key,
    std::initializer_list<std::pair<std::string_view, std::string>> args) {
    return Localization::instance().trf(key, args);
}

} // namespace imgstudio::app
