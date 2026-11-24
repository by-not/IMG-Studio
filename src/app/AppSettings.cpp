#include "app/AppSettings.hpp"
#include "platform/AtomicFileOps.hpp"
#include "platform/Platform.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>

namespace imgstudio::app {

void AppSettings::add_recent_file(const std::filesystem::path& path) {
    if (path.empty()) {
        return;
    }
    const std::string target_utf8 = platform::to_utf8(path);
    recent_files.erase(
        std::remove_if(recent_files.begin(), recent_files.end(), [&](const std::filesystem::path& p) {
            return platform::to_utf8(p) == target_utf8;
        }),
        recent_files.end());
    recent_files.insert(recent_files.begin(), path);
    if (recent_files.size() > max_recent_files) {
        recent_files.resize(max_recent_files);
    }
}

const char* duplicate_policy_label(DuplicateImportPolicy policy) noexcept {
    switch (policy) {
        case DuplicateImportPolicy::Ask: return "Ask every time";
        case DuplicateImportPolicy::Replace: return "Replace existing";
        case DuplicateImportPolicy::Skip: return "Skip duplicates";
        case DuplicateImportPolicy::AutoRename: return "Auto-rename incoming";
    }
    return "Ask every time";
}

AppSettings AppSettingsStore::load_or_default(const std::filesystem::path& settings_file) {
    AppSettings s;
    s.backup_directory = platform::get_system_paths().default_backup_dir;

    std::error_code ec;
    if (!std::filesystem::exists(settings_file, ec)) {
        return s;
    }

    std::ifstream in(settings_file);
    if (!in.is_open()) {
        return s;
    }

    try {
        nlohmann::json j;
        in >> j;

        if (j.contains("backup_directory") && j["backup_directory"].is_string()) {
            const auto dir = platform::from_utf8(j["backup_directory"].get<std::string>());
            if (!dir.empty()) {
                s.backup_directory = dir;
            }
        }
        if (j.contains("auto_backup_before_commit") && j["auto_backup_before_commit"].is_boolean()) {
            s.auto_backup_before_commit = j["auto_backup_before_commit"].get<bool>();
        }
        if (j.contains("duplicate_policy") && j["duplicate_policy"].is_number_integer()) {
            const int v = j["duplicate_policy"].get<int>();
            if (v >= 0 && v <= 3) {
                s.duplicate_policy = static_cast<DuplicateImportPolicy>(v);
            }
        }
        if (j.contains("show_deleted_entries") && j["show_deleted_entries"].is_boolean()) {
            s.show_deleted_entries = j["show_deleted_entries"].get<bool>();
        }
        if (j.contains("verify_payloads_on_open") && j["verify_payloads_on_open"].is_boolean()) {
            s.verify_payloads_on_open = j["verify_payloads_on_open"].get<bool>();
        }
        if (j.contains("ui_scale") && j["ui_scale"].is_number()) {
            s.ui_scale = std::clamp(j["ui_scale"].get<float>(), 0.8f, 2.0f);
        }
        if (j.contains("sidebar_width") && j["sidebar_width"].is_number()) {
            s.sidebar_width = std::clamp(j["sidebar_width"].get<float>(), 200.0f, 450.0f);
        }
        if (j.contains("inspector_width") && j["inspector_width"].is_number()) {
            s.inspector_width = std::clamp(j["inspector_width"].get<float>(), 240.0f, 520.0f);
        }
        if (j.contains("show_sidebar") && j["show_sidebar"].is_boolean()) {
            s.show_sidebar = j["show_sidebar"].get<bool>();
        }
        if (j.contains("show_inspector") && j["show_inspector"].is_boolean()) {
            s.show_inspector = j["show_inspector"].get<bool>();
        }
        if (j.contains("recent_files") && j["recent_files"].is_array()) {
            s.recent_files.clear();
            for (const auto& item : j["recent_files"]) {
                if (item.is_string() && s.recent_files.size() < s.max_recent_files) {
                    const auto p = platform::from_utf8(item.get<std::string>());
                    if (!p.empty()) {
                        s.recent_files.push_back(p);
                    }
                }
            }
        }
        if (j.contains("language") && j["language"].is_string()) {
            const auto lang = j["language"].get<std::string>();
            if (!lang.empty()) {
                s.language = lang;
            }
        }
    } catch (...) {
        // Return clean defaults if settings file is corrupted
        return AppSettings{
            platform::get_system_paths().default_backup_dir,
            true,
            DuplicateImportPolicy::Ask,
            true,
            false,
            1.0f,
            270.0f,
            340.0f,
            true,
            true,
            {},
            15U,
            "auto"
        };
    }

    return s;
}

core::Result<void> AppSettingsStore::save(
    const AppSettings& settings,
    const std::filesystem::path& settings_file) {
    std::error_code ec;
    const auto parent = settings_file.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return core::Result<void>::failure("Failed to create settings directory.", ec.message());
        }
    }

    nlohmann::json j;
    j["backup_directory"] = platform::to_utf8(settings.backup_directory);
    j["auto_backup_before_commit"] = settings.auto_backup_before_commit;
    j["duplicate_policy"] = static_cast<int>(settings.duplicate_policy);
    j["show_deleted_entries"] = settings.show_deleted_entries;
    j["verify_payloads_on_open"] = settings.verify_payloads_on_open;
    j["ui_scale"] = settings.ui_scale;
    j["sidebar_width"] = settings.sidebar_width;
    j["inspector_width"] = settings.inspector_width;
    j["show_sidebar"] = settings.show_sidebar;
    j["show_inspector"] = settings.show_inspector;
    j["language"] = settings.language;

    nlohmann::json recent = nlohmann::json::array();
    for (const auto& p : settings.recent_files) {
        recent.push_back(platform::to_utf8(p));
    }
    j["recent_files"] = std::move(recent);

    const auto tmp = platform::make_unique_temp_path(settings_file, ".json.tmp");
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out.is_open()) {
            return core::Result<void>::failure("Failed to write temporary settings file.", platform::to_utf8(tmp));
        }
        out << j.dump(2) << '\n';
        out.flush();
        if (!out) {
            out.close();
            std::filesystem::remove(tmp, ec);
            return core::Result<void>::failure("Failed to flush settings JSON.");
        }
    }

    auto rep = platform::atomic_replace_file(tmp, settings_file);
    if (!rep.ok()) {
        std::filesystem::remove(tmp, ec);
    }
    return rep;
}

} // namespace imgstudio::app
