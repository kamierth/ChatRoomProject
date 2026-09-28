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
        static constexpr std::size_t send_high_watermark = 256 * 1024;
        static constexpr std::size_t send_low_watermark = 128 * 1024;

        static_assert(send_low_watermark < send_high_watermark);
        static_assert(send_high_watermark < max_send_buffer_size);

        bool append_recv_buffer(const char *data, size_t size);
        bool append_send_buffer(const std::string &content);

        std::optional<std::string> pop_recv_buffer();

        bool has_pending_send() const noexcept;
        const char *send_data() const noexcept;
        size_t send_size() const noexcept;
        void consume_sent(size_t size);

        bool receive_paused() const noexcept;
        bool refresh_backpressure_state() noexcept;

    private:
        int fd_;
        std::string recv_buffer_;
        std::string send_buffer_;
        bool receive_paused_{false};
    };
}
