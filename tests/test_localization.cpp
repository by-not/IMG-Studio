#include <catch2/catch_test_macros.hpp>

#include "TestHelpers.hpp"
#include "app/AppSettings.hpp"
#include "app/Localization.hpp"
#include "platform/Platform.hpp"

#include <nlohmann/json.hpp>

#include <fstream>
#include <regex>
#include <set>
#include <string>
#include <vector>

using namespace imgstudio::app;
using namespace imgstudio::tests;

namespace {

std::set<std::string> extract_placeholders(const std::string& text) {
    static const std::regex kPhRe(R"(\{([a-zA-Z0-9_]+)\})");
    std::set<std::string> out;
    auto begin = std::sregex_iterator(text.begin(), text.end(), kPhRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        out.insert((*it)[1].str());
    }
    return out;
}

std::filesystem::path find_locales_dir() {
    auto resolved = imgstudio::platform::resolve_asset_path("locales");
    if (std::filesystem::exists(resolved / "en.json")) {
        return resolved;
    }
    return {};
}

} // namespace

TEST_CASE("LocalizationManager loads real EN/ES locales and ignores template.json", "[app][localization]") {
    const auto locales_dir = find_locales_dir();
    REQUIRE_FALSE(locales_dir.empty());

    LocalizationManager loc;
    const auto res = loc.load_from_directory(locales_dir);
    REQUIRE(res.ok());
    REQUIRE(loc.load_diagnostics().empty());

    const auto available = loc.available_locales();
    REQUIRE(available.size() >= 2U);
    REQUIRE(available[0].code == "en");
    REQUIRE(available[0].native_name == "English");

    bool has_es = false;
    bool has_template = false;
    for (const auto& m : available) {
        if (m.code == "es") {
            has_es = true;
            REQUIRE(m.native_name == "Español");
            REQUIRE(m.english_name == "Spanish");
        }
        if (m.code == "xx" || m.code == "template") {
            has_template = true;
        }
    }
    REQUIRE(has_es);
    REQUIRE_FALSE(has_template);
}

TEST_CASE("LocalizationManager live switching, fallback, missing diagnostics, and UTF-8", "[app][localization]") {
    const auto locales_dir = find_locales_dir();
    REQUIRE_FALSE(locales_dir.empty());

    LocalizationManager loc;
    REQUIRE(loc.load_from_directory(locales_dir).ok());

    REQUIRE(loc.set_language("en"));
    REQUIRE(loc.current_language() == "en");
    REQUIRE(loc.tr("menu.file.open") == "Open Archive...");
    REQUIRE(loc.tr("common.yes") == "Yes");

    REQUIRE(loc.set_language("es"));
    REQUIRE(loc.current_language() == "es");
    REQUIRE(loc.tr("common.yes") == "Sí");
    REQUIRE(loc.tr("menu.file.settings") != "Settings...");

    // Switch back cleanly without restart
    REQUIRE(loc.set_language("en"));
    REQUIRE(loc.current_language() == "en");
    REQUIRE(loc.tr("common.yes") == "Yes");

    // Unsupported language falls back cleanly to English
    REQUIRE_FALSE(loc.set_language("zz"));
    REQUIRE(loc.current_language() == "en");
    REQUIRE(loc.tr("menu.file.open") == "Open Archive...");

    // Missing key triggers fallback/key return and records diagnostic
    loc.clear_diagnostics();
    const std::string& missing = loc.tr("nonexistent.sample.key");
    REQUIRE(missing == "[nonexistent.sample.key]");
    const auto recorded = loc.missing_keys();
    REQUIRE(recorded.size() == 1U);
    REQUIRE(recorded[0] == "en:nonexistent.sample.key");
}

TEST_CASE("LocalizationManager parameter substitution and reordered arguments", "[app][localization]") {
    const std::string simple = LocalizationManager::substitute_placeholders(
        "Added: {added}, Replaced: {replaced}, Renamed: {renamed}",
        {{"added", "3"}, {"replaced", "1"}, {"renamed", "2"}});
    REQUIRE(simple == "Added: 3, Replaced: 1, Renamed: 2");

    // Spanish or translated locales can reorder parameters freely
    const std::string reordered = LocalizationManager::substitute_placeholders(
        "Mostrando {visible} de {total} entradas ({visible} activas)",
        {{"total", "16316"}, {"visible", "42"}});
    REQUIRE(reordered == "Mostrando 42 de 16316 entradas (42 activas)");
}

TEST_CASE("LocalizationManager handles missing directory and corrupt JSON without crashing", "[app][localization]") {
    TempDirGuard tmp("loc_faults");
    LocalizationManager loc;
    loc.set_builtin_fallback_strings({{"common.cancel", "Cancel"}});

    // Missing directory returns error without crashing, built-in fallback still works
    const auto missing_res = loc.load_from_directory(tmp.path() / "does_not_exist");
    REQUIRE_FALSE(missing_res.ok());
    REQUIRE(loc.tr("common.cancel") == "Cancel");

    // Corrupt JSON + valid override in a temp locale folder
    const auto loc_dir = tmp.path() / "locales";
    std::filesystem::create_directories(loc_dir);
    {
        std::ofstream bad(loc_dir / "broken.json", std::ios::binary);
        bad << "{ this is invalid json";
    }
    {
        std::ofstream good(loc_dir / "en.json", std::ios::binary);
        good << R"({
            "_meta": {"code": "en", "native_name": "English", "english_name": "English"},
            "strings": {"menu.file": "File", "common.cancel": "Cancel"}
        })";
    }
    {
        std::ofstream partial(loc_dir / "fr.json", std::ios::binary);
        partial << R"({
            "_meta": {"code": "fr", "native_name": "Français", "english_name": "French"},
            "strings": {"menu.file": "Fichier"}
        })";
    }

    const auto ok_res = loc.load_from_directory(loc_dir);
    REQUIRE(ok_res.ok());
    REQUIRE(loc.load_diagnostics().size() == 1U);
    REQUIRE(loc.load_diagnostics()[0].filename == "broken.json");

    // Partial locale resolves translated key and falls back to English for missing keys
    REQUIRE(loc.set_language("fr"));
    REQUIRE(loc.tr("menu.file") == "Fichier");
    REQUIRE(loc.tr("common.cancel") == "Cancel");
}

TEST_CASE("AppSettings persists explicit and auto language selection", "[app][settings][localization]") {
    TempDirGuard tmp("loc_settings");
    const auto cfg_path = tmp.path() / "settings.json";

    AppSettings s;
    s.language = "es";
    REQUIRE(AppSettingsStore::save(s, cfg_path).ok());

    const auto reloaded = AppSettingsStore::load_or_default(cfg_path);
    REQUIRE(reloaded.language == "es");

    s.language = "auto";
    REQUIRE(AppSettingsStore::save(s, cfg_path).ok());
    const auto auto_reloaded = AppSettingsStore::load_or_default(cfg_path);
    REQUIRE(auto_reloaded.language == "auto");
}

TEST_CASE("EN and ES locale JSON files maintain 100 percent key and placeholder parity", "[app][localization]") {
    const auto locales_dir = find_locales_dir();
    REQUIRE_FALSE(locales_dir.empty());

    nlohmann::json en_json;
    nlohmann::json es_json;
    {
        std::ifstream f_en(locales_dir / "en.json", std::ios::binary);
        REQUIRE(f_en.is_open());
        en_json = nlohmann::json::parse(f_en);
    }
    {
        std::ifstream f_es(locales_dir / "es.json", std::ios::binary);
        REQUIRE(f_es.is_open());
        es_json = nlohmann::json::parse(f_es);
    }

    const auto& en_s = en_json.at("strings");
    const auto& es_s = es_json.at("strings");
    REQUIRE(en_s.size() == es_s.size());

    for (const auto& [k, en_val] : en_s.items()) {
        REQUIRE(es_s.contains(k));
        const std::string en_str = en_val.get<std::string>();
        const std::string es_str = es_s.at(k).get<std::string>();
        REQUIRE_FALSE(en_str.empty());
        REQUIRE_FALSE(es_str.empty());
        REQUIRE(extract_placeholders(en_str) == extract_placeholders(es_str));
    }
}
