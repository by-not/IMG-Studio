#include "ui/AppWindow.hpp"
#include "app/ArchiveController.hpp"
#include "core/ImgFilename.hpp"
#include "platform/Platform.hpp"
#include "platform/WindowChrome.hpp"
#include "ui/ExplorerTable.hpp"
#include "ui/MainView.hpp"
#include "ui/Theme.hpp"
#include "ui/UiState.hpp"

#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace imgstudio::ui {

namespace {

struct DropDispatchContext {
    app::ArchiveController* controller = nullptr;
    UiState* state = nullptr;
};

void glfw_drop_callback(GLFWwindow* window, int path_count, const char* paths[]) {
    auto* ctx = static_cast<DropDispatchContext*>(glfwGetWindowUserPointer(window));
    if (!ctx || !ctx->controller || !ctx->state || path_count <= 0) {
        return;
    }

    std::vector<std::filesystem::path> img_files;
    std::vector<std::filesystem::path> asset_files;
    for (int i = 0; i < path_count; ++i) {
        if (!paths[i]) continue;
        const auto p = platform::from_utf8(paths[i]);
        if (core::to_ascii_lower(p.extension().string()) == ".img") {
            img_files.push_back(p);
        } else {
            asset_files.push_back(p);
        }
    }

    if (!img_files.empty() && !ctx->controller->is_busy()) {
        ctx->controller->start_open_archive(img_files.front());
    }
    if (!asset_files.empty()) {
        if (ctx->controller->active_document()) {
            trigger_import_files(*ctx->controller, *ctx->state, asset_files);
        } else {
            ctx->state->push_toast(
                "No archive open",
                "Open a .img archive before dropping files to import.",
                true);
        }
    }
}

void write_smoke_report(
    const AppLaunchOptions& options,
    const std::string& status,
    const std::string& gl_vendor,
    const std::string& gl_renderer,
    const std::string& gl_version,
    const std::string& font_summary,
    const app::ArchiveController& controller) {
    if (!options.smoke_test_report_path.has_value()) {
        return;
    }
    std::ofstream out(*options.smoke_test_report_path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) {
        return;
    }
    out << "status=" << status << "\n";
    out << "gl_vendor=" << gl_vendor << "\n";
    out << "gl_renderer=" << gl_renderer << "\n";
    out << "gl_version=" << gl_version << "\n";
    out << "fonts=" << font_summary << "\n";
    out << "open_tabs=" << controller.tabs().size() << "\n";
    if (const auto* doc = controller.active_document()) {
        out << "active_archive=" << platform::to_utf8(doc->path()) << "\n";
        out << "active_entries=" << doc->entries().size() << "\n";
        out << "validation_errors=" << doc->validation_report().error_count << "\n";
    }
    out.flush();
}

} // namespace

int run_application(const AppLaunchOptions& options) {
    if (!glfwInit()) {
        return 2;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (options.smoke_test) {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    }

    GLFWwindow* window = glfwCreateWindow(1360, 840, "IMG Studio — GTA San Andreas IMG VER2", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 3;
    }

    glfwSetWindowSizeLimits(window, 960, 620, GLFW_DONT_CARE, GLFW_DONT_CARE);
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    void* native_hwnd = nullptr;
#ifdef _WIN32
    native_hwnd = glfwGetWin32Window(window);
    platform::apply_dark_title_bar(native_hwnd, true);
#endif

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr; // Avoid polluting working directories with imgui.ini

    app::ArchiveController controller;
    UiState ui_state;

    float xscale = 1.0f;
    float yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    apply_modern_dark_theme(controller.settings().ui_scale);
    const FontBundle fonts = load_application_fonts(xscale, controller.settings().ui_scale);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    DropDispatchContext drop_ctx{&controller, &ui_state};
    glfwSetWindowUserPointer(window, &drop_ctx);
    glfwSetDropCallback(window, glfw_drop_callback);

    if (options.initial_archive.has_value()) {
        auto open_res = controller.open_archive_sync(*options.initial_archive);
        if (!open_res.ok()) {
            ui_state.push_toast("Failed to open archive", open_res.error->message, true);
        }
    }

    bool request_window_close = false;
    int frame_counter = 0;

    while (!request_window_close) {
        if (glfwWindowShouldClose(window)) {
            if (!options.smoke_test && controller.any_unsaved_changes()) {
                glfwSetWindowShouldClose(window, GLFW_FALSE);
                ui_state.close_app_after_confirm = true;
                ui_state.open_close_confirm_modal = true;
            } else {
                break;
            }
        }
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        render_main_view(controller, ui_state, native_hwnd, fonts, request_window_close);

        ImGui::Render();
        int fb_w = 0;
        int fb_h = 0;
        glfwGetFramebufferSize(window, &fb_w, &fb_h);
        glViewport(0, 0, fb_w, fb_h);
        const auto& bg = theme_palette().bg_root;
        glClearColor(bg.x, bg.y, bg.z, bg.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);

        ++frame_counter;
        if (options.smoke_test && frame_counter >= 3) {
            const char* vendor = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
            const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
            const char* glver = reinterpret_cast<const char*>(glGetString(GL_VERSION));
            write_smoke_report(
                options,
                "SMOKE_TEST_OK",
                vendor ? vendor : "unknown",
                renderer ? renderer : "unknown",
                glver ? glver : "unknown",
                fonts.loaded_summary,
                controller);
            break;
        }
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

} // namespace imgstudio::ui
