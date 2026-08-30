#pragma once

#include <cstddef>
#include <string>
#include <optional>
namespace chat
{
    class ClientConnection
    {
    public:
        ClientConnection(int fd) : fd_(fd) {};
        int fd() const noexcept;
        static constexpr std::size_t max_recv_buffer_size = 64 * 1024;
        static constexpr std::size_t max_send_buffer_size = 1024 * 1024;

        bool append_recv_buffer(const char *data, size_t size);
        bool append_send_buffer(const std::string &content);

        std::optional<std::string> pop_recv_buffer();

        bool has_pending_send() const noexcept;
        const char *send_data() const noexcept;
        size_t send_size() const noexcept;
        void consume_sent(size_t size);

    private:
        int fd_;
        std::string recv_buffer_;
        std::string send_buffer_;
    };
}