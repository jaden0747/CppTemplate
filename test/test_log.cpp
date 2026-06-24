// ---------------------------------------------------------------------------
// test_log.cpp — Unit tests for Log module (log.hpp)
// ---------------------------------------------------------------------------

#include "core/log.hpp"

#include <spdlog/sinks/ostream_sink.h>

#include <gtest/gtest.h>

#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Fixture: captures spdlog output via ostream_sink
// ---------------------------------------------------------------------------

class LogTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_stream = std::make_shared<std::ostringstream>();
        m_sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(*m_stream);
        m_sink->set_pattern("[%l] [%n] %v");
    }

    std::shared_ptr<spdlog::logger> makeLogger(const std::string& name)
    {
        auto logger = std::make_shared<spdlog::logger>(name, m_sink);
        logger->set_level(spdlog::level::trace);
        logger->set_pattern("[%l] [%n] %v");
        return logger;
    }

    std::string output() const { return m_stream->str(); }

    std::shared_ptr<std::ostringstream> m_stream;
    std::shared_ptr<spdlog::sinks::ostream_sink_mt> m_sink;
};

// ---------------------------------------------------------------------------

TEST_F(LogTest, LoggerWritesToSink)
{
    auto logger = makeLogger("writer");
    logger->info("hello {}", 42);
    logger->flush();

    EXPECT_NE(output().find("hello 42"), std::string::npos);
    EXPECT_NE(output().find("[info]"), std::string::npos);
}

TEST_F(LogTest, MultipleLogLevels)
{
    auto logger = makeLogger("levels");
    logger->trace("t_msg");
    logger->debug("d_msg");
    logger->info("i_msg");
    logger->warn("w_msg");
    logger->error("e_msg");
    logger->critical("c_msg");
    logger->flush();

    std::string out = output();
    EXPECT_NE(out.find("[trace]"), std::string::npos);
    EXPECT_NE(out.find("[debug]"), std::string::npos);
    EXPECT_NE(out.find("[info]"), std::string::npos);
    EXPECT_NE(out.find("[warning]"), std::string::npos);
    EXPECT_NE(out.find("[error]"), std::string::npos);
    EXPECT_NE(out.find("[critical]"), std::string::npos);
}

TEST_F(LogTest, LevelFilteringWorks)
{
    auto logger = makeLogger("filtered");
    logger->set_level(spdlog::level::warn);

    logger->info("hidden_msg");
    logger->warn("visible_msg");
    logger->flush();

    std::string out = output();
    EXPECT_EQ(out.find("hidden_msg"), std::string::npos);
    EXPECT_NE(out.find("visible_msg"), std::string::npos);
}

TEST_F(LogTest, LoggerNameInOutput)
{
    auto logger = makeLogger("mymodule");
    logger->info("test message");
    logger->flush();

    std::string out = output();
    EXPECT_NE(out.find("[mymodule]"), std::string::npos);
    EXPECT_NE(out.find("test message"), std::string::npos);
}

TEST_F(LogTest, MultipleSinksReceiveMessages)
{
    auto stream2 = std::make_shared<std::ostringstream>();
    auto sink2 = std::make_shared<spdlog::sinks::ostream_sink_mt>(*stream2);

    auto logger = std::make_shared<spdlog::logger>(
        "multi", spdlog::sinks_init_list{m_sink, sink2});
    logger->set_level(spdlog::level::trace);

    logger->info("broadcast");
    logger->flush();

    EXPECT_NE(output().find("broadcast"), std::string::npos);
    EXPECT_NE(stream2->str().find("broadcast"), std::string::npos);
}

TEST_F(LogTest, DynamicSinkAddition)
{
    auto logger = makeLogger("dynamic");
    logger->info("first_msg");
    logger->flush();

    auto stream2 = std::make_shared<std::ostringstream>();
    auto sink2 = std::make_shared<spdlog::sinks::ostream_sink_mt>(*stream2);
    logger->sinks().push_back(sink2);

    logger->info("second_msg");
    logger->flush();

    EXPECT_NE(output().find("first_msg"), std::string::npos);
    EXPECT_NE(output().find("second_msg"), std::string::npos);
    EXPECT_EQ(stream2->str().find("first_msg"), std::string::npos);
    EXPECT_NE(stream2->str().find("second_msg"), std::string::npos);
}

TEST_F(LogTest, FmtFormattingWorks)
{
    auto logger = makeLogger("fmt");
    logger->info("val={} str={}", 123, "abc");
    logger->flush();

    EXPECT_NE(output().find("val=123 str=abc"), std::string::npos);
}

// ---------------------------------------------------------------------------
// Test the Log singleton wrapper
// ---------------------------------------------------------------------------

TEST(LogSingleton, GetReturnsValidLogger)
{
    auto logger = Log::get("test_singleton_a");
    ASSERT_NE(logger, nullptr);
    EXPECT_EQ(logger->name(), "test_singleton_a");
}

TEST(LogSingleton, GetReturnsSameInstance)
{
    auto a = Log::get("test_singleton_same");
    auto b = Log::get("test_singleton_same");
    EXPECT_EQ(a.get(), b.get());
}

TEST(LogSingleton, DifferentNamesReturnDifferentLoggers)
{
    auto a = Log::get("test_singleton_x");
    auto b = Log::get("test_singleton_y");
    EXPECT_NE(a.get(), b.get());
}
