# Prototype

Minimal C++ application framework with three core features:

1. **Settings System** — JSON-backed runtime configuration with typed accessors and change tracking
2. **Sender/Receiver Ports** — Zero-copy multithreaded data transfer via ring-buffer mempool
3. **Debug Window** — ImGui over OpenGL 3.3 + GLFW for runtime inspection

## Build

```bash
# Install dependencies
conan install . --output-folder=build/Debug --build=missing -s build_type=Debug

# Configure (first time or after CMakeLists changes)
cmake --preset conan-debug

# Build
cmake --build --preset conan-debug

# Test
ctest --preset conan-debug
```

## Targets

| Target | Description |
|--------|-------------|
| `app` | Main window with ImGui panels showing settings + data ports |
| `cli_server` | Telnet CLI for inspecting/modifying settings at runtime |
| `tests` | GTest unit tests for data_container, settings, and logging |

## Project Structure

```
include/
  settings/       — SettingsRegistry, SettingsItem, DirtyTracker,
                    command_registry, settings_editor, examples/
  core/           — log, imgui_log_sink, imgui_app (window/ImGui/GL host) +
                    dc/ (data_container ports + interface/ payload types, namespace dc)
app/
  main.cpp        — application entry point (orchestration only)
  counter_demo.hpp— Sender/Receiver ports demo (background thread + panel)
  cli_server.cpp  — CLI/Telnet settings server
test/
  test_data_container.cpp
  test_settings.cpp
  test_log.cpp
resources/
  settings.json   — Default settings file
  font/           — Bundled UI font
```
