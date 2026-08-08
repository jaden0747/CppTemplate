# pf

Reusable C++17 **platform** code, intended to be consumed by other projects as a
Conan package. Four building blocks:

1. **Sender/Receiver Ports** (`pf::dc`) — zero-copy multithreaded data transfer via ring-buffer mempool
2. **Settings** (`pf::settings`) — XML-backed runtime configuration with typed accessors, defaults and change tracking
3. **Logging** (`pf::log`) — spdlog wrapper with named per-module loggers
4. **UI** (`pf::ui_imgui`) — GLFW + Dear ImGui + OpenGL host, plus a generic settings editor panel that introspects the registry with no per-field boilerplate

Everything shipped lives under `include/pf/` + `src/pf/` in `namespace pf`.
Everything else in this repo is demo, tutorial or test code.

## Build

```bash
conan install . --build=missing -s build_type=Debug
cmake --preset conan-default     # multi-config generators (Visual Studio)
# cmake --preset conan-debug     # single-config generators (Ninja/Makefiles)
cmake --build --preset conan-debug
ctest --preset conan-debug
```

## Library targets

| Target | Description | Third-party deps |
|---|---|---|
| `pf::dc` | Sender/receiver ports (header-only) | — |
| `pf::log` | Named loggers, `LogLevel`, sinks | nlohmann_json, spdlog |
| `pf::settings` | Registry, `SettingsItem<T>`, XML load/save | nlohmann_json, libxml2 |
| `pf::cli` | Settings registry over a telnet CLI | + daniele77/cli |
| `pf::ui_imgui` | Window host, settings editor, ImGui log sink | + imgui, glfw, OpenGL |

Link only what you need — a headless consumer takes `pf::dc` + `pf::log` and
never pulls in libxml2, the CLI stack, OpenGL or Dear ImGui.

## Demo / tutorial targets

| Target | Description |
|---|---|
| `app` | ImGui window with settings editor, log panel and a threaded ports demo |
| `cli_server` | Telnet CLI for inspecting/modifying settings at runtime |
| `tui` | FTXUI component gallery |
| `tui_imtui` | ImTui ncurses demo (non-Windows only) |
| `ftxui_tutorial` | Progressive FTXUI tutorial — see `tutorial/ftxui/TUTORIAL.md` |
| `tests` | GTest unit tests for ports, settings and logging |

## Packaging status

**Not yet a Conan package.** `conanfile.py` is still a consumer file and
`CMakeLists.txt` has no `install()`/`export()` rules. Blockers, in order:

1. The `CMAKE_MAP_IMPORTED_CONFIG_*` block maps every config to Debug, and
   `imgui_backends` hardcodes `${imgui_PACKAGE_FOLDER_DEBUG}` — downstream
   consumers will build Release.
2. `imgui_backends` compiles sources from absolute Conan package-folder paths,
   so `pf::ui_imgui` has no valid `INSTALL_INTERFACE`.
3. `imtui` is fetched via `FetchContent` at configure time; it should become a
   local Conan recipe under `recipes/`.

Also outstanding: `pf::SettingsItem` doesn't unregister on destruction, the
settings registry singleton makes tests share state, and there's no CI.

## Project structure

```
include/pf/            — public API (namespace pf) ........... SHIPPED
  dc/                    sender/receiver ports
  log/                   log, log_level
  settings/              registry, item, dirty_tracker
  cli/                   command_registry
  ui/                    imgui_app, imgui_log_sink, settings_editor
src/pf/                — implementation ..................... SHIPPED

examples/include/demo/ — example settings structs + port payloads (namespace demo)
app/                   — demo executables (main, cli_server, tui, imtui)
tutorial/ftxui/        — progressive FTXUI tutorial
test/                  — GTest unit tests
resources/             — default settings.xml + bundled UI font
```
