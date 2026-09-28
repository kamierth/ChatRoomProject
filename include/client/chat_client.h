#pragma once

#include <string>
#include <vector>
#include <poll.h>
#include <cstdint>
namespace chat
{
    class ChatClient
    {
    public:
        ChatClient(std::string server_ip, std::uint16_t port);
        ~ChatClient();

        ChatClient(const ChatClient &) = delete;
        ChatClient &operator=(const ChatClient &) = delete;
        bool start();
        void run();
        void stop() noexcept;

    private:
        bool connect_server();
        bool set_nonblocking();
        void initialize_poll_fds();

        bool handle_stdin();
        void handle_stdout();
        bool handle_socket_recv();
        bool handle_socket_send();

        void update_socket_events(int events);
        void clean_up() noexcept;

        std::string server_ip_;
        std::uint16_t port_;

        std::string recv_buffer_;
        std::string send_buffer_;

        int socket_fd_{-1};
        std::vector<pollfd> fds_;

        bool stdin_open_{false};
        bool running_{false};
    };
}
