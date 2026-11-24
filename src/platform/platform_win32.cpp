#include "platform/AtomicFileOps.hpp"
#include "platform/FileDialogs.hpp"
#include "platform/Platform.hpp"
#include "platform/WindowChrome.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <dwmapi.h>
#include <objbase.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>

#include <chrono>
#include <fstream>
#include <vector>

namespace imgstudio::platform {

namespace {

std::wstring utf8_to_wide(const std::string& utf8) {
    if (utf8.empty()) {
        return {};
    }
    const int needed = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
    if (needed <= 0) {
        return {};
    }
    std::wstring wide(static_cast<std::size_t>(needed), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), wide.data(), needed);
    return wide;
}

std::string wide_to_utf8(const std::wstring& wide) {
    if (wide.empty()) {
        return {};
    }
    const int needed = WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (needed <= 0) {
        return {};
    }
    std::string utf8(static_cast<std::size_t>(needed), '\0');
    WideCharToMultiByte(
        CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), needed, nullptr, nullptr);
    return utf8;
}

std::string format_win32_error(DWORD code) {
    LPWSTR buf = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD len = FormatMessageW(flags, nullptr, code, 0, reinterpret_cast<LPWSTR>(&buf), 0, nullptr);
    std::string msg = "Win32 error " + std::to_string(code);
    if (len > 0 && buf != nullptr) {
        std::wstring wmsg(buf, len);
        while (!wmsg.empty() && (wmsg.back() == L'\r' || wmsg.back() == L'\n')) {
            wmsg.pop_back();
        }
        msg += ": " + wide_to_utf8(wmsg);
    }
    if (buf != nullptr) {
        LocalFree(buf);
    }
    return msg;
}

class ComInitGuard {
public:
    ComInitGuard() noexcept {
        const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        initialized_ = (hr == S_OK || hr == S_FALSE);
    }
    ~ComInitGuard() {
        if (initialized_) {
            CoUninitialize();
        }
    }
    ComInitGuard(const ComInitGuard&) = delete;
    ComInitGuard& operator=(const ComInitGuard&) = delete;

private:
    bool initialized_ = false;
};

struct WideFilterStorage {
    std::vector<std::wstring> labels;
    std::vector<std::wstring> patterns;
    std::vector<COMDLG_FILTERSPEC> specs;
};

WideFilterStorage build_filter_specs(const std::vector<FileFilterSpec>& filters) {
    WideFilterStorage storage;
    storage.labels.reserve(filters.size());
    storage.patterns.reserve(filters.size());
    storage.specs.reserve(filters.size());
    for (const auto& f : filters) {
        storage.labels.push_back(utf8_to_wide(f.label));
        storage.patterns.push_back(utf8_to_wide(f.pattern));
    }
    for (std::size_t i = 0; i < filters.size(); ++i) {
        storage.specs.push_back(COMDLG_FILTERSPEC{
            storage.labels[i].c_str(),
            storage.patterns[i].c_str()
        });
    }
    return storage;
}

std::optional<std::filesystem::path> path_from_shell_item(IShellItem* item) {
    if (!item) {
        return std::nullopt;
    }
    PWSTR psz_path = nullptr;
    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &psz_path)) && psz_path != nullptr) {
        std::filesystem::path out(psz_path);
        CoTaskMemFree(psz_path);
        return out;
    }
    return std::nullopt;
}

} // namespace

SystemPaths get_system_paths() {
    SystemPaths paths;

    std::wstring exe_buf(MAX_PATH, L'\0');
    DWORD len = GetModuleFileNameW(nullptr, exe_buf.data(), static_cast<DWORD>(exe_buf.size()));
    while (len == exe_buf.size() && exe_buf.size() < 32768U) {
        exe_buf.resize(exe_buf.size() * 2U);
        len = GetModuleFileNameW(nullptr, exe_buf.data(), static_cast<DWORD>(exe_buf.size()));
    }
    if (len > 0) {
        exe_buf.resize(len);
        paths.executable_dir = std::filesystem::path(exe_buf).parent_path();
    } else {
        paths.executable_dir = std::filesystem::current_path();
    }

    PWSTR local_app_data = nullptr;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &local_app_data)) && local_app_data != nullptr) {
        paths.app_data_dir = std::filesystem::path(local_app_data) / "IMGStudio";
        CoTaskMemFree(local_app_data);
    } else {
        paths.app_data_dir = paths.executable_dir / "data";
    }

    paths.default_backup_dir = paths.app_data_dir / "backups";
    paths.settings_file = paths.app_data_dir / "settings.json";
    paths.assets_dir = paths.executable_dir / "assets";

    std::error_code ec;
    if (!std::filesystem::exists(paths.assets_dir, ec)) {
        const auto parent_assets = paths.executable_dir.parent_path() / "assets";
        if (std::filesystem::exists(parent_assets, ec)) {
            paths.assets_dir = parent_assets;
        }
    }

    return paths;
}

std::string to_utf8(const std::filesystem::path& p) {
    return wide_to_utf8(p.wstring());
}

std::filesystem::path from_utf8(const std::string& utf8) {
    return std::filesystem::path(utf8_to_wide(utf8));
}

std::filesystem::path resolve_asset_path(const std::string& relative_path) {
    const auto sys = get_system_paths();
    const auto rel = from_utf8(relative_path);
    std::error_code ec;
    const auto p1 = sys.assets_dir / rel;
    if (std::filesystem::exists(p1, ec)) {
        return p1;
    }
    const auto p2 = std::filesystem::current_path() / "assets" / rel;
    if (std::filesystem::exists(p2, ec)) {
        return p2;
    }
    return p1;
}

bool reveal_in_file_manager(const std::filesystem::path& path) {
    const std::wstring arg = L"/select,\"" + path.wstring() + L"\"";
    const HINSTANCE res = ShellExecuteW(nullptr, L"open", L"explorer.exe", arg.c_str(), nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<std::intptr_t>(res) > 32;
}

bool open_directory_in_file_manager(const std::filesystem::path& dir) {
    const HINSTANCE res = ShellExecuteW(nullptr, L"open", dir.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<std::intptr_t>(res) > 32;
}

std::string detect_system_language() {
    wchar_t locale_name[LOCALE_NAME_MAX_LENGTH]{};
    if (GetUserDefaultLocaleName(locale_name, LOCALE_NAME_MAX_LENGTH) > 0) {
        const std::string utf8 = wide_to_utf8(locale_name);
        std::string code;
        for (char ch : utf8) {
            if ((ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')) {
                code.push_back((ch >= 'A' && ch <= 'Z') ? static_cast<char>(ch + ('a' - 'A')) : ch);
                if (code.size() == 2U) {
                    return code;
                }
            } else {
                break;
            }
        }
    }
    return "en";
}

std::optional<std::filesystem::path> open_file_dialog(
    void* native_window_handle,
    const std::string& title,
    const std::vector<FileFilterSpec>& filters) {
    ComInitGuard com;
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
        return std::nullopt;
    }

    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST);

    const std::wstring wtitle = utf8_to_wide(title);
    dlg->SetTitle(wtitle.c_str());

    const auto storage = build_filter_specs(filters);
    if (!storage.specs.empty()) {
        dlg->SetFileTypes(static_cast<UINT>(storage.specs.size()), storage.specs.data());
        dlg->SetFileTypeIndex(1);
    }

    std::optional<std::filesystem::path> result;
    if (SUCCEEDED(dlg->Show(static_cast<HWND>(native_window_handle)))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item != nullptr) {
            result = path_from_shell_item(item);
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

std::vector<std::filesystem::path> open_files_dialog(
    void* native_window_handle,
    const std::string& title,
    const std::vector<FileFilterSpec>& filters) {
    ComInitGuard com;
    std::vector<std::filesystem::path> results;

    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
        return results;
    }

    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST | FOS_ALLOWMULTISELECT);

    const std::wstring wtitle = utf8_to_wide(title);
    dlg->SetTitle(wtitle.c_str());

    const auto storage = build_filter_specs(filters);
    if (!storage.specs.empty()) {
        dlg->SetFileTypes(static_cast<UINT>(storage.specs.size()), storage.specs.data());
        dlg->SetFileTypeIndex(1);
    }

    if (SUCCEEDED(dlg->Show(static_cast<HWND>(native_window_handle)))) {
        IShellItemArray* items = nullptr;
        if (SUCCEEDED(dlg->GetResults(&items)) && items != nullptr) {
            DWORD count = 0;
            if (SUCCEEDED(items->GetCount(&count))) {
                results.reserve(count);
                for (DWORD i = 0; i < count; ++i) {
                    IShellItem* item = nullptr;
                    if (SUCCEEDED(items->GetItemAt(i, &item)) && item != nullptr) {
                        if (auto p = path_from_shell_item(item)) {
                            results.push_back(std::move(*p));
                        }
                        item->Release();
                    }
                }
            }
            items->Release();
        }
    }
    dlg->Release();
    return results;
}

std::optional<std::filesystem::path> save_file_dialog(
    void* native_window_handle,
    const std::string& title,
    const std::string& default_name,
    const std::vector<FileFilterSpec>& filters) {
    ComInitGuard com;
    IFileSaveDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileSaveDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
        return std::nullopt;
    }

    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_OVERWRITEPROMPT | FOS_PATHMUSTEXIST);

    const std::wstring wtitle = utf8_to_wide(title);
    dlg->SetTitle(wtitle.c_str());

    if (!default_name.empty()) {
        const std::wstring wdef = utf8_to_wide(default_name);
        dlg->SetFileName(wdef.c_str());
    }

    const auto storage = build_filter_specs(filters);
    if (!storage.specs.empty()) {
        dlg->SetFileTypes(static_cast<UINT>(storage.specs.size()), storage.specs.data());
        dlg->SetFileTypeIndex(1);
    }

    std::optional<std::filesystem::path> result;
    if (SUCCEEDED(dlg->Show(static_cast<HWND>(native_window_handle)))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item != nullptr) {
            result = path_from_shell_item(item);
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

std::optional<std::filesystem::path> pick_folder_dialog(
    void* native_window_handle,
    const std::string& title) {
    ComInitGuard com;
    IFileOpenDialog* dlg = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dlg)))) {
        return std::nullopt;
    }

    DWORD opts = 0;
    dlg->GetOptions(&opts);
    dlg->SetOptions(opts | FOS_FORCEFILESYSTEM | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);

    const std::wstring wtitle = utf8_to_wide(title);
    dlg->SetTitle(wtitle.c_str());

    std::optional<std::filesystem::path> result;
    if (SUCCEEDED(dlg->Show(static_cast<HWND>(native_window_handle)))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dlg->GetResult(&item)) && item != nullptr) {
            result = path_from_shell_item(item);
            item->Release();
        }
    }
    dlg->Release();
    return result;
}

core::Result<void> atomic_replace_file(
    const std::filesystem::path& temp_file,
    const std::filesystem::path& target_file) {
    std::error_code ec;
    if (!std::filesystem::exists(temp_file, ec)) {
        return core::Result<void>::failure("Temporary file does not exist for atomic replacement.", to_utf8(temp_file));
    }

    // Flush buffers on temporary file before replacing destination
    {
        HANDLE h = CreateFileW(
            temp_file.wstring().c_str(),
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            FlushFileBuffers(h);
            CloseHandle(h);
        }
    }

    if (std::filesystem::exists(target_file, ec)) {
        if (ReplaceFileW(
                target_file.wstring().c_str(),
                temp_file.wstring().c_str(),
                nullptr,
                REPLACEFILE_IGNORE_MERGE_ERRORS,
                nullptr,
                nullptr)) {
            return core::Result<void>::success();
        }
        const DWORD err = GetLastError();
        if (MoveFileExW(
                temp_file.wstring().c_str(),
                target_file.wstring().c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            return core::Result<void>::success();
        }
        return core::Result<void>::failure(
            "Failed to atomically replace target file.",
            format_win32_error(err));
    }

    if (!MoveFileExW(
            temp_file.wstring().c_str(),
            target_file.wstring().c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        return core::Result<void>::failure(
            "Failed to move rebuilt file to destination.",
            format_win32_error(GetLastError()));
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
    if (!CopyFileW(source_file.wstring().c_str(), tmp.wstring().c_str(), FALSE)) {
        const DWORD err = GetLastError();
        std::filesystem::remove(tmp, ec);
        return core::Result<void>::failure("Failed to copy file.", format_win32_error(err));
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
    const DWORD pid = GetCurrentProcessId();
    const std::wstring wtag = utf8_to_wide(tag);
    return std::filesystem::path(
        target_file.wstring() + L"." + std::to_wstring(pid) + L"." + std::to_wstring(stamp) + wtag);
}

void apply_dark_title_bar(void* native_window_handle, bool dark_mode) {
    if (!native_window_handle) {
        return;
    }
    HWND hwnd = static_cast<HWND>(native_window_handle);
    const BOOL value = dark_mode ? TRUE : FALSE;
    // DWMWA_USE_IMMERSIVE_DARK_MODE = 20 (Windows 11 / Win10 20H1+), fallback 19
    if (FAILED(DwmSetWindowAttribute(hwnd, 20, &value, sizeof(value)))) {
        DwmSetWindowAttribute(hwnd, 19, &value, sizeof(value));
    }

    const HINSTANCE hinst = GetModuleHandleW(nullptr);
    if (HICON big_icon = static_cast<HICON>(LoadImageW(
            hinst, MAKEINTRESOURCEW(101), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR))) {
        SendMessageW(hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(big_icon));
    }
    if (HICON small_icon = static_cast<HICON>(LoadImageW(
            hinst, MAKEINTRESOURCEW(101), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR))) {
        SendMessageW(hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(small_icon));
    }
}

} // namespace imgstudio::platform
