# Ground Rules

- Any configurable value or static/global variable **must** live in a `SettingsItem<T>` struct — never as a hardcoded constant, bare global, or a separate config mechanism.
- Any data passed between threads **must** go through a `dc::SenderPort` / `dc::ReceiverPort` pipeline — never via shared globals, queues, or condition variables.

# Build

```bash
conan install . --build=missing -s build_type=Debug
cmake --preset conan-debug
cmake --build --preset conan-debug
ctest --preset conan-debug
./build/Debug/tests --gtest_filter=DataContainer.*   # single test
```

Targets: `app` (ImGui window), `cli_server` (telnet CLI), `tui` (FTXUI component gallery demo), `tui_imtui` (ImTui ncurses demo, non-Windows only — see below), `ftxui_tutorial` (progressive FTXUI tutorial — see `tutorial/ftxui/TUTORIAL.md`), `tests` (GTest). After build, `settings.xml` and `font/` are copied next to `app`/`cli_server`.

`imtui` isn't on ConanCenter, so `cmake --preset conan-debug`'s configure step fetches it (and its patched Dear ImGui fork) via CMake `FetchContent` — needs network access on a clean configure. The whole `tui_imtui` target (and its `ncurses` Conan dependency) is dropped on Windows — ImTui's ncurses backend there depends on pdcurses plus a hand-written shim header, which wasn't worth maintaining; see the `if(NOT WIN32)` block in `CMakeLists.txt`.

# Code Style

clang-format: Allman braces, 4-space indent, 120-column limit, pointer-left. C++17.

# Settings (`include/settings/`)

Define a struct with `NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT` and declare an `inline SettingsItem<T>` global next to it in the header — it self-registers at static-init time, and the `inline` keyword gives one shared definition across every binary that includes the header. See `include/settings/examples/` for canonical patterns (flat, nested, `DirtyTracker`).

```cpp
// my_settings.hpp
struct MySettings {
    int count = 10;
    NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(MySettings, count)
};
inline SettingsItem<MySettings> g_my{"MySettings"};  // key matches settings.xml
```

- Access: `g_my->count`, `g_my.data()`
- Settings persist as XML (`settings.xml`), not JSON — the in-memory/wire model is still `nlohmann::json` throughout (`loader`/`saver`/`setItemValue`/CLI all unchanged); only the file format changed. `SettingsRegistry::loadXml`/`saveXml` convert via libxml2, schema-guided by each item's default-constructed JSON shape so field types (bool/number/string/array/object) come from the struct, not the file. Root element `<Settings>`, one child per item key; scalar fields are child-element text, arrays (e.g. `clearColor`) are whitespace-separated text in one element, nested objects are nested elements. Reading accepts attributes and child elements interchangeably (child element wins if both present); writing always emits child elements. Missing file on load → current in-memory defaults are written out as a new file.
- Load/save the whole registry: `SettingsRegistry::instance().loadXml/saveXml("settings.xml")`
- Targeted update: `SettingsRegistry::instance().setItemValue<int>("MySettings", "count", 42)`
- Revert to defaults (the struct's member initializers, i.e. `T{}`): `resetItem("MySettings")` for a whole item, `resetItemValue("MySettings", "count")` for one member; `getDefaultJson("MySettings")` returns the defaults as JSON. Both resets apply via `loader`/`onLoaded` so change callbacks fire.
- Editor metadata (enum combo options, etc.): add a `static void registerMetadata(SettingsRegistry&, const std::string& key)` to the struct — `SettingsItem<T>` detects and calls it at static-init time, so there's no manual `registerEnumOptions` wiring in `main()`. See `AppConfig` (`logLevel` options derived from the enum via `logLevelOptions()`).
- Change detection: inherit `DirtyTracker`, then register a callback with `setOnChanged()` — fired automatically after every load/set. Register callbacks after app resources (windows, GL context) are ready, not at static-init time.
- CLI access: `buildRootMenu()` exposes the registry via `coding list/get/set/export`; connect with `telnet localhost 5000`

# Sender/Receiver Ports (`include/core/dc/data_container.hpp`, namespace `dc`)

Pool and sender are file-scope; receiver lives at the consumer site.

```cpp
dc::Mempool<T>    g_pool(4);
dc::SenderPort<T> g_sender;
g_sender.connectMempool(g_pool);          // producer thread

dc::ReceiverPort<T> receiver;
receiver.connect(g_sender);               // consumer thread
```

Producer: `reserve()` → fill → `deliver()`. Returns `nullptr` when pool is exhausted — drop the send.

Consumer (main loop):
```cpp
receiver.update();          // promote pending → active
if (receiver.hasNewData())
    use(receiver.getData()); // valid until next update()
receiver.cleanup();         // clear hasNewData; hasData() persists
```

Latest-wins: multiple sends before `update()` collapse to the last value.

Payload types (`T`) are the contract between producer and consumer — define each as a plain struct in its own header under `include/core/dc/interface/` (namespace `dc`), one per file, so both threads share a single definition. A payload must be default-constructible and copy-assignable (`Mempool` resets slots with `T{}`; fan-out copies them).

# Logging (`include/core/log.hpp`)

```cpp
static auto log = Log::get("mymodule");   // safe before Log::init()
log->info("value={}", x);
```

Init once after settings load: `Log::init(g_app->logLevel, "app.log")`.

ImGui sink (app only):
```cpp
auto sink = std::make_shared<ImGuiLogSink_mt>();
Log::addSink(sink);      // propagates to all existing loggers
sink->draw("Log");       // call each frame inside ImGui
```

# Settings Editor (`include/settings/settings_editor.hpp`)

Generic ImGui panel that introspects `SettingsRegistry` at runtime — no per-field boilerplate. Renders type-appropriate widgets (checkbox, drag int/float, text input, `ColorEdit4` for 4-float arrays whose key contains "olor", tree nodes for nested objects). Applies changes immediately via the registry's `loader`/`onLoaded` callbacks. Save and Reload buttons persist/restore `settings.xml`. A per-field **Reset** button appears next to any value that differs from its default; a **Reset to Defaults** button reverts the whole selected item.

```cpp
SettingsEditor::draw("Settings");  // call once per frame inside ImGui
```

New `SettingsItem<T>` structs appear automatically — no changes to the editor needed.

# ImGui App (`include/core/imgui_app.hpp`, `app/main.cpp`)

`ImGuiApp` is an RAII host that owns the GLFW window + Dear ImGui + OpenGL 3.3 (3.2 on macOS) lifecycle, including the fullscreen DockSpace and UI font. Construct it with an `ImGuiApp::Config`; the constructor throws `std::runtime_error` on failure. Draw panels between `beginFrame()` and `endFrame(clearColor)`:

```cpp
ImGuiApp app({width, height, title, fontPath, fontSize});
while (app.running())
{
    app.beginFrame();          // poll events, new frame, dockspace
    MyPanel::draw();           // ... ImGui panels ...
    app.endFrame(clearColor);  // render + swap
}
```

`main.cpp` is just orchestration: load settings → `setupLogging()` → construct `ImGuiApp` → `installSettingsBindings()` (wires `g_app`/`g_render` `onChanged` callbacks to window/GL state) → frame loop. The threaded ports demo lives in `app/counter_demo.hpp` (`CounterDemo`). ImGui (docking branch) comes from the `imgui/*-docking` Conan package — no GLAD needed. The package only compiles imgui core into `imgui::imgui`; backend (`imgui_impl_*`) and `imgui_stdlib` sources ship as package resources with no CMakeDeps variable, so `CMakeLists.txt` compiles them itself from `${imgui_PACKAGE_FOLDER_DEBUG}/res/{bindings,misc/cpp}` into a local `imgui_backends` target.
