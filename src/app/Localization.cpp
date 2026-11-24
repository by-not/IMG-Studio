#include "app/Localization.hpp"
#include "core/ImgFilename.hpp"
#include "platform/Platform.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

namespace imgstudio::app {

LocalizationManager& Localization::instance() {
    static LocalizationManager mgr;
    return mgr;
}

void LocalizationManager::set_builtin_fallback_strings(
    std::unordered_map<std::string, std::string> strings) {
    std::lock_guard lock(mutex_);
    builtin_en_ = std::move(strings);
}

core::Result<void> LocalizationManager::load_from_directory(const std::filesystem::path& locales_dir) {
    std::lock_guard lock(mutex_);
    locales_.clear();
    load_diagnostics_.clear();
    missing_keys_.clear();
    missing_cache_.clear();

    std::error_code ec;
    if (!std::filesystem::exists(locales_dir, ec) || !std::filesystem::is_directory(locales_dir, ec)) {
        load_diagnostics_.push_back(LocaleLoadDiagnostic{
            platform::to_utf8(locales_dir),
            "Locales directory does not exist."
        });
        return core::Result<void>::failure("Locales directory does not exist.", platform::to_utf8(locales_dir));
    }

    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(locales_dir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        const auto ext = core::to_ascii_lower(entry.path().extension().string());
        if (ext != ".json") continue;
        const auto stem = core::to_ascii_lower(entry.path().stem().string());
        if (stem == "template") continue;
        files.push_back(entry.path());
    }
    std::sort(files.begin(), files.end());

    for (const auto& file : files) {
        const std::string fname = platform::to_utf8(file.filename());
        std::ifstream in(file, std::ios::binary);
        if (!in.is_open()) {
            load_diagnostics_.push_back(LocaleLoadDiagnostic{fname, "Failed to open locale file."});
            continue;
        }
        try {
            nlohmann::json j;
            in >> j;
            if (!j.is_object() || !j.contains("strings") || !j["strings"].is_object()) {
                load_diagnostics_.push_back(LocaleLoadDiagnostic{fname, "Missing top-level 'strings' object."});
                continue;
            }

            LoadedLocale loc;
            loc.meta.code = core::to_ascii_lower(file.stem().string());
            if (j.contains("_meta") && j["_meta"].is_object()) {
                const auto& m = j["_meta"];
                if (m.contains("code") && m["code"].is_string() && !m["code"].get<std::string>().empty()) {
                    loc.meta.code = core::to_ascii_lower(m["code"].get<std::string>());
                }
                loc.meta.native_name = m.value("native_name", loc.meta.code);
                loc.meta.english_name = m.value("english_name", loc.meta.code);
            } else {
                loc.meta.native_name = loc.meta.code;
                loc.meta.english_name = loc.meta.code;
            }

            for (auto it = j["strings"].begin(); it != j["strings"].end(); ++it) {
                if (it.value().is_string()) {
                    loc.strings.emplace(it.key(), it.value().get<std::string>());
                } else {
                    load_diagnostics_.push_back(LocaleLoadDiagnostic{
                        fname,
                        "Non-string translation value for key: " + it.key()
                    });
                }
            }

            locales_[loc.meta.code] = std::move(loc);
        } catch (const std::exception& ex) {
            load_diagnostics_.push_back(LocaleLoadDiagnostic{
                fname,
                std::string("Invalid JSON: ") + ex.what()
            });
        }
    }

    if (locales_.find(active_lang_) == locales_.end()) {
        active_lang_ = locales_.find("en") != locales_.end() ? "en"
            : (locales_.empty() ? "en" : locales_.begin()->first);
    }

    return locales_.empty()
        ? core::Result<void>::failure("No valid locale files could be loaded.", platform::to_utf8(locales_dir))
        : core::Result<void>::success();
}

bool LocalizationManager::set_language(const std::string& requested_code) {
    std::lock_guard lock(mutex_);
    std::string clean = core::to_ascii_lower(requested_code);
    if (clean.empty()) {
        clean = "auto";
    }
    requested_lang_ = clean;

    std::string target = clean;
    if (clean == "auto") {
        target = core::to_ascii_lower(platform::detect_system_language());
    }

    if (locales_.find(target) != locales_.end()) {
        active_lang_ = target;
        return true;
    }

    // Try prefix match (e.g. "es-mx" -> "es")
    if (target.size() > 2U) {
        const std::string short_code = target.substr(0, 2U);
        if (locales_.find(short_code) != locales_.end()) {
            active_lang_ = short_code;
            return true;
        }
    }

    active_lang_ = "en";
    return clean == "auto" || clean == "en";
}

std::string LocalizationManager::current_language() const {
    std::lock_guard lock(mutex_);
    return active_lang_;
}

std::string LocalizationManager::requested_language() const {
    std::lock_guard lock(mutex_);
    return requested_lang_;
}

std::vector<LocaleMetadata> LocalizationManager::available_locales() const {
    std::lock_guard lock(mutex_);
    std::vector<LocaleMetadata> out;
    out.reserve(locales_.size());
    for (const auto& [code, loc] : locales_) {
        out.push_back(loc.meta);
    }
    std::sort(out.begin(), out.end(), [](const LocaleMetadata& a, const LocaleMetadata& b) {
        if (a.code == "en") return true;
        if (b.code == "en") return false;
        return a.code < b.code;
    });
    return out;
}

const std::string& LocalizationManager::tr(std::string_view key) const {
    std::lock_guard lock(mutex_);
    const std::string k(key);

    if (const auto lit = locales_.find(active_lang_); lit != locales_.end()) {
        if (const auto sit = lit->second.strings.find(k); sit != lit->second.strings.end()) {
            return sit->second;
        }
    }

    // Fallback to English locale and record missing key diagnostic if active_lang_ != "en"
    if (active_lang_ != "en") {
        missing_keys_.insert(active_lang_ + ":" + k);
        if (const auto en_it = locales_.find("en"); en_it != locales_.end()) {
            if (const auto sit = en_it->second.strings.find(k); sit != en_it->second.strings.end()) {
                return sit->second;
            }
        }
    }

    if (const auto bit = builtin_en_.find(k); bit != builtin_en_.end()) {
        return bit->second;
    }

    missing_keys_.insert(active_lang_ + ":" + k);
    auto [cit, inserted] = missing_cache_.emplace(k, "[" + k + "]");
    (void)inserted;
    return cit->second;
}

const char* LocalizationManager::tr_c(std::string_view key) const {
    return tr(key).c_str();
}

std::string LocalizationManager::substitute_placeholders(
    std::string_view pattern,
    std::initializer_list<std::pair<std::string_view, std::string>> args) {
    std::string out;
    out.reserve(pattern.size() + 32U);

    std::size_t i = 0;
    while (i < pattern.size()) {
        if (pattern[i] == '{') {
            const std::size_t close = pattern.find('}', i + 1U);
            if (close != std::string_view::npos) {
                const std::string_view token = pattern.substr(i + 1U, close - i - 1U);
                bool replaced = false;
                for (const auto& [name, val] : args) {
                    if (name == token) {
                        out.append(val);
                        replaced = true;
                        break;
                    }
                }
                if (replaced) {
                    i = close + 1U;
                    continue;
                }
            }
        }
        out.push_back(pattern[i]);
        ++i;
    }
    return out;
}

std::string LocalizationManager::trf(
    std::string_view key,
    std::initializer_list<std::pair<std::string_view, std::string>> args) const {
    const std::string pattern = tr(key);
    return substitute_placeholders(pattern, args);
}

std::vector<std::string> LocalizationManager::missing_keys() const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> out(missing_keys_.begin(), missing_keys_.end());
    std::sort(out.begin(), out.end());
    return out;
}

std::vector<LocaleLoadDiagnostic> LocalizationManager::load_diagnostics() const {
    std::lock_guard lock(mutex_);
    return load_diagnostics_;
}

void LocalizationManager::clear_diagnostics() {
    std::lock_guard lock(mutex_);
    missing_keys_.clear();
    load_diagnostics_.clear();
}

} // namespace imgstudio::app
