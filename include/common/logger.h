#pragma once

#include <string_view>

namespace chat::log
{
    enum class Level
    {
        Debug,
        Info,
        Warning,
        Error
    };

    void set_level(Level level) noexcept;
    Level level() noexcept;

    void write(Level level, std::string_view message) noexcept;
    void debug(std::string_view message) noexcept;
    void info(std::string_view message) noexcept;
    void warning(std::string_view message) noexcept;
    void error(std::string_view message) noexcept;

    void system_error(std::string_view operation, int error_number) noexcept;
}
