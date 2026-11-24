#include "ui/AppWindow.hpp"
#include "app/ArchiveController.hpp"
#include "app/Localization.hpp"
#include "core/ImgFilename.hpp"
#include "platform/Platform.hpp"
#include "platform/WindowChrome.hpp"
#include "render/GlLoader.hpp"
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
        ctx->state->active_workspace = WorkspaceTab::ArchiveExplorer;
    }
    if (!asset_files.empty()) {
        if (ctx->controller->active_document()) {
            trigger_import_files(*ctx->controller, *ctx->state, asset_files);
        } else {
            ctx->state->push_toast(
                app::tr("toast.no_archive_open"),
                app::tr("toast.drop_need_img"),
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
    const app::ArchiveController& controller,
    const StudioModules& modules,
    GLenum last_gl_err) {
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
    out << "gl_loader_ok=" << (render::gl::is_gl_ready() ? 1 : 0) << "\n";
    out << "dff_renderer_ok=" << (modules.dff_studio.renderer().is_initialized() ? 1 : 0) << "\n";
    out << "dff_fbo_ok=" << (modules.dff_studio.renderer().fbo_valid() ? 1 : 0) << "\n";
    out << "dff_has_model=" << (modules.dff_studio.renderer().has_model() ? 1 : 0) << "\n";
    if (modules.dff_studio.session().has_value()) {
        const auto& s = *modules.dff_studio.session();
        std::size_t resolved_cnt = 0;
        for (const auto& [k, v] : s.resolved_textures) {
            if (v.resolved) ++resolved_cnt;
        }
        out << "dff_vertices=" << s.model.total_vertices << "\n";
        out << "dff_triangles=" << s.model.total_triangles << "\n";
        out << "dff_resolved_textures=" << resolved_cnt << "\n";
    }
    if (modules.txd_studio.dictionary() != nullptr) {
        out << "txd_loaded_textures=" << modules.txd_studio.dictionary()->textures.size() << "\n";
    }
    out << "gl_error=" << static_cast<unsigned>(last_gl_err) << "\n";
    out << "locale=" << app::Localization::instance().current_language() << "\n";
    out << "requested_locale=" << app::Localization::instance().requested_language() << "\n";
    out << "sample_explorer=" << app::tr("workspace.explorer") << "\n";
    out << "sample_settings=" << app::tr("modal.settings.title") << "\n";
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

    GLFWwindow* window = glfwCreateWindow(1400, 860, "IMG Studio 2.0 — GTA San Andreas Asset & Archive Studio", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 3;
    }

    glfwSetWindowSizeLimits(window, 1024, 640, GLFW_DONT_CARE, GLFW_DONT_CARE);
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
    io.IniFilename = nullptr;

    app::ArchiveController controller(options.settings_file_override);
    (void)app::Localization::instance().load_from_directory(platform::resolve_asset_path("locales"));
    if (options.initial_language.has_value()) {
        app::Localization::instance().set_language(*options.initial_language);
        if (options.save_language_override) {
            controller.settings().language = *options.initial_language;
            controller.save_settings();
        }
    } else {
        app::Localization::instance().set_language(controller.settings().language);
    }

    std::string last_window_lang = app::Localization::instance().current_language();
    glfwSetWindowTitle(window, app::tr_c("app.window_title"));

    UiState ui_state;
    StudioModules modules;

    float xscale = 1.0f;
    float yscale = 1.0f;
    glfwGetWindowContentScale(window, &xscale, &yscale);
    apply_modern_dark_theme(controller.settings().ui_scale);
    const FontBundle fonts = load_application_fonts(xscale, controller.settings().ui_scale);

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");
    (void)render::gl::init_gl_functions();

    DropDispatchContext drop_ctx{&controller, &ui_state};
    glfwSetWindowUserPointer(window, &drop_ctx);
    glfwSetDropCallback(window, glfw_drop_callback);

    if (options.initial_archive.has_value()) {
        auto open_res = controller.open_archive_sync(*options.initial_archive);
        if (!open_res.ok()) {
            ui_state.push_toast(app::tr("toast.open_failed"), open_res.error->message, true);
        } else if (options.smoke_test) {
            // Auto-load first DFF and TXD found inside archive during smoke test if not specified externally
            const auto* doc = controller.active_document();
            if (doc && !options.initial_dff.has_value()) {
                for (const auto& e : doc->entries()) {
                    if (e.category == core::FileCategory::DffModel) {
                        modules.dff_studio.open_from_document_sync(*doc, e.id);
                        break;
                    }
                }
            }
            if (doc && !options.initial_txd.has_value()) {
                for (const auto& e : doc->entries()) {
                    if (e.category == core::FileCategory::TxdTexture) {
                        modules.txd_studio.open_from_document_sync(*doc, e.id);
                        break;
                    }
                }
            }
        }
    }

    if (options.initial_dff.has_value()) {
        if (!modules.dff_studio.open_file_sync(*options.initial_dff)) {
            ui_state.push_toast(app::tr("toast.dff_open_failed"), platform::to_utf8(*options.initial_dff), true);
        } else {
            ui_state.active_workspace = WorkspaceTab::DffStudio;
        }
    }
    if (options.initial_txd.has_value()) {
        if (!modules.txd_studio.open_file_sync(*options.initial_txd)) {
            ui_state.push_toast(app::tr("toast.txd_open_failed"), platform::to_utf8(*options.initial_txd), true);
        } else if (!options.initial_dff.has_value()) {
            ui_state.active_workspace = WorkspaceTab::TxdStudio;
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

        const std::string cur_lang = app::Localization::instance().current_language();
        if (cur_lang != last_window_lang) {
            last_window_lang = cur_lang;
            glfwSetWindowTitle(window, app::tr_c("app.window_title"));
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        render_main_view(controller, ui_state, modules, native_hwnd, fonts, request_window_close);

        if (options.smoke_test) {
            modules.dff_studio.render_smoke_frame(640, 480);
        }

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
            const GLenum gl_err = glGetError();
            write_smoke_report(
                options,
                "SMOKE_TEST_OK",
                vendor ? vendor : "unknown",
                renderer ? renderer : "unknown",
                glver ? glver : "unknown",
                fonts.loaded_summary,
                controller,
                modules,
                gl_err);
            break;
        }
    }

    modules.shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

} // namespace imgstudio::ui
