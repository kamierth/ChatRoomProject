#include "common/logger.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <system_error>

namespace
{
    std::atomic<chat::log::Level> minimum_level{chat::log::Level::Info};
    std::mutex output_mutex;

    std::string_view level_name(chat::log::Level level) noexcept
    {
        switch (level)
        {
        case chat::log::Level::Debug:
            return "DEBUG";
        case chat::log::Level::Info:
            return "INFO";
        case chat::log::Level::Warning:
            return "WARN";
        case chat::log::Level::Error:
            return "ERROR";
        }
        return "UNKNOWN";
    }
}

void chat::log::set_level(Level level) noexcept
{
    minimum_level.store(level, std::memory_order_relaxed);
}

chat::log::Level chat::log::level() noexcept
{
    return minimum_level.load(std::memory_order_relaxed);
}

void chat::log::write(Level message_level, std::string_view message) noexcept
{
    try
    {
        if (message_level < level())
        {
            return;
        }

        const auto now = std::chrono::system_clock::now();
        const auto time = std::chrono::system_clock::to_time_t(now);
        const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      now.time_since_epoch()) %
                                  1000;

        std::tm local_time{};
        ::localtime_r(&time, &local_time);

        std::lock_guard<std::mutex> lock(output_mutex);
        std::clog << '[' << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S")
                  << '.' << std::setfill('0') << std::setw(3) << milliseconds.count()
                  << "] [" << level_name(message_level) << "] "
                  << message << '\n';
        std::clog.flush();
    }
    catch (...)
    {
        // Logging must never interrupt the application or resource cleanup.
    }
}

void chat::log::debug(std::string_view message) noexcept
{
    write(Level::Debug, message);
}

void chat::log::info(std::string_view message) noexcept
{
    write(Level::Info, message);
}

void chat::log::warning(std::string_view message) noexcept
{
    write(Level::Warning, message);
}

void chat::log::error(std::string_view message) noexcept
{
    write(Level::Error, message);
}

void chat::log::system_error(std::string_view operation, int error_number) noexcept
{
    try
    {
        const std::error_code error_code(error_number, std::generic_category());
        std::ostringstream message;
        message << operation << ": " << error_code.message()
                << " (errno=" << error_number << ')';
        write(Level::Error, message.str());
    }
    catch (...)
    {
        write(Level::Error, operation);
    }
}
