from conan import ConanFile
from conan.tools.cmake import cmake_layout


class CppTemplateConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeDeps", "CMakeToolchain"
    default_options = {
        "spdlog/*:header_only": True,
    }

    def requirements(self):
        self.requires("gtest/1.16.0")
        self.requires("glm/1.0.1")
        self.requires("glfw/3.4")
        self.requires("nlohmann_json/3.11.3")
        self.requires("libxml2/2.13.6")
        self.requires("cli/2.1.0")
        self.requires("asio/1.28.2")
        self.requires("spdlog/1.15.3")
        self.requires("imgui/1.92.8-docking")
        self.requires("pybind11/3.0.1")
        self.requires("ftxui/5.0.0")

        # ImTui itself isn't on ConanCenter (vendored via CMake FetchContent
        # in CMakeLists.txt), but its ncurses backend needs a curses
        # implementation. The tui_imtui target is dropped on Windows (see
        # CMakeLists.txt), so only pull ncurses in for the platforms that
        # still build it.
        if self.settings.os != "Windows":
            self.requires("ncurses/6.5")

    def layout(self):
        cmake_layout(self)
