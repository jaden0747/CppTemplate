// ---------------------------------------------------------------------------
// test_settings.cpp — Unit tests for SettingsRegistry + SettingsItem
// ---------------------------------------------------------------------------
#include "settings/settings_item.hpp"
#include "settings/examples/app_config.hpp"
#include "settings/examples/render_settings.hpp"
#include "settings/examples/network_config.hpp"

#include <gtest/gtest.h>
#include <fstream>

// Note: SettingsRegistry is a singleton, so tests share state. The g_app /
// g_render / g_network globals come from the example headers (inline vars).

TEST(Settings, DefaultValues)
{
    EXPECT_EQ(g_app->appName, "MyApp");
    EXPECT_EQ(g_app->windowWidth, 1280);
    EXPECT_EQ(g_app->windowHeight, 720);
    EXPECT_FALSE(g_app->fullscreen);
    EXPECT_FLOAT_EQ(g_app->targetFps, 60.0f);
}

TEST(Settings, LoadJson)
{
    // Write a temporary JSON file
    const char* json = R"({
        "AppConfig": {
            "appName": "TestApp",
            "windowWidth": 800,
            "windowHeight": 600,
            "fullscreen": true,
            "targetFps": 30.0
        },
        "RenderSettings": {
            "clearColor": [1.0, 0.0, 0.0, 1.0],
            "wireframe": true,
            "shadowQuality": "low",
            "maxLights": 4
        }
    })";

    {
        std::ofstream f("test_settings_tmp.json");
        f << json;
    }

    SettingsRegistry::instance().loadJson("test_settings_tmp.json");

    EXPECT_EQ(g_app->appName, "TestApp");
    EXPECT_EQ(g_app->windowWidth, 800);
    EXPECT_TRUE(g_app->fullscreen);
    EXPECT_FLOAT_EQ(g_app->targetFps, 30.0f);

    EXPECT_TRUE(g_render->wireframe);
    EXPECT_EQ(g_render->shadowQuality, "low");
    EXPECT_EQ(g_render->maxLights, 4);
    EXPECT_GT(g_render->getModifiedCount(), 0u);

    std::remove("test_settings_tmp.json");
}

TEST(Settings, SetItemValue)
{
    auto& reg = SettingsRegistry::instance();

    bool ok = reg.setItemValue<std::string>("AppConfig", "appName", "Modified");
    EXPECT_TRUE(ok);
    EXPECT_EQ(g_app->appName, "Modified");

    ok = reg.setItemValue<int>("AppConfig", "windowWidth", 1920);
    EXPECT_TRUE(ok);
    EXPECT_EQ(g_app->windowWidth, 1920);

    // Non-existent key
    ok = reg.setItemValue<int>("AppConfig", "nonExistent", 0);
    EXPECT_FALSE(ok);
}

TEST(Settings, GetItemValue)
{
    auto& reg = SettingsRegistry::instance();

    std::string name;
    bool ok = reg.getItemValue<std::string>("AppConfig", "appName", name);
    EXPECT_TRUE(ok);
    EXPECT_EQ(name, g_app->appName);
}

TEST(Settings, DirtyTracker)
{
    uint32_t before = g_render->getModifiedCount();
    auto& reg = SettingsRegistry::instance();
    reg.setItemValue<bool>("RenderSettings", "wireframe", !g_render->wireframe);
    EXPECT_EQ(g_render->getModifiedCount(), before + 1);
}

TEST(Settings, SaveAndReload)
{
    auto& reg = SettingsRegistry::instance();

    reg.setItemValue<std::string>("AppConfig", "appName", "SaveTest");
    reg.saveJson("test_save_tmp.json");

    // Modify in memory
    reg.setItemValue<std::string>("AppConfig", "appName", "Changed");
    EXPECT_EQ(g_app->appName, "Changed");

    // Reload
    reg.loadJson("test_save_tmp.json");
    EXPECT_EQ(g_app->appName, "SaveTest");

    std::remove("test_save_tmp.json");
}

TEST(Settings, EnumOptionsAutoRegistered)
{
    // AppConfig::registerMetadata() runs at SettingsItem static-init time, so
    // the logLevel combo options exist with no manual registerEnumOptions call.
    auto& reg  = SettingsRegistry::instance();
    const auto* opts = reg.getEnumOptions("AppConfig", "logLevel");
    ASSERT_NE(opts, nullptr);
    EXPECT_EQ(*opts, logLevelOptions());
    EXPECT_EQ(opts->front(), "trace");
    EXPECT_EQ(opts->back(), "critical");

    // A field without registered options returns nullptr.
    EXPECT_EQ(reg.getEnumOptions("AppConfig", "appName"), nullptr);
}

TEST(Settings, GetDefaultJson)
{
    auto& reg = SettingsRegistry::instance();

    nlohmann::json def = reg.getDefaultJson("AppConfig");
    EXPECT_EQ(def["appName"], "MyApp");
    EXPECT_EQ(def["windowWidth"], 1280);

    // Unknown item -> null json
    EXPECT_TRUE(reg.getDefaultJson("NoSuchItem").is_null());
}

TEST(Settings, ResetItemToDefault)
{
    auto& reg = SettingsRegistry::instance();

    reg.setItemValue<std::string>("AppConfig", "appName", "Changed");
    reg.setItemValue<int>("AppConfig", "windowWidth", 999);
    EXPECT_EQ(g_app->appName, "Changed");

    EXPECT_TRUE(reg.resetItem("AppConfig"));
    EXPECT_EQ(g_app->appName, "MyApp");
    EXPECT_EQ(g_app->windowWidth, 1280);
    EXPECT_EQ(g_app->windowHeight, 720);

    EXPECT_FALSE(reg.resetItem("NoSuchItem"));
}

TEST(Settings, ResetSingleMemberToDefault)
{
    auto& reg = SettingsRegistry::instance();

    reg.setItemValue<std::string>("AppConfig", "appName", "Changed");
    reg.setItemValue<int>("AppConfig", "windowWidth", 999);

    // Revert only appName; windowWidth stays modified.
    EXPECT_TRUE(reg.resetItemValue("AppConfig", "appName"));
    EXPECT_EQ(g_app->appName, "MyApp");
    EXPECT_EQ(g_app->windowWidth, 999);

    EXPECT_FALSE(reg.resetItemValue("AppConfig", "nonExistent"));

    reg.resetItem("AppConfig"); // leave clean for other tests
}
