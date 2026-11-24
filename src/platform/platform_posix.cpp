#include "platform/AtomicFileOps.hpp"
#include "platform/FileDialogs.hpp"
#include "platform/Platform.hpp"
#include "platform/WindowChrome.hpp"

#include <chrono>
#include <cstdlib>
#include <unistd.h>

namespace imgstudio::platform {

SystemPaths get_system_paths() {
    SystemPaths paths;
    paths.executable_dir = std::filesystem::current_path();
    const char* xdg = std::getenv("XDG_DATA_HOME");
    const char* home = std::getenv("HOME");
    if (xdg && *xdg) {
        paths.app_data_dir = std::filesystem::path(xdg) / "IMGStudio";
    } else if (home && *home) {
        paths.app_data_dir = std::filesystem::path(home) / ".local" / "share" / "IMGStudio";
    } else {
        paths.app_data_dir = std::filesystem::temp_directory_path() / "IMGStudio";
    }
    paths.default_backup_dir = paths.app_data_dir / "backups";
    paths.settings_file = paths.app_data_dir / "settings.json";
    paths.assets_dir = paths.executable_dir / "assets";
    return paths;
}

std::string to_utf8(const std::filesystem::path& p) {
    const auto u8 = p.u8string();
    return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

std::filesystem::path from_utf8(const std::string& utf8) {
    const std::u8string u8(reinterpret_cast<const char8_t*>(utf8.data()), utf8.size());
    return std::filesystem::path(u8);
}

std::filesystem::path resolve_asset_path(const std::string& relative_path) {
    return get_system_paths().assets_dir / from_utf8(relative_path);
}

bool reveal_in_file_manager(const std::filesystem::path& /*path*/) {
    return false;
}

bool open_directory_in_file_manager(const std::filesystem::path& /*dir*/) {
    return false;
}

std::string detect_system_language() {
    for (const char* env_var : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
        const char* val = std::getenv(env_var);
        if (val && *val) {
            std::string code;
            for (const char* p = val; *p; ++p) {
                const char ch = *p;
                if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
                    code.push_back((ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch + ('a' - 'A')) : ch);
                    if (code.size() == 2U) {
                        if (code == "c." || code == "po") {
                            break;
                        }
                        return code;
                    }
                } else {
                    break;
                }
            }
        }
    }
    return "en";
}

std::optional<std::filesystem::path> open_file_dialog(
    void* /*native_window_handle*/,
    const std::string& /*title*/,
    const std::vector<FileFilterSpec>& /*filters*/) {
    return std::nullopt;
}

std::vector<std::filesystem::path> open_files_dialog(
    void* /*native_window_handle*/,
    const std::string& /*title*/,
    const std::vector<FileFilterSpec>& /*filters*/) {
    return {};
}

std::optional<std::filesystem::path> save_file_dialog(
    void* /*native_window_handle*/,
    const std::string& /*title*/,
    const std::string& /*default_name*/,
    const std::vector<FileFilterSpec>& /*filters*/) {
    return std::nullopt;
}

std::optional<std::filesystem::path> pick_folder_dialog(
    void* /*native_window_handle*/,
    const std::string& /*title*/) {
    return std::nullopt;
}

core::Result<void> atomic_replace_file(
    const std::filesystem::path& temp_file,
    const std::filesystem::path& target_file) {
    std::error_code ec;
    if (!std::filesystem::exists(temp_file, ec)) {
        return core::Result<void>::failure("Temporary file does not exist.", to_utf8(temp_file));
    }
    std::filesystem::rename(temp_file, target_file, ec);
    if (ec) {
        return core::Result<void>::failure("Failed to rename temporary file to target.", ec.message());
    }
    return core::Result<void>::success();
}

core::Result<void> copy_file_verified(
    const std::filesystem::path& source_file,
    const std::filesystem::path& target_file) {
    std::error_code ec;
    const auto parent = target_file.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            return core::Result<void>::failure("Failed to create target directory.", ec.message());
        }
    }
    const auto tmp = make_unique_temp_path(target_file, ".copytmp");
    std::filesystem::copy_file(
        source_file,
        tmp,
        std::filesystem::copy_options::overwrite_existing,
        ec);
    if (ec) {
        std::filesystem::remove(tmp, ec);
        return core::Result<void>::failure("Failed to copy file.", ec.message());
    }
    auto rep = atomic_replace_file(tmp, target_file);
    if (!rep.ok()) {
        std::filesystem::remove(tmp, ec);
    }
    return rep;
}

std::filesystem::path make_unique_temp_path(
    const std::filesystem::path& target_file,
    const std::string& tag) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const long pid = static_cast<long>(getpid());
    return std::filesystem::path(
        target_file.string() + "." + std::to_string(pid) + "." + std::to_string(stamp) + tag);
}

void apply_dark_title_bar(void* /*native_window_handle*/, bool /*dark_mode*/) {
}

} // namespace imgstudio::platform
