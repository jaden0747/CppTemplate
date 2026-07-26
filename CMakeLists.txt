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
find_package(cli CONFIG REQUIRED)
find_package(asio CONFIG REQUIRED)
find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
find_package(spdlog REQUIRED)
find_package(imgui REQUIRED CONFIG)
find_package(pybind11 REQUIRED CONFIG)

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
# necessity; their non-template collaborators live here instead).
# ---------------------------------------------------------------------------
add_library(libs STATIC
    libs/core/imgui_app.cpp
    libs/core/imgui_log_sink.cpp
    libs/core/log.cpp
    libs/core/log_level.cpp
    libs/settings/command_registry.cpp
    libs/settings/dirty_tracker.cpp
    libs/settings/settings_editor.cpp
    libs/settings/settings_registry.cpp
)
target_include_directories(libs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(libs PUBLIC
    imgui_backends
    glfw
    OpenGL::GL
    nlohmann_json::nlohmann_json
    spdlog::spdlog_header_only
    Threads::Threads
    cli::cli
)
if(APPLE)
    target_compile_definitions(libs PUBLIC GL_SILENCE_DEPRECATION)
endif()

# ---------------------------------------------------------------------------
# Main application — ImGui window + settings + sender/receiver demo
# ---------------------------------------------------------------------------
add_executable(app
    app/main.cpp
)
target_link_libraries(app PRIVATE
    libs
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
        "${CMAKE_SOURCE_DIR}/resources/settings.json"
        "$<TARGET_FILE_DIR:app>/settings.json"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/resources/font"
        "$<TARGET_FILE_DIR:app>/font"
)
add_custom_command(TARGET cli_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/resources/settings.json"
        "$<TARGET_FILE_DIR:cli_server>/settings.json"
)
