cmake_minimum_required(VERSION 3.16)
project(pf VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Only Debug packages are installed via Conan; map other configs to Debug so
# imported targets resolve correctly and CMP0111 warnings are silenced.
# NOTE: this (and the _DEBUG suffix in the imgui section below) is a local
# development shortcut that must go before pf ships as a Conan package —
# downstream consumers will build Release.
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
#
# These absolute package-folder include paths are why pf_ui_imgui is not yet
# installable as part of the Conan package — see README "Packaging status".
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

# ===========================================================================
# pf — the platform library, split into components so a consumer links only
# what it needs. A headless service takes pf::dc + pf::log and never pulls in
# libxml2, the CLI stack, OpenGL or Dear ImGui.
#
# Public headers live under include/pf/<component>/ and everything in them is
# in `namespace pf` (ports nested as `pf::dc`), so a downstream project's own
# core/ or settings/ headers can never collide with the platform's.
# ===========================================================================
set(PF_PUBLIC_INCLUDE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# --- pf::dc — sender/receiver ports. Header-only, zero third-party deps. ---
add_library(pf_dc INTERFACE)
add_library(pf::dc ALIAS pf_dc)
target_include_directories(pf_dc INTERFACE ${PF_PUBLIC_INCLUDE})
target_link_libraries(pf_dc INTERFACE Threads::Threads)
target_compile_features(pf_dc INTERFACE cxx_std_17)

# --- pf::log — spdlog wrapper + named loggers + LogLevel enum ---
add_library(pf_log STATIC
    src/pf/log/log.cpp
    src/pf/log/log_level.cpp
)
add_library(pf::log ALIAS pf_log)
target_include_directories(pf_log PUBLIC ${PF_PUBLIC_INCLUDE})
target_link_libraries(pf_log PUBLIC
    nlohmann_json::nlohmann_json   # LogLevel <-> string serialization
    spdlog::spdlog_header_only
    Threads::Threads
)

# --- pf::settings — registry, typed items, dirty tracking, XML persistence ---
add_library(pf_settings STATIC
    src/pf/settings/settings_registry.cpp
    src/pf/settings/dirty_tracker.cpp
)
add_library(pf::settings ALIAS pf_settings)
target_include_directories(pf_settings PUBLIC ${PF_PUBLIC_INCLUDE})
target_link_libraries(pf_settings PUBLIC
    nlohmann_json::nlohmann_json
    LibXml2::LibXml2
)

# --- pf::cli — exposes the settings registry over daniele77/cli ---
add_library(pf_cli STATIC
    src/pf/cli/command_registry.cpp
)
add_library(pf::cli ALIAS pf_cli)
target_include_directories(pf_cli PUBLIC ${PF_PUBLIC_INCLUDE})
target_link_libraries(pf_cli PUBLIC
    pf::settings
    cli::cli
)

# --- pf::ui_imgui — window host, settings editor panel, ImGui log sink ---
# Kept separate from every other component so targets that must not link Dear
# ImGui (cli_server, tui, tui_imtui with its own patched ImGui fork) can't
# accidentally pull it in.
add_library(pf_ui_imgui STATIC
    src/pf/ui/imgui_app.cpp
    src/pf/ui/imgui_log_sink.cpp
    src/pf/ui/settings_editor.cpp
)
add_library(pf::ui_imgui ALIAS pf_ui_imgui)
target_include_directories(pf_ui_imgui PUBLIC ${PF_PUBLIC_INCLUDE})
target_link_libraries(pf_ui_imgui PUBLIC
    pf::settings
    pf::log
    imgui_backends
    glfw
    OpenGL::GL
)
if(APPLE)
    target_compile_definitions(pf_ui_imgui PUBLIC GL_SILENCE_DEPRECATION)
endif()

# NOTE: there is deliberately no pf::ui_ftxui yet — nothing FTXUI-side has been
# extracted from the tutorial into reusable platform code. See TUTORIAL.md
# lessons 06-08 for the candidates.

# ===========================================================================
# demo::config — example settings structs and port payloads used by the demo
# apps, the tests and the tutorial. Explicitly NOT part of the pf package:
# downstream projects write their own equivalents. Lives in `namespace demo`
# so the app/platform boundary is visible at every call site.
# ===========================================================================
add_library(demo_config INTERFACE)
add_library(demo::config ALIAS demo_config)
target_include_directories(demo_config INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/examples/include)
target_link_libraries(demo_config INTERFACE pf::settings pf::log)

# ---------------------------------------------------------------------------
# Main application — ImGui window + settings + sender/receiver demo
# ---------------------------------------------------------------------------
add_executable(app
    app/main.cpp
)
target_link_libraries(app PRIVATE
    pf::ui_imgui
    pf::dc
    demo::config
)

# ---------------------------------------------------------------------------
# CLI server — telnet-based settings inspector
# ---------------------------------------------------------------------------
add_executable(cli_server
    app/cli_server.cpp
)
target_compile_definitions(cli_server PRIVATE ASIO_STANDALONE)
target_link_libraries(cli_server PRIVATE
    pf::cli
    demo::config
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
# ftxui_tutorial — progressive FTXUI lessons (DOM, components, events, custom
# components) plus this project's own pf::SettingsItem<T> / pf::dc:: ports /
# pf::Log systems wired into a terminal UI. See tutorial/ftxui/TUTORIAL.md.
# Links the non-ImGui pf components only.
# ---------------------------------------------------------------------------
add_executable(ftxui_tutorial
    tutorial/ftxui/main.cpp
    tutorial/ftxui/examples/lesson_01_hello_world.cpp
    tutorial/ftxui/examples/lesson_02_layout_and_styling.cpp
    tutorial/ftxui/examples/lesson_03_components.cpp
    tutorial/ftxui/examples/lesson_04_events_and_focus.cpp
    tutorial/ftxui/examples/lesson_05_custom_components.cpp
    tutorial/ftxui/examples/lesson_06_dc_ports.cpp
    tutorial/ftxui/examples/lesson_07_settings_panel.cpp
    tutorial/ftxui/examples/lesson_08_log_panel.cpp
    tutorial/ftxui/examples/capstone_panels.cpp
    tutorial/ftxui/examples/lesson_09_capstone_tabs.cpp
    tutorial/ftxui/examples/lesson_10_capstone_split.cpp
)
target_link_libraries(ftxui_tutorial PRIVATE
    pf::settings
    pf::log
    pf::dc
    demo::config
    ftxui::screen
    ftxui::dom
    ftxui::component
)

# ---------------------------------------------------------------------------
# ImTui — Dear ImGui rendered to a terminal via ncurses. Not on ConanCenter,
# so it's vendored with FetchContent. ImTui ships its own patched Dear ImGui
# fork (ggerganov/imgui@imtui, pinned to the commit ImTui v1.0.5's submodule
# points at) and its own CMakeLists.txt assumes a MinGW/Unix toolchain
# (Windows handling is marked "todo" upstream), so rather than
# add_subdirectory()'ing it, its handful of source files are compiled
# directly here instead — the same approach imgui_backends above takes with
# the Conan imgui package's resource sources. Populate-only (no
# add_subdirectory) so neither repo's own build logic runs.
#
# Dropped entirely on Windows: ImTui's ncurses backend there depends on
# pdcurses plus a hand-written <pdcurses.h>/<curses.h> shim, which proved
# more trouble than the demo target is worth. Non-Windows platforms keep
# tui_imtui as before.
#
# TODO: replace FetchContent with a local Conan recipe (recipes/imtui) so a
# clean configure doesn't need network access.
# ---------------------------------------------------------------------------
if(NOT WIN32)
FetchContent_Declare(imtui
    GIT_REPOSITORY https://github.com/ggerganov/imtui.git
    GIT_TAG        v1.0.5
    # Upstream's ncurses backend tracks left/right mouse buttons but never
    # reads BUTTON4/5_PRESSED (scroll wheel) into ImGui's IO, so wheel
    # scrolling silently does nothing. Patch it in.
    PATCH_COMMAND  git apply --whitespace=nowarn ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imtui_patches/0001-ncurses-mouse-wheel-scroll.patch
    # GIT_TAG is a fixed tag, so there's nothing to update — but without
    # this, every reconfigure re-runs the git update step, which bumps the
    # step's stamp and forces PATCH_COMMAND to re-run against the
    # already-patched tree (git apply then fails: "patch does not apply").
    UPDATE_DISCONNECTED TRUE
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
# must never be linked into the same binary as pf::ui_imgui/imgui_backends.
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
# Conan's ncurses package overrides find_package(Curses) (Module mode) with
# its own generated shim, same as how LibXml2 is resolved above, so this
# resolves to the Conan-installed ncurses rather than a system-installed one.
find_package(Curses REQUIRED)
target_include_directories(imtui_ncurses PRIVATE ${CURSES_INCLUDE_DIR})
target_link_libraries(imtui_ncurses PUBLIC ${CURSES_LIBRARIES})

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
endif() # NOT WIN32

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
    pf::dc
    pf::settings
    pf::log
    demo::config
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
