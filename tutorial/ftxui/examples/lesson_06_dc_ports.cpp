// ---------------------------------------------------------------------------
// lesson_06_dc_ports.cpp — feeding a live FTXUI view from a background
// thread via pf::dc::SenderPort/ReceiverPort.
//
// Project ground rule: data crossing threads goes through a pf::dc:: port
// pipeline, never a shared global/queue/condition variable. See
// include/pf/dc/data_container.hpp and app/counter_demo.hpp (the same
// pattern feeding the ImGui app's debug panel instead of a terminal).
// ---------------------------------------------------------------------------

#include "lessons.hpp"

#include <pf/dc/data_container.hpp>
#include <demo/dc/tutorial_tick.hpp>
#include <pf/log/log.hpp>

#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <atomic>
#include <chrono>
#include <thread>

using namespace ftxui;

namespace tutorial
{

void RunLesson06()
{
    static auto log = pf::Log::get("tutorial.lesson06");

    auto screen = ScreenInteractive::Fullscreen();

    // Pool + sender are the producer side; both would normally be file-scope
    // globals so any producer can reach them. Kept local here since the
    // producer thread lives entirely inside this function.
    pf::dc::Mempool<demo::TutorialTick>    pool(4);
    pf::dc::SenderPort<demo::TutorialTick> sender;
    sender.connectMempool(pool);

    // Receiver lives at the consumer site — here, the render loop.
    pf::dc::ReceiverPort<demo::TutorialTick> receiver;
    receiver.connect(sender);

    std::atomic<bool> running{true};
    std::thread       producer(
        [&]
        {
            using clock      = std::chrono::steady_clock;
            const auto start = clock::now();
            int        tick  = 0;
            while (running.load())
            {
                // reserve() → fill → deliver(); reserve() returns nullptr when
                // the pool is exhausted (every slot still held by a receiver) —
                // drop the send rather than block the producer.
                if (demo::TutorialTick* slot = sender.reserve())
                {
                    slot->tick           = tick++;
                    slot->elapsedSeconds = std::chrono::duration<double>(clock::now() - start).count();
                    sender.deliver();
                }
                screen.PostEvent(Event::Custom); // wake the render loop
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
            }
        });
    log->info("pf::dc:: producer thread started");

    auto renderer = Renderer(
        [&]
        {
            // Main-loop pattern from data_container.hpp: update() promotes the
            // latest delivered slot, cleanup() clears the "new" flag. Both
            // happen here since this lambda IS the render loop's one frame hook.
            receiver.update();
            Element body;
            if (const demo::TutorialTick* d = receiver.getData())
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
            receiver.cleanup();

            return vbox({
                       text("Cross-thread updates via pf::dc:: ports") | bold,
                       text("A background thread posts a demo::TutorialTick every 150ms") | dim,
                       separator(),
                       body,
                       separator(),
                       text("Esc to return to the menu") | dim,
                   }) |
                   border;
        });

    auto component = CatchEvent(
        renderer,
        [&](Event event)
        {
            if (event == Event::Escape)
            {
                screen.Exit();
                return true;
            }
            return false;
        });

    screen.Loop(component);

    running.store(false);
    producer.join();
    log->info("pf::dc:: producer thread stopped");
}

} // namespace tutorial
