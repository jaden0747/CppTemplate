#pragma once

// ---------------------------------------------------------------------------
// counter_demo.hpp — CounterDemo: self-contained Sender/Receiver ports demo.
//
// A background thread produces an incrementing counter and ships it to the main
// thread through a dc::SenderPort / dc::ReceiverPort pipeline (per the project
// ground rule that inter-thread data flows through ports, never shared globals).
//
//   CounterDemo demo;
//   demo.start();                 // launch producer thread
//   while (app.running()) {
//       demo.update();            // promote latest value (main thread)
//       ...
//       demo.drawPanel("Data Ports");
//       demo.cleanup();           // end-of-frame
//   }
//   demo.stop();                  // join (also happens in destructor)
// ---------------------------------------------------------------------------

#include "core/dc/data_container.hpp"
#include "core/dc/interface/counter_data.hpp"
#include "core/log.hpp"

#include <imgui.h>

#include <atomic>
#include <chrono>
#include <thread>

class CounterDemo
{
public:
    using CounterData = dc::CounterData;

    CounterDemo() { m_receiver.connect(m_sender); }
    ~CounterDemo() { stop(); }

    CounterDemo(const CounterDemo&)            = delete;
    CounterDemo& operator=(const CounterDemo&) = delete;

    void start()
    {
        if (m_thread.joinable())
            return;
        m_running.store(true);
        m_thread = std::thread([this] { run(); });
    }

    void stop()
    {
        if (!m_thread.joinable())
            return;
        m_running.store(false);
        m_thread.join();
    }

    // Main-thread frame hooks.
    void update() { m_receiver.update(); }
    void cleanup() { m_receiver.cleanup(); }

    void drawPanel(const char* title)
    {
        ImGui::Begin(title);
        if (const CounterData* d = m_receiver.getData())
        {
            ImGui::Text("counter   : %d", d->value);
            ImGui::Text("timestamp : %.3f", d->timestamp);
        }
        else
        {
            ImGui::TextDisabled("waiting for data...");
        }
        ImGui::End();
    }

private:
    void run()
    {
        m_sender.connectMempool(m_pool);
        auto log = Log::get("ports");
        log->info("Background sender started");

        int count = 0;
        while (m_running.load())
        {
            if (CounterData* slot = m_sender.reserve())
            {
                slot->value     = count++;
                slot->timestamp = static_cast<float>(
                    std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count());
                m_sender.deliver();
                if (count % 50 == 0)
                    log->debug("Sent counter={}", count);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        log->info("Background sender stopped");
    }

    dc::Mempool<CounterData>      m_pool{4};
    dc::SenderPort<CounterData>   m_sender;
    dc::ReceiverPort<CounterData> m_receiver;
    std::atomic<bool>             m_running{false};
    std::thread                   m_thread;
};
