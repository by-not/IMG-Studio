include(FetchContent)

set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# nlohmann/json v3.11.3 (pinned commit: 9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

if(IMGSTUDIO_BUILD_GUI)
    # GLFW 3.4 (pinned commit: 70a9bb3881fe80fd483236e2b203cb451c6ecf40)
    FetchContent_Declare(
        glfw
        GIT_REPOSITORY https://github.com/glfw/glfw.git
        GIT_TAG 3.4
        GIT_SHALLOW TRUE
    )
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(glfw)

    # Dear ImGui v1.91.9b-docking (pinned commit: 52fe0a05a7b1aa180a202bb24f0f2a049a9c1b7d)
    FetchContent_Declare(
        imgui
        GIT_REPOSITORY https://github.com/ocornut/imgui.git
        GIT_TAG v1.91.9b-docking
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(imgui)
    if(NOT imgui_POPULATED)
        FetchContent_MakeAvailable(imgui)
    endif()

    add_library(imgui STATIC
        ${imgui_SOURCE_DIR}/imgui.cpp
        ${imgui_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
        ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        ${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp
    )
    target_include_directories(imgui PUBLIC
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
        ${imgui_SOURCE_DIR}/misc/cpp
    )
    target_link_libraries(imgui PUBLIC glfw)
    if(WIN32)
        target_link_libraries(imgui PUBLIC opengl32 gdi32)
    endif()
    set_target_properties(imgui PROPERTIES CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON)
endif()

if(IMGSTUDIO_BUILD_TESTS)
    # Catch2 v3.8.1 (pinned commit: 56809e5282f104c5c8b570e7c2996cdc352d94f0)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.8.1
        GIT_SHALLOW TRUE
    )
    set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(Catch2)
endif()
