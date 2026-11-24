cmake_minimum_required(VERSION 3.25)

project(IMGStudio
    VERSION 2.0.0
    DESCRIPTION "Modern GTA San Andreas IMG VER2, DFF and TXD Studio"
    LANGUAGES CXX C
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    set(_default_build_gui ON)
else()
    set(_default_build_gui OFF)
endif()

option(IMGSTUDIO_BUILD_GUI "Build the desktop GUI executable" ${_default_build_gui})
option(IMGSTUDIO_BUILD_TESTS "Build the automated test suite" ON)
option(IMGSTUDIO_ENABLE_SANITIZERS "Enable ASan and UBSan on supported compilers" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Dependencies)

function(imgstudio_apply_sanitizers target_name)
    if(IMGSTUDIO_ENABLE_SANITIZERS AND NOT WIN32)
        target_compile_options(${target_name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)
    endif()
endfunction()

# Static MinGW runtime helper for Windows targets
function(imgstudio_apply_win32_static_runtime target_name)
    if(MINGW)
        target_link_options(${target_name} PRIVATE
            -static
            -static-libgcc
            -static-libstdc++
        )
    endif()
endfunction()

# Core IMG VER2 + RenderWare binary engine (pure C++20, zero UI/Win32 dependencies)
add_library(imgstudio_core STATIC
    src/core/BinaryIO.cpp
    src/core/ImgFilename.cpp
    src/core/ImgReader.cpp
    src/core/ImgArchive.cpp
    src/core/ImgValidator.cpp
    src/core/ImgExtractor.cpp
    src/core/ImgBuilder.cpp
    src/core/ImgCompare.cpp
    src/core/rw/RwStream.cpp
    src/core/rw/DffParser.cpp
    src/core/rw/TxdDecoder.cpp
    src/core/rw/TxdParser.cpp
    src/core/rw/ImageExporter.cpp
)
target_include_directories(imgstudio_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
)
imgstudio_apply_warnings(imgstudio_core)
imgstudio_apply_sanitizers(imgstudio_core)

# Platform abstraction layer
if(WIN32)
    set(_platform_sources src/platform/platform_win32.cpp)
else()
    set(_platform_sources src/platform/platform_posix.cpp)
endif()

add_library(imgstudio_platform STATIC ${_platform_sources})
target_include_directories(imgstudio_platform PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(imgstudio_platform PUBLIC imgstudio_core)
if(WIN32)
    target_link_libraries(imgstudio_platform PUBLIC ole32 shell32 uuid dwmapi comdlg32)
endif()
imgstudio_apply_warnings(imgstudio_platform)
imgstudio_apply_sanitizers(imgstudio_platform)

# Application services layer
add_library(imgstudio_app STATIC
    src/app/AppSettings.cpp
    src/app/BackupManager.cpp
    src/app/OperationHistory.cpp
    src/app/EntryFilter.cpp
    src/app/ArchiveDocument.cpp
    src/app/ArchiveController.cpp
    src/app/AssetLoader.cpp
    src/app/CompareService.cpp
    src/app/BatchProcessor.cpp
)
target_include_directories(imgstudio_app PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(imgstudio_app PUBLIC
    imgstudio_core
    imgstudio_platform
    nlohmann_json::nlohmann_json
)
imgstudio_apply_warnings(imgstudio_app)
imgstudio_apply_sanitizers(imgstudio_app)

# Desktop GUI executable
if(IMGSTUDIO_BUILD_GUI)
    set(_gui_sources
        src/main.cpp
        src/render/GlLoader.cpp
        src/render/GpuTexture.cpp
        src/render/ViewportFbo.cpp
        src/render/DffRenderer.cpp
        src/ui/Theme.cpp
        src/ui/AppWindow.cpp
        src/ui/MainView.cpp
        src/ui/ExplorerTable.cpp
        src/ui/InspectorPanel.cpp
        src/ui/Dialogs.cpp
        src/ui/DffStudioView.cpp
        src/ui/TxdStudioView.cpp
        src/ui/ImgCompareView.cpp
        src/ui/BatchView.cpp
    )
    if(WIN32)
        list(APPEND _gui_sources src/platform/windows/resources.rc)
    endif()

    add_executable(IMGStudio ${_gui_sources})
    target_include_directories(IMGStudio PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(IMGStudio PRIVATE
        imgstudio_app
        imgstudio_platform
        imgstudio_core
        imgui
        glfw
        nlohmann_json::nlohmann_json
    )
    if(WIN32)
        target_link_libraries(IMGStudio PRIVATE opengl32 gdi32 shell32 ole32 dwmapi)
        if(MINGW)
            target_link_options(IMGStudio PRIVATE -mwindows)
        endif()
    endif()
    imgstudio_apply_warnings(IMGStudio)
    imgstudio_apply_sanitizers(IMGStudio)
    imgstudio_apply_win32_static_runtime(IMGStudio)

    add_custom_command(TARGET IMGStudio POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:IMGStudio>/assets"
        COMMENT "Copying IMGStudio assets to output directory"
    )
endif()

# Automated tests
if(IMGSTUDIO_BUILD_TESTS)
    enable_testing()
    add_executable(IMGStudioTests
        tests/TestHelpers.cpp
        tests/RwTestHelpers.cpp
        tests/test_binary_io.cpp
        tests/test_img_reader.cpp
        tests/test_img_validator.cpp
        tests/test_img_extractor.cpp
        tests/test_img_builder.cpp
        tests/test_large_and_faults.cpp
        tests/test_archive_document.cpp
        tests/test_backup_manager.cpp
        tests/test_app_settings.cpp
        tests/test_entry_filter.cpp
        tests/test_dff_parser.cpp
        tests/test_txd_parser.cpp
        tests/test_dff_txd_integration.cpp
        tests/test_img_compare.cpp
        tests/test_batch_processor.cpp
    )
    target_include_directories(IMGStudioTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src"
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
    )
    target_link_libraries(IMGStudioTests PRIVATE
        imgstudio_app
        imgstudio_platform
        imgstudio_core
        nlohmann_json::nlohmann_json
        Catch2::Catch2WithMain
    )
    imgstudio_apply_warnings(IMGStudioTests)
    imgstudio_apply_sanitizers(IMGStudioTests)
    imgstudio_apply_win32_static_runtime(IMGStudioTests)
    add_test(NAME IMGStudioTests COMMAND IMGStudioTests)
endif()
