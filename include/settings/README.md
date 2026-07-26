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
        // Called automatically after every loadXml / setItemValue
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

Consumers (`app/main.cpp`, `app/cli_server.cpp`, tests) just `#include` the header — no per-binary re-declaration. The string key is how you address the item via the registry and the XML file (as the matching top-level element under `<Settings>`).

---

## SettingsRegistry API

All access goes through the singleton:

```cpp
auto& reg = SettingsRegistry::instance();
```

### Load from XML file

```cpp
reg.loadXml("settings.xml");
```

Reads each top-level `<Settings>` child element, matched by registered item key, and deserializes it into that item — internally still going through the same `nlohmann::json` `loader` every other code path uses (`setItemValue`, `resetItem`, the CLI, the editor); XML is purely a file-format concern layered on top. Field types (bool/number/string/array/object) are inferred from the item's default-constructed JSON shape, so the file itself carries no type annotations. Items with `DirtyTracker` have `dirty()` called automatically after loading. If the file doesn't exist, the current in-memory (default) values are written out as a new file instead.

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

### Save to XML file

```cpp
reg.saveXml("out.xml");
```

Serializes all registered items to an XML file, always as child elements. The output can be loaded back with `loadXml`.

---

## XML file format

Each top-level element under `<Settings>` matches a registered item name. Only the fields you want to override need to be present — missing fields keep their C++ default values. Scalar fields are child elements holding text; array/vector fields (e.g. `clearColor`) are a single element with whitespace-separated values; nested objects (e.g. `endpoint`) are nested elements.

```xml
<Settings>
    <AppConfig>
        <appName>RegistryDemo</appName>
        <windowWidth>1920</windowWidth>
        <fullscreen>false</fullscreen>
    </AppConfig>
    <RenderSettings>
        <clearColor>0.2 0.2 0.25 1.0</clearColor>
        <shadowQuality>high</shadowQuality>
        <maxLights>16</maxLights>
    </RenderSettings>
    <NetworkConfig>
        <endpoint>
            <host>192.168.1.100</host>
            <port>9090</port>
        </endpoint>
        <timeoutMs>3000</timeoutMs>
        <enableTLS>true</enableTLS>
    </NetworkConfig>
</Settings>
```

On **read**, attributes and child elements are treated interchangeably for scalar/array fields — `<AppConfig windowWidth="1920">` works the same as a `<windowWidth>` child element, and if a field is given as both, the child element wins. On **write**, the registry always emits child elements, so re-saved files are uniform regardless of how the source file was hand-edited.

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
| `coding export <file>`               | Write all items to an XML file                |
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

app> save out.xml
Saved to "out.xml"
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

=== Step 2: loadXml ===
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

=== Step 6: saveXml ===
  Saved current state to out.xml

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
    settings.xml            — default overrides (copied to build output)
```
