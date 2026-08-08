// ---------------------------------------------------------------------------
// test_settings.cpp — Unit tests for pf::SettingsRegistry + SettingsItem
// ---------------------------------------------------------------------------
#include <pf/settings/settings_item.hpp>
#include <demo/app_config.hpp>
#include <demo/render_settings.hpp>
#include <demo/network_config.hpp>

#include <gtest/gtest.h>
#include <fstream>

// Note: pf::SettingsRegistry is a singleton, so tests share state. The demo::g_app /
// demo::g_render / demo::g_network globals come from the example headers (inline vars).

TEST(Settings, DefaultValues)
{
    EXPECT_EQ(demo::g_app->appName, "MyApp");
    EXPECT_EQ(demo::g_app->windowWidth, 1280);
    EXPECT_EQ(demo::g_app->windowHeight, 720);
    EXPECT_FALSE(demo::g_app->fullscreen);
    EXPECT_FLOAT_EQ(demo::g_app->targetFps, 60.0f);
}

TEST(Settings, LoadXml)
{
    // Write a temporary XML file. clearColor is whitespace-separated element
    // text; windowWidth demonstrates an attribute (mixed with sibling
    // elements) to exercise attribute-or-child-element parsing.
    const char* xml = R"(<?xml version="1.0" encoding="UTF-8"?>
    <Settings>
        <AppConfig windowWidth="800">
            <appName>TestApp</appName>
            <windowHeight>600</windowHeight>
            <fullscreen>true</fullscreen>
            <targetFps>30.0</targetFps>
        </AppConfig>
        <RenderSettings>
            <clearColor>1.0 0.0 0.0 1.0</clearColor>
            <wireframe>true</wireframe>
            <shadowQuality>low</shadowQuality>
            <maxLights>4</maxLights>
        </RenderSettings>
    </Settings>)";

    {
        std::ofstream f("test_settings_tmp.xml");
        f << xml;
    }

    pf::SettingsRegistry::instance().loadXml("test_settings_tmp.xml");

    EXPECT_EQ(demo::g_app->appName, "TestApp");
    EXPECT_EQ(demo::g_app->windowWidth, 800);
    EXPECT_TRUE(demo::g_app->fullscreen);
    EXPECT_FLOAT_EQ(demo::g_app->targetFps, 30.0f);

    EXPECT_TRUE(demo::g_render->wireframe);
    EXPECT_EQ(demo::g_render->shadowQuality, "low");
    EXPECT_EQ(demo::g_render->maxLights, 4);
    EXPECT_FLOAT_EQ(demo::g_render->clearColor[0], 1.0f);
    EXPECT_FLOAT_EQ(demo::g_render->clearColor[3], 1.0f);
    EXPECT_GT(demo::g_render->getModifiedCount(), 0u);

    std::remove("test_settings_tmp.xml");
}

TEST(Settings, SetItemValue)
{
    auto& reg = pf::SettingsRegistry::instance();

    bool ok = reg.setItemValue<std::string>("AppConfig", "appName", "Modified");
    EXPECT_TRUE(ok);
    EXPECT_EQ(demo::g_app->appName, "Modified");

    ok = reg.setItemValue<int>("AppConfig", "windowWidth", 1920);
    EXPECT_TRUE(ok);
    EXPECT_EQ(demo::g_app->windowWidth, 1920);

    // Non-existent key
    ok = reg.setItemValue<int>("AppConfig", "nonExistent", 0);
    EXPECT_FALSE(ok);
}

TEST(Settings, GetItemValue)
{
    auto& reg = pf::SettingsRegistry::instance();

    std::string name;
    bool ok = reg.getItemValue<std::string>("AppConfig", "appName", name);
    EXPECT_TRUE(ok);
    EXPECT_EQ(name, demo::g_app->appName);
}

TEST(Settings, DirtyTracker)
{
    uint32_t before = demo::g_render->getModifiedCount();
    auto& reg = pf::SettingsRegistry::instance();
    reg.setItemValue<bool>("RenderSettings", "wireframe", !demo::g_render->wireframe);
    EXPECT_EQ(demo::g_render->getModifiedCount(), before + 1);
}

TEST(Settings, SaveAndReload)
{
    auto& reg = pf::SettingsRegistry::instance();

    reg.setItemValue<std::string>("AppConfig", "appName", "SaveTest");
    reg.saveXml("test_save_tmp.xml");

    // Modify in memory
    reg.setItemValue<std::string>("AppConfig", "appName", "Changed");
    EXPECT_EQ(demo::g_app->appName, "Changed");

    // Reload
    reg.loadXml("test_save_tmp.xml");
    EXPECT_EQ(demo::g_app->appName, "SaveTest");

    std::remove("test_save_tmp.xml");
}

TEST(Settings, LoadXmlMissingFileWritesDefaults)
{
    std::remove("test_settings_missing.xml");

    pf::SettingsRegistry::instance().loadXml("test_settings_missing.xml");

    std::ifstream f("test_settings_missing.xml");
    EXPECT_TRUE(f.is_open());

    std::remove("test_settings_missing.xml");
}

TEST(Settings, EnumOptionsAutoRegistered)
{
    // demo::AppConfig::registerMetadata() runs at SettingsItem static-init time, so
    // the logLevel combo options exist with no manual registerEnumOptions call.
    auto& reg  = pf::SettingsRegistry::instance();
    const auto* opts = reg.getEnumOptions("AppConfig", "logLevel");
    ASSERT_NE(opts, nullptr);
    EXPECT_EQ(*opts, pf::logLevelOptions());
    EXPECT_EQ(opts->front(), "trace");
    EXPECT_EQ(opts->back(), "critical");

    // A field without registered options returns nullptr.
    EXPECT_EQ(reg.getEnumOptions("AppConfig", "appName"), nullptr);
}

TEST(Settings, GetDefaultJson)
{
    auto& reg = pf::SettingsRegistry::instance();

    nlohmann::json def = reg.getDefaultJson("AppConfig");
    EXPECT_EQ(def["appName"], "MyApp");
    EXPECT_EQ(def["windowWidth"], 1280);

    // Unknown item -> null json
    EXPECT_TRUE(reg.getDefaultJson("NoSuchItem").is_null());
}

TEST(Settings, ResetItemToDefault)
{
    auto& reg = pf::SettingsRegistry::instance();

    reg.setItemValue<std::string>("AppConfig", "appName", "Changed");
    reg.setItemValue<int>("AppConfig", "windowWidth", 999);
    EXPECT_EQ(demo::g_app->appName, "Changed");

    EXPECT_TRUE(reg.resetItem("AppConfig"));
    EXPECT_EQ(demo::g_app->appName, "MyApp");
    EXPECT_EQ(demo::g_app->windowWidth, 1280);
    EXPECT_EQ(demo::g_app->windowHeight, 720);

    EXPECT_FALSE(reg.resetItem("NoSuchItem"));
}

TEST(Settings, ResetSingleMemberToDefault)
{
    auto& reg = pf::SettingsRegistry::instance();

    reg.setItemValue<std::string>("AppConfig", "appName", "Changed");
    reg.setItemValue<int>("AppConfig", "windowWidth", 999);

    // Revert only appName; windowWidth stays modified.
    EXPECT_TRUE(reg.resetItemValue("AppConfig", "appName"));
    EXPECT_EQ(demo::g_app->appName, "MyApp");
    EXPECT_EQ(demo::g_app->windowWidth, 999);

    EXPECT_FALSE(reg.resetItemValue("AppConfig", "nonExistent"));

    reg.resetItem("AppConfig"); // leave clean for other tests
}
