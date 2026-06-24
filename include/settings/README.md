# Settings & Command Registry

A lightweight, header-only settings framework that provides:

- **`SettingsRegistry`** — singleton that maps string keys to typed settings objects
- **`SettingsItem<T>`** — RAII wrapper that auto-registers a settings struct at static-init time
- **`DirtyTracker`** — mixin base for change-count bookkeeping and side-effect callbacks
- **`buildRootMenu()`** — builds a `daniele77/cli` menu exposing the full registry over a local terminal or Telnet

---

## Quick Start

### 1. Define a settings struct

```cpp
// include/settings/examples/app_config.hpp
#include <nlohmann/json.hpp>

struct AppConfig
{
    std::string appName     = "MyApp";
    int         windowWidth = 1280;
    bool        fullscreen  = false;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(AppConfig, appName, windowWidth, fullscreen)
};
```

Use `NLOHMANN_DEFINE_TYPE_INTRUSIVE` to opt in to JSON serialization.
All public fields listed become JSON members.

### 2. Add change-notification (optional)

Inherit `DirtyTracker` to get a `modifiedCount` that increments on every load/set, and override `onChanged()` for custom side effects:

```cpp
// include/settings/examples/render_settings.hpp
#include "settings/dirty_tracker.hpp"

struct RenderSettings : DirtyTracker
{
    std::string shadowQuality = "medium";   // "low" | "medium" | "high"
    int         maxLights     = 8;

    NLOHMANN_DEFINE_TYPE_INTRUSIVE(RenderSettings, shadowQuality, maxLights)

protected:
    void onChanged() override
    {
        // Called automatically after every loadJson / setItemValue
        rebuildShadowMaps();
    }
};
```

### 3. Register instances

Declare an `inline SettingsItem<T>` next to the struct in its header. Registration happens at static-init time — before `main()` runs — and `inline` keeps it to one shared definition across every binary that includes the header:

```cpp
// app_config.hpp
inline SettingsItem<AppConfig>      g_app{"AppConfig"};       // key = "AppConfig"
inline SettingsItem<RenderSettings> g_render{"RenderSettings"};
```

Consumers (`app/main.cpp`, `app/cli_server.cpp`, tests) just `#include` the header — no per-binary re-declaration. The string key is how you address the item via the registry and the JSON file.

---

## SettingsRegistry API

All access goes through the singleton:

```cpp
auto& reg = SettingsRegistry::instance();
```

### Load from JSON file

```cpp
reg.loadJson("settings.json");
```

Reads each top-level key from the JSON file and deserializes it into the matching registered item.
Items with `DirtyTracker` have `dirty()` called automatically after loading.

### Read a single member

```cpp
int width = 0;
bool ok = reg.getItemValue<int>("AppConfig", "windowWidth", width);
```

Returns `false` if the item key or member name does not exist, or if the type does not match.

### Write a single member

```cpp
bool ok = reg.setItemValue<std::string>("RenderSettings", "shadowQuality", "high");
```

Serializes the full item to JSON, patches the member, deserializes back.
If the item has `DirtyTracker`, `dirty()` is called.

### Revert to defaults

The "default" of any item is its struct's member initializers — i.e. a
default-constructed `T{}`. No separate defaults file is needed.

```cpp
nlohmann::json def = reg.getDefaultJson("AppConfig");      // defaults as JSON
reg.resetItem("AppConfig");                                // revert the whole item
reg.resetItemValue("AppConfig", "windowWidth");            // revert one member
```

Both resets apply through the same `loader`/`onLoaded` path as a normal load, so
change callbacks and `DirtyTracker` fire. In the ImGui editor this is surfaced as
a per-field **Reset** button (shown only when a value differs from its default)
and a **Reset to Defaults** button for the selected item.

### Iterate all items

```cpp
for (const auto& [key, entry] : reg.getItems())
{
    nlohmann::json j = entry.saver(entry.ptr);   // serialize to JSON
    std::cout << key << ": " << j.dump(2) << "\n";
}
```

### Save to JSON file

```cpp
reg.saveJson("out.json");
```

Serializes all registered items to a JSON file. The output can be loaded back with `loadJson`.

---

## JSON file format

Each top-level key matches a registered item name. Only the fields you want to override need to be present — missing fields keep their C++ default values.

```json
{
  "AppConfig": {
    "appName": "RegistryDemo",
    "windowWidth": 1920,
    "fullscreen": false
  },
  "RenderSettings": {
    "shadowQuality": "high",
    "maxLights": 16
  },
  "NetworkConfig": {
    "endpoint": { "host": "192.168.1.100", "port": 9090 },
    "timeoutMs": 3000,
    "enableTLS": true
  }
}
```

C-style `// comments` are supported in the input file.

---

## CLI server (`cli_server`)

`buildRootMenu()` (in `command_registry.hpp`) creates a `cli::Menu` with a `coding` submenu that exposes the full registry over a local terminal and optionally over Telnet.

### Build & run

```sh
# build (from workspace root)
cmake --preset conan-debug   # or your preset name
cmake --build build --config Debug --target cli_server

# run on default port 5000
.\build\Debug\cli_server.exe

# run on a custom port
.\build\Debug\cli_server.exe --port 6000
```

### Connect via Telnet

```sh
telnet localhost 5000
# or
python test/test_client.py
```

### Available commands

| Command                              | Description                                   |
| ------------------------------------ | --------------------------------------------- |
| `coding list`                        | List all registered item keys                 |
| `coding list <item>`                 | List the member names of one item             |
| `coding get`                         | Dump all items as formatted JSON              |
| `coding get <item>`                  | Dump one item as formatted JSON               |
| `coding set <item> <member> <value>` | Set a member (value is JSON or a bare string) |
| `coding export <file>`               | Write all items to a JSON file                |
| `save <file>`                        | Alias for `coding export`                     |
| `status`                             | Print all registered item keys                |
| `echo <message>`                     | Echo a string back                            |

Example session:

```
app> coding list
AppConfig
NetworkConfig
RenderSettings

app> coding get RenderSettings
{
    "clearColor": [0.2, 0.2, 0.25, 1.0],
    "maxLights": 16,
    "shadowQuality": "high",
    "wireframe": false
}

app> coding set RenderSettings shadowQuality low
OK

app> save out.json
Saved to "out.json"
```

---

## `settings_demo` (standalone demo, no networking)

Runs through all registry features in order and asserts correctness. Useful as a sanity check after changes.

```sh
cmake --build build --config Debug --target settings_demo
.\build\Debug\settings_demo.exe
```

Expected output:

```
=== Step 1: Default values ===
  appName:      MyApp
  windowWidth:  1280
  fullscreen:   0
  shadowQuality: medium
  maxLights:     8
  modifiedCount: 0
  endpoint:     localhost:8080

=== Step 2: loadJson ===
[RenderSettings] Settings updated (modifiedCount=1)
  appName:       RegistryDemo
  shadowQuality: high
  modifiedCount: 1
  endpoint:      192.168.1.100:9090

=== Step 3: getItemValue ===
  AppConfig.windowWidth = 1920
  RenderSettings.shadowQuality = high

=== Step 4: setItemValue ===
  AppConfig.appName -> DemoApp
[RenderSettings] Settings updated (modifiedCount=2)
  RenderSettings.shadowQuality -> high
  RenderSettings.modifiedCount  = 2
  AppConfig.fullscreen -> 1

=== Step 5: getItems (iterate registry) ===
  [AppConfig]
    appName
    ...
  [RenderSettings]
    clearColor
    ...

=== Step 6: saveJson ===
  Saved current state to out.json

All assertions passed.
```

---

## File layout

```
include/settings/
    dirty_tracker.hpp       — DirtyTracker mixin base
    settings_registry.hpp   — SettingsRegistry singleton
    settings_item.hpp       — SettingsItem<T> RAII wrapper
    command_registry.hpp    — buildRootMenu() for CLI server
    examples/
        app_config.hpp      — flat scalars, no DirtyTracker
        render_settings.hpp — array + string enum, inherits DirtyTracker
        network_config.hpp  — nested sub-object (EndpointInfo)

src/settings_demo/
    settings_demo_main.cpp  — standalone demo (no networking)
    cli_server_main.cpp     — Telnet CLI server

resources/
    settings.json           — default overrides (copied to build output)
```
