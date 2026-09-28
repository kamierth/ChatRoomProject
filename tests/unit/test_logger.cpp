#include "common/logger.h"
#include "test_helpers.h"

namespace
{
    void test_log_level_can_be_changed()
    {
        const auto original_level = chat::log::level();

        chat::log::set_level(chat::log::Level::Debug);
        EXPECT_EQ(chat::log::level(), chat::log::Level::Debug);

        chat::log::set_level(chat::log::Level::Warning);
        EXPECT_EQ(chat::log::level(), chat::log::Level::Warning);

        chat::log::set_level(original_level);
    }
}

void run_logger_tests()
{
    test::run("Logger changes its minimum level", test_log_level_can_be_changed);
}
