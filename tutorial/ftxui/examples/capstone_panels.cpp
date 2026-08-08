#include "capstone_panels.hpp"
#include "tutorial_settings.hpp"

#include "settings/settings_registry.hpp"

#include <ftxui/component/component_options.hpp>
#include <ftxui/dom/elements.hpp>

#include <chrono>
#include <stdexcept>

using namespace ftxui;

namespace tutorial
{

// -----------------------------------------------------------------------
// DataPanel
// -----------------------------------------------------------------------

DataPanel::DataPanel(ScreenInteractive& screen)
{
    m_sender.connectMempool(m_pool);
    m_receiver.connect(m_sender);

    m_thread = std::thread(
        [this, &screen]
        {
            using clock      = std::chrono::steady_clock;
            const auto start = clock::now();
            int        tick  = 0;
            while (m_running.load())
            {
                if (dc::TutorialTick* slot = m_sender.reserve())
                {
                    slot->tick           = tick++;
                    slot->elapsedSeconds = std::chrono::duration<double>(clock::now() - start).count();
                    m_sender.deliver();
                }
                screen.PostEvent(Event::Custom);
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }
        });

    m_component = Renderer(
        [this]
        {
            m_receiver.update();
            Element body;
            if (const dc::TutorialTick* d = m_receiver.getData())
            {
                body = vbox({
                    text("tick    : " + std::to_string(d->tick)),
                    text("elapsed : " + std::to_string(d->elapsedSeconds) + "s"),
                });
            }
            else
            {
                body = text("waiting for data...") | dim;
            }
            m_receiver.cleanup();
            return body;
        });
}

DataPanel::~DataPanel()
{
    m_running.store(false);
    if (m_thread.joinable())
        m_thread.join();
}

// -----------------------------------------------------------------------
// FtxuiLogSink
// -----------------------------------------------------------------------

FtxuiLogSink::FtxuiLogSink(size_t maxEntries)
    : m_maxEntries(maxEntries)
{
}

std::vector<LogEntry> FtxuiLogSink::snapshot()
{
    std::lock_guard<std::mutex> lock(mutex_);
    return {m_entries.begin(), m_entries.end()};
}

void FtxuiLogSink::sink_it_(const spdlog::details::log_msg& msg)
{
    spdlog::memory_buf_t formatted;
    formatter_->format(msg, formatted);
    std::string text(formatted.data(), formatted.size());
    if (!text.empty() && text.back() == '\n')
        text.pop_back();

    m_entries.push_back({msg.level, std::move(text)});
    while (m_entries.size() > m_maxEntries)
        m_entries.pop_front();
}

namespace
{

Color LevelColor(spdlog::level::level_enum level)
{
    switch (level)
    {
    case spdlog::level::trace:
        return Color::GrayDark;
    case spdlog::level::debug:
        return Color::Cyan;
    case spdlog::level::info:
        return Color::Green;
    case spdlog::level::warn:
        return Color::Yellow;
    case spdlog::level::err:
        return Color::Red;
    case spdlog::level::critical:
        return Color::Magenta;
    default:
        return Color::White;
    }
}

} // namespace

// -----------------------------------------------------------------------
// LogPanel
// -----------------------------------------------------------------------

LogPanel::LogPanel(ScreenInteractive& screen)
{
    m_sink = std::make_shared<FtxuiLogSink>(10);
    m_log  = Log::get("tutorial.capstone");
    m_log->set_level(spdlog::level::trace);
    Log::addSink(m_sink);
    m_sink->set_level(spdlog::level::trace);

    m_thread = std::thread(
        [this, &screen]
        {
            int i = 0;
            while (m_running.load())
            {
                switch (i % 5)
                {
                case 0:
                    m_log->trace("trace #{}", i);
                    break;
                case 1:
                    m_log->debug("debug #{}", i);
                    break;
                case 2:
                    m_log->info("info #{}", i);
                    break;
                case 3:
                    m_log->warn("warn #{}", i);
                    break;
                default:
                    m_log->error("error #{}", i);
                    break;
                }
                ++i;
                screen.PostEvent(Event::Custom);
                std::this_thread::sleep_for(std::chrono::milliseconds(400));
            }
        });

    auto sink   = m_sink; // copy of the shared_ptr, kept alive by the render lambda itself
    m_component = Renderer(
        [sink]
        {
            Elements lines;
            for (const auto& entry : sink->snapshot())
                lines.push_back(text(entry.text) | color(LevelColor(entry.level)));
            return vbox(std::move(lines));
        });
}

LogPanel::~LogPanel()
{
    m_running.store(false);
    if (m_thread.joinable())
        m_thread.join();
}

// -----------------------------------------------------------------------
// SettingsPanel
// -----------------------------------------------------------------------

SettingsPanel::SettingsPanel(std::string autoSavePath)
    : m_autoSavePath(std::move(autoSavePath))
    , m_root(Container::Vertical({}))
{
    auto& registry = SettingsRegistry::instance();
    for (const auto& [name, entry] : registry.getItems())
        m_itemNames.push_back(name);

    MenuOption itemMenuOption;
    itemMenuOption.entries   = &m_itemNames;
    itemMenuOption.selected  = &m_selectedItem;
    itemMenuOption.on_change = [this] { rebuild(); };
    m_itemMenu               = Menu(itemMenuOption);

    m_saveButton   = Button("Save", [this] { SettingsRegistry::instance().saveXml(m_autoSavePath); });
    m_reloadButton = Button(
        "Reload",
        [this]
        {
            SettingsRegistry::instance().loadXml(m_autoSavePath);
            rebuild();
        });
    m_resetButton = Button(
        "Reset to Defaults",
        [this]
        {
            if (!m_itemNames.empty())
            {
                SettingsRegistry::instance().resetItem(m_itemNames[m_selectedItem]);
                rebuild();
            }
        });

    rebuild(); // safe now: m_itemMenu/m_saveButton/m_reloadButton/m_resetButton all exist

    m_component = Renderer(
        m_root,
        [this]
        {
            Elements fieldRows;
            for (const auto& w : m_fieldWidgets)
                fieldRows.push_back(w->Render());

            return vbox({
                text("Items:"),
                m_itemMenu->Render() | frame | size(HEIGHT, EQUAL, 3),
                separator(),
                vbox(std::move(fieldRows)),
                separator(),
                hbox({m_saveButton->Render(), m_reloadButton->Render(), m_resetButton->Render()}),
            });
        });
}

void SettingsPanel::rebuild()
{
    auto& registry = SettingsRegistry::instance();
    m_root->DetachAllChildren();
    m_mirrors.clear();
    m_fieldWidgets.clear();
    m_root->Add(m_itemMenu);

    if (!m_itemNames.empty())
    {
        const std::string& itemName = m_itemNames[m_selectedItem];
        const auto&        entry    = registry.getItems().at(itemName);
        nlohmann::json     j        = entry.saver(entry.ptr);

        m_mirrors.reserve(j.size());
        for (const auto& [key, val] : j.items())
        {
            FieldMirror m;
            m.key = key;
            if (val.is_boolean())
            {
                m.isBool  = true;
                m.boolVal = val.get<bool>();
            }
            else if (val.is_number_integer())
            {
                m.textVal = std::to_string(val.get<int>());
            }
            else if (val.is_number_float())
            {
                m.textVal = std::to_string(val.get<float>());
            }
            else if (val.is_string())
            {
                m.textVal = val.get<std::string>();
            }
            else
            {
                continue; // arrays/objects: out of scope for this panel
            }
            m_mirrors.push_back(std::move(m));
        }

        auto labeledInput = [](const std::string& key, InputOption opt)
        {
            // Default multiline=true still calls on_enter but ALSO inserts a
            // literal '\n' into the content — force single-line so Enter just commits.
            opt.multiline   = false;
            Component input = Input(opt);
            return Renderer(input, [key, input] { return hbox({text(key + ": "), input->Render()}); });
        };

        for (size_t i = 0; i < m_mirrors.size(); ++i)
        {
            FieldMirror&          m        = m_mirrors[i];
            const nlohmann::json& original = j.at(m.key);
            Component             field;

            if (m.isBool)
            {
                CheckboxOption opt;
                opt.on_change = [itemName, &m]
                { SettingsRegistry::instance().setItemValue<bool>(itemName, m.key, m.boolVal); };
                // FTXUI 5.0.0 only implements the (label, checked, option) overload of
                // Checkbox(), not the CheckboxOption-only one declared in its header.
                field = Checkbox(m.key, &m.boolVal, opt);
            }
            else if (original.is_number_integer())
            {
                InputOption opt;
                opt.content  = &m.textVal;
                opt.on_enter = [itemName, &m]
                {
                    try
                    {
                        int parsed = std::stoi(m.textVal);
                        SettingsRegistry::instance().setItemValue<int>(itemName, m.key, parsed);
                        // Re-derive the display text from the parsed value — otherwise
                        // e.g. "12abc" stays on screen even though 12 was what committed.
                        m.textVal = std::to_string(parsed);
                    }
                    catch (const std::exception&)
                    {
                    }
                };
                field = labeledInput(m.key, opt);
            }
            else if (original.is_number_float())
            {
                InputOption opt;
                opt.content  = &m.textVal;
                opt.on_enter = [itemName, &m]
                {
                    try
                    {
                        float parsed = std::stof(m.textVal);
                        SettingsRegistry::instance().setItemValue<float>(itemName, m.key, parsed);
                        m.textVal = std::to_string(parsed);
                    }
                    catch (const std::exception&)
                    {
                    }
                };
                field = labeledInput(m.key, opt);
            }
            else // string
            {
                InputOption opt;
                opt.content  = &m.textVal;
                opt.on_enter = [itemName, &m]
                { SettingsRegistry::instance().setItemValue<std::string>(itemName, m.key, m.textVal); };
                field = labeledInput(m.key, opt);
            }

            m_fieldWidgets.push_back(field);
            m_root->Add(field);
        }
    }

    m_root->Add(m_saveButton);
    m_root->Add(m_reloadButton);
    m_root->Add(m_resetButton);
}

} // namespace tutorial
