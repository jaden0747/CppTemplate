// ---------------------------------------------------------------------------
// test_data_container.cpp — Unit tests for Mempool, SenderPort, ReceiverPort
// ---------------------------------------------------------------------------
#include <pf/dc/data_container.hpp>

#include <gtest/gtest.h>

#include <thread>
#include <vector>

TEST(Mempool, AcquireAndRelease)
{
    pf::dc::Mempool<int> pool(3);
    EXPECT_EQ(pool.poolCount(), 3u);

    auto* buf = pool.acquire();
    ASSERT_NE(buf, nullptr);
    EXPECT_EQ(buf->refs.load(), 1);

    pool.release(*buf);
    EXPECT_EQ(buf->refs.load(), 0);
}

TEST(Mempool, ExhaustsWhenAllHeld)
{
    pf::dc::Mempool<int> pool(2);

    auto* b1 = pool.acquire();
    auto* b2 = pool.acquire();
    ASSERT_NE(b1, nullptr);
    ASSERT_NE(b2, nullptr);

    // Pool exhausted
    auto* b3 = pool.acquire();
    EXPECT_EQ(b3, nullptr);

    // Release one, then acquire should succeed
    pool.release(*b1);
    auto* b4 = pool.acquire();
    EXPECT_NE(b4, nullptr);
}

TEST(SenderReceiver, BasicDelivery)
{
    pf::dc::Mempool<int>      pool(4);
    pf::dc::SenderPort<int>   sender;
    pf::dc::ReceiverPort<int> receiver;

    sender.connectMempool(pool);
    receiver.connect(sender);

    int* slot = sender.reserve();
    ASSERT_NE(slot, nullptr);
    *slot = 42;
    sender.deliver();

    receiver.update();
    EXPECT_TRUE(receiver.hasNewData());
    EXPECT_TRUE(receiver.hasData());
    EXPECT_EQ(*receiver.getData(), 42);

    receiver.cleanup();
    EXPECT_FALSE(receiver.hasNewData());
    EXPECT_TRUE(receiver.hasData());
}

TEST(SenderReceiver, MultipleReceivers)
{
    pf::dc::Mempool<int>      pool(4);
    pf::dc::SenderPort<int>   sender;
    pf::dc::ReceiverPort<int> r1, r2;

    sender.connectMempool(pool);
    r1.connect(sender);
    r2.connect(sender);

    int* slot = sender.reserve();
    *slot = 99;
    sender.deliver();

    r1.update();
    r2.update();
    EXPECT_EQ(*r1.getData(), 99);
    EXPECT_EQ(*r2.getData(), 99);
}

TEST(SenderReceiver, LatestWins)
{
    pf::dc::Mempool<int>      pool(4);
    pf::dc::SenderPort<int>   sender;
    pf::dc::ReceiverPort<int> receiver;

    sender.connectMempool(pool);
    receiver.connect(sender);

    // Send two values before receiver updates
    int* s1 = sender.reserve();
    *s1 = 1;
    sender.deliver();

    int* s2 = sender.reserve();
    *s2 = 2;
    sender.deliver();

    // Receiver should see the latest value
    receiver.update();
    EXPECT_EQ(*receiver.getData(), 2);
}

TEST(SenderReceiver, ThreadedDelivery)
{
    pf::dc::Mempool<int>      pool(8);
    pf::dc::SenderPort<int>   sender;
    pf::dc::ReceiverPort<int> receiver;

    sender.connectMempool(pool);
    receiver.connect(sender);

    constexpr int N = 10;
    std::thread producer([&]()
    {
        for (int i = 0; i < N; ++i)
        {
            int* slot = sender.reserve();
            if (slot)
            {
                *slot = i;
                sender.deliver();
            }
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    });

    int lastSeen = -1;
    for (int iter = 0; iter < N * 20; ++iter)
    {
        receiver.update();
        if (receiver.hasNewData())
        {
            int val = *receiver.getData();
            EXPECT_GE(val, lastSeen); // values should be monotonically increasing
            lastSeen = val;
        }
        receiver.cleanup();
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    producer.join();
    EXPECT_GT(lastSeen, 0); // should have received at least something
}
