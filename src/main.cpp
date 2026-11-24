#include "platform/Platform.hpp"
#include "ui/AppWindow.hpp"

#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

int run_cli_main(int argc, char** argv) {
    imgstudio::ui::AppLaunchOptions opts;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--smoke-test") {
            opts.smoke_test = true;
        } else if (arg == "--open" && i + 1 < argc) {
            opts.initial_archive = imgstudio::platform::from_utf8(argv[++i]);
        } else if (arg == "--smoke-test-out" && i + 1 < argc) {
            opts.smoke_test_report_path = imgstudio::platform::from_utf8(argv[++i]);
        } else if (!arg.empty() && arg[0] != '-' && !opts.initial_archive.has_value()) {
            opts.initial_archive = imgstudio::platform::from_utf8(arg);
        }
    }
    return imgstudio::ui::run_application(opts);
}

} // namespace

int main(int argc, char** argv) {
    return run_cli_main(argc, argv);
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    int wargc = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    std::vector<std::string> utf8_args;
    std::vector<char*> ptrs;
    if (wargv != nullptr) {
        utf8_args.reserve(static_cast<std::size_t>(wargc));
        ptrs.reserve(static_cast<std::size_t>(wargc));
        for (int i = 0; i < wargc; ++i) {
            const int len = WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, nullptr, 0, nullptr, nullptr);
            std::string s(len > 0 ? static_cast<std::size_t>(len - 1) : 0U, '\0');
            if (len > 1) {
                WideCharToMultiByte(CP_UTF8, 0, wargv[i], -1, s.data(), len, nullptr, nullptr);
            }
            utf8_args.push_back(std::move(s));
        }
        for (auto& s : utf8_args) {
            ptrs.push_back(s.data());
        }
        LocalFree(wargv);
    }
    return run_cli_main(static_cast<int>(ptrs.size()), ptrs.data());
}
#endif
