#include "server/client_connection.h"
#include "test_helpers.h"

#include <string>
#include <string_view>

namespace
{
    void test_fd_and_empty_state()
    {
        chat::ClientConnection connection(42);

        EXPECT_EQ(connection.fd(), 42);
        EXPECT_FALSE(connection.has_pending_send());
        EXPECT_EQ(connection.send_size(), std::size_t{0});
        EXPECT_FALSE(connection.pop_recv_buffer().has_value());
    }

    void test_partial_and_multiple_received_messages()
    {
        chat::ClientConnection connection(10);
        const std::string first = "hel";
        const std::string second = "lo\nworld\npartial";

        EXPECT_TRUE(connection.append_recv_buffer(first.data(), first.size()));
        EXPECT_FALSE(connection.pop_recv_buffer().has_value());
        EXPECT_TRUE(connection.append_recv_buffer(second.data(), second.size()));

        auto hello = connection.pop_recv_buffer();
        auto world = connection.pop_recv_buffer();
        auto incomplete = connection.pop_recv_buffer();

        EXPECT_TRUE(hello.has_value());
        EXPECT_TRUE(world.has_value());
        EXPECT_FALSE(incomplete.has_value());
        if (hello)
        {
            EXPECT_EQ(*hello, std::string("hello"));
        }
        if (world)
        {
            EXPECT_EQ(*world, std::string("world"));
        }
    }

    void test_crlf_is_normalized()
    {
        chat::ClientConnection connection(10);
        const std::string input = "hello\r\n";
        connection.append_recv_buffer(input.data(), input.size());

        auto message = connection.pop_recv_buffer();
        EXPECT_TRUE(message.has_value());
        if (message)
        {
            EXPECT_EQ(*message, std::string("hello"));
        }
    }

    void test_receive_buffer_limit()
    {
        chat::ClientConnection connection(10);
        const std::string full(chat::ClientConnection::max_recv_buffer_size, 'x');
        const char extra = 'y';

        EXPECT_FALSE(connection.append_recv_buffer(nullptr, 1));
        EXPECT_TRUE(connection.append_recv_buffer(full.data(), full.size()));
        EXPECT_FALSE(connection.append_recv_buffer(&extra, 1));
    }

    void test_send_buffer_and_partial_consumption()
    {
        chat::ClientConnection connection(10);

        EXPECT_TRUE(connection.append_send_buffer("hello"));
        EXPECT_TRUE(connection.has_pending_send());
        EXPECT_EQ(connection.send_size(), std::size_t{5});
        EXPECT_EQ(std::string_view(connection.send_data(), connection.send_size()),
                  std::string_view("hello"));

        connection.consume_sent(2);
        EXPECT_EQ(connection.send_size(), std::size_t{3});
        EXPECT_EQ(std::string_view(connection.send_data(), connection.send_size()),
                  std::string_view("llo"));

        connection.consume_sent(100);
        EXPECT_FALSE(connection.has_pending_send());
        EXPECT_EQ(connection.send_size(), std::size_t{0});
    }

    void test_send_buffer_limit()
    {
        chat::ClientConnection connection(10);
        const std::string full(chat::ClientConnection::max_send_buffer_size, 'x');

        EXPECT_TRUE(connection.append_send_buffer(full));
        EXPECT_FALSE(connection.append_send_buffer("y"));
    }

    void test_backpressure_uses_high_and_low_watermarks()
    {
        chat::ClientConnection connection(10);
        const std::string almost_high(
            chat::ClientConnection::send_high_watermark - 1, 'x');

        EXPECT_TRUE(connection.append_send_buffer(almost_high));
        EXPECT_FALSE(connection.refresh_backpressure_state());
        EXPECT_FALSE(connection.receive_paused());

        EXPECT_TRUE(connection.append_send_buffer("x"));
        EXPECT_TRUE(connection.refresh_backpressure_state());
        EXPECT_TRUE(connection.receive_paused());

        connection.consume_sent(
            chat::ClientConnection::send_high_watermark -
            chat::ClientConnection::send_low_watermark - 1);
        EXPECT_FALSE(connection.refresh_backpressure_state());
        EXPECT_TRUE(connection.receive_paused());

        connection.consume_sent(1);
        EXPECT_TRUE(connection.refresh_backpressure_state());
        EXPECT_FALSE(connection.receive_paused());
    }
}

void run_client_connection_tests()
{
    test::run("ClientConnection starts empty", test_fd_and_empty_state);
    test::run("ClientConnection handles stream framing", test_partial_and_multiple_received_messages);
    test::run("ClientConnection normalizes CRLF", test_crlf_is_normalized);
    test::run("ClientConnection enforces receive limit", test_receive_buffer_limit);
    test::run("ClientConnection consumes partial sends", test_send_buffer_and_partial_consumption);
    test::run("ClientConnection enforces send limit", test_send_buffer_limit);
    test::run("ClientConnection applies backpressure watermarks",
              test_backpressure_uses_high_and_low_watermarks);
}
