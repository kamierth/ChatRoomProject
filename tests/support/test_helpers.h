#pragma once

#include <exception>
#include <iostream>
#include <string_view>
#include <utility>

namespace test
{
    inline int assertion_count = 0;
    inline int failure_count = 0;
    inline int case_count = 0;

    inline void expect(
        bool condition,
        std::string_view expression,
        std::string_view file,
        int line)
    {
        ++assertion_count;
        if (condition)
        {
            return;
        }

        ++failure_count;
        std::cerr << file << ':' << line
                  << ": check failed: " << expression << '\n';
    }

    template <typename Actual, typename Expected>
    void expect_equal(
        const Actual &actual,
        const Expected &expected,
        std::string_view expression,
        std::string_view file,
        int line)
    {
        expect(actual == expected, expression, file, line);
    }

    template <typename Function>
    void run(std::string_view name, Function &&function)
    {
        ++case_count;
        const int failures_before = failure_count;

        try
        {
            std::forward<Function>(function)();
        }
        catch (const std::exception &exception)
        {
            ++failure_count;
            std::cerr << "[EXCEPTION] " << name << ": "
                      << exception.what() << '\n';
        }
        catch (...)
        {
            ++failure_count;
            std::cerr << "[EXCEPTION] " << name
                      << ": unknown exception\n";
        }

        std::cout << (failure_count == failures_before ? "[PASS] " : "[FAIL] ")
                  << name << '\n';
    }
}

#define EXPECT_TRUE(expression)                         \
    ::test::expect(                                     \
        static_cast<bool>(expression),                  \
        #expression,                                    \
        __FILE__,                                       \
        __LINE__)

#define EXPECT_FALSE(expression)                        \
    ::test::expect(                                     \
        !static_cast<bool>(expression),                 \
        "!(" #expression ")",                         \
        __FILE__,                                       \
        __LINE__)

#define EXPECT_EQ(actual, expected)                     \
    ::test::expect_equal(                               \
        (actual),                                       \
        (expected),                                     \
        #actual " == " #expected,                      \
        __FILE__,                                       \
        __LINE__)
