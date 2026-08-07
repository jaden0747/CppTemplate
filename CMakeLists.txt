cmake_minimum_required(VERSION 3.16)
project(prototype CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Only Debug packages are installed via Conan; map other configs to Debug so
# imported targets resolve correctly and CMP0111 warnings are silenced.
set(CMAKE_MAP_IMPORTED_CONFIG_RELEASE Debug)
set(CMAKE_MAP_IMPORTED_CONFIG_MINSIZEREL Debug)
set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Debug)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Dependencies (via Conan)
# ---------------------------------------------------------------------------
find_package(glm CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(cli CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)
find_package(imgui REQUIRED CONFIG)
find_package(pybind11 REQUIRED CONFIG)
find_package(ftxui REQUIRED CONFIG)

include(FetchContent)

# ---------------------------------------------------------------------------
# Dear ImGui (docking branch, via Conan)
# The conan-center recipe only compiles imgui core into imgui::imgui; backend
# (imgui_impl_*) and misc/cpp (imgui_stdlib) sources ship as package resources
# under res/bindings and res/misc/cpp — CMakeDeps has no generated variable for
# them, so we reach into the package folder directly. Only Debug is installed
# (see CMAKE_MAP_IMPORTED_CONFIG_* above), so the _DEBUG suffix is fixed.
# ---------------------------------------------------------------------------
set(IMGUI_BACKENDS_DIR ${imgui_PACKAGE_FOLDER_DEBUG}/res/bindings)
set(IMGUI_MISC_CPP_DIR ${imgui_PACKAGE_FOLDER_DEBUG}/res/misc/cpp)
add_library(imgui_backends
    ${IMGUI_BACKENDS_DIR}/imgui_impl_glfw.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_opengl3.cpp
    ${IMGUI_MISC_CPP_DIR}/imgui_stdlib.cpp   # ImGui::InputText(std::string&) helper
)
target_include_directories(imgui_backends PUBLIC ${IMGUI_BACKENDS_DIR} ${IMGUI_MISC_CPP_DIR})
target_link_libraries(imgui_backends PUBLIC imgui::imgui glfw OpenGL::GL)
# Suppress macOS OpenGL deprecation warnings (Apple deprecated GL in 10.14 but it still works)
if(APPLE)
    target_compile_definitions(imgui_backends PUBLIC GL_SILENCE_DEPRECATION)
endif()

# ---------------------------------------------------------------------------
# libs — implementation library for everything under include/ that isn't
# purely template-based (dc::* ports and SettingsItem<T> stay header-only by
# necessity; their non-template collaborators live here instead). Kept free
# of Dear ImGui so it can be linked into targets (cli_server, tui, tui_imtui)
# that must not pull in ImGui — see libs_imgui below for why that matters.
# ---------------------------------------------------------------------------
add_library(libs STATIC
    libs/core/log.cpp
    libs/core/log_level.cpp
    libs/settings/command_registry.cpp
    libs/settings/dirty_tracker.cpp
    libs/settings/settings_registry.cpp
    libs/calculator/calculator.cpp
)
target_include_directories(libs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(libs PUBLIC
    nlohmann_json::nlohmann_json
    LibXml2::LibXml2
    spdlog::spdlog_header_only
    Threads::Threads
    cli::cli
)

# ---------------------------------------------------------------------------
# libs_imgui — the subset of libs/ that depends on real Dear ImGui
# (imgui::imgui). Split out from libs so that tui_imtui, which links its own
# vendored/patched ImGui fork (see the ImTui section below), never ends up
# with both ImGui builds' identically-named symbols in one binary.
# ---------------------------------------------------------------------------
add_library(libs_imgui STATIC
    libs/core/imgui_app.cpp
    libs/core/imgui_log_sink.cpp
    libs/settings/settings_editor.cpp
)
target_link_libraries(libs_imgui PUBLIC
    libs
    imgui_backends
    glfw
    OpenGL::GL
)
if(APPLE)
    target_compile_definitions(libs_imgui PUBLIC GL_SILENCE_DEPRECATION)
endif()

# ---------------------------------------------------------------------------
# Main application — ImGui window + settings + sender/receiver demo
# ---------------------------------------------------------------------------
add_executable(app
    app/main.cpp
)
target_link_libraries(app PRIVATE
    libs_imgui
)

# ---------------------------------------------------------------------------
# CLI server — telnet-based settings inspector
# ---------------------------------------------------------------------------
add_executable(cli_server
    app/cli_server.cpp
)
target_compile_definitions(cli_server PRIVATE ASIO_STANDALONE)
target_link_libraries(cli_server PRIVATE
    libs
    asio::asio
)
if(WIN32)
    target_link_libraries(cli_server PRIVATE ws2_32 mswsock)
endif()

# ---------------------------------------------------------------------------
# TUI — FTXUI component gallery (vendored from FTXUI's own example, see
# app/tui_main.cpp), demonstrating what the library can do.
# ---------------------------------------------------------------------------
add_executable(tui
    app/tui_main.cpp
)
target_link_libraries(tui PRIVATE
    ftxui::screen
    ftxui::dom
    ftxui::component
)

# ---------------------------------------------------------------------------
# ImTui — Dear ImGui rendered to a terminal via (pd)curses. Not on
# ConanCenter, so it's vendored with FetchContent. ImTui ships its own
# patched Dear ImGui fork (ggerganov/imgui@imtui, pinned to the commit ImTui
# v1.0.5's submodule points at) and its own CMakeLists.txt assumes a
# MinGW/Unix toolchain (Windows handling is marked "todo" upstream), so
# rather than add_subdirectory()'ing it, its handful of source files are
# compiled directly here instead — the same approach imgui_backends above
# takes with the Conan imgui package's resource sources. Populate-only (no
# add_subdirectory) so neither repo's own build logic runs.
# ---------------------------------------------------------------------------
FetchContent_Declare(imtui
    GIT_REPOSITORY https://github.com/ggerganov/imtui.git
    GIT_TAG        v1.0.5
)
FetchContent_GetProperties(imtui)
if(NOT imtui_POPULATED)
    FetchContent_Populate(imtui)
endif()

FetchContent_Declare(imtui_imgui_fork
    GIT_REPOSITORY https://github.com/ggerganov/imgui.git
    GIT_TAG        7a6176e15e762ed9f50beadb8e2013743eb3ccd0 # pinned by imtui v1.0.5's third-party/imgui/imgui submodule
    SOURCE_DIR     ${CMAKE_BINARY_DIR}/_deps/imtui_imgui_fork-src/imgui
)
FetchContent_GetProperties(imtui_imgui_fork)
if(NOT imtui_imgui_fork_POPULATED)
    FetchContent_Populate(imtui_imgui_fork)
endif()
get_filename_component(IMTUI_IMGUI_FORK_ROOT ${imtui_imgui_fork_SOURCE_DIR} DIRECTORY)

# ImTui's forked/patched Dear ImGui core — kept entirely separate from the
# imgui::imgui Conan package (different version, same symbol names) so it
# must never be linked into the same binary as libs_imgui/imgui_backends.
add_library(imgui_for_imtui STATIC
    ${imtui_imgui_fork_SOURCE_DIR}/imgui.cpp
    ${imtui_imgui_fork_SOURCE_DIR}/imgui_draw.cpp
    ${imtui_imgui_fork_SOURCE_DIR}/imgui_demo.cpp
    ${imtui_imgui_fork_SOURCE_DIR}/imgui_widgets.cpp
    ${imtui_imgui_fork_SOURCE_DIR}/imgui_tables.cpp
)
target_include_directories(imgui_for_imtui PUBLIC ${IMTUI_IMGUI_FORK_ROOT})

add_library(imtui_core STATIC
    ${imtui_SOURCE_DIR}/src/imtui-impl-text.cpp
)
target_include_directories(imtui_core PUBLIC ${imtui_SOURCE_DIR}/include)
target_link_libraries(imtui_core PUBLIC imgui_for_imtui)

add_library(imtui_ncurses STATIC
    ${imtui_SOURCE_DIR}/src/imtui-impl-ncurses.cpp
)
target_link_libraries(imtui_ncurses PUBLIC imtui_core)
if(WIN32)
    # ImTui's Windows branch (src/imtui-impl-ncurses.cpp) includes
    # <pdcurses.h>, but ConanCenter's pdcurses package only ships the
    # conventional <curses.h>. This one-line shim bridges the two.
    find_package(pdcurses REQUIRED CONFIG)
    target_include_directories(imtui_ncurses PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/third_party/pdcurses_shim)
    target_link_libraries(imtui_ncurses PUBLIC pdcurses::pdcurses)
else()
    # Conan's ncurses package overrides find_package(Curses) (Module mode)
    # with its own generated shim, same as how LibXml2 is resolved above, so
    # this resolves to the Conan-installed ncurses rather than a
    # system-installed one.
    find_package(Curses REQUIRED)
    target_include_directories(imtui_ncurses PRIVATE ${CURSES_INCLUDE_DIR})
    target_link_libraries(imtui_ncurses PUBLIC ${CURSES_LIBRARIES})
endif()

# tui_imtui — ImTui's own ncurses demo (see app/tui_imtui_main.cpp), the
# classic Dear ImGui demo window ported to text, demonstrating what the
# library can do.
add_executable(tui_imtui
    app/tui_imtui_main.cpp
    app/imtui_demo.cpp
)
target_link_libraries(tui_imtui PRIVATE
    imtui_ncurses
)

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
enable_testing()

add_executable(tests
    test/test_data_container.cpp
    test/test_settings.cpp
    test/test_log.cpp
)
target_link_libraries(tests PRIVATE
    libs
    GTest::gtest
    GTest::gtest_main
)
add_test(NAME unit_tests COMMAND tests)

# ---------------------------------------------------------------------------
# Copy resources next to binaries
# ---------------------------------------------------------------------------
add_custom_command(TARGET app POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/settings.xml"
        "$<TARGET_FILE_DIR:app>/settings.xml"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources/font"
        "$<TARGET_FILE_DIR:app>/font"
)
add_custom_command(TARGET cli_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/settings.xml"
        "$<TARGET_FILE_DIR:cli_server>/settings.xml"
)
