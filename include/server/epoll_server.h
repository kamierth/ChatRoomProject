#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <sys/epoll.h>

#include "server/chat_service.h"
#include "server/client_connection.h"

namespace chat
{
    class EpollServer
    {
    public:
        explicit EpollServer(uint16_t port, size_t max_events = 1024);
        ~EpollServer();
        EpollServer(const EpollServer &) = delete;
        EpollServer &operator=(const EpollServer &) = delete;
        bool start();
        void run();
        // bool stop();

    private:
        uint16_t port_;
        size_t max_events_;
        int listen_fd_{-1};
        int epoll_fd_{-1};
        int signal_fd_{-1};
        bool running_{false};
        bool stopping_{false};
        std::vector<epoll_event> events_;
        std::unordered_map<int, std::unique_ptr<ClientConnection>> clients_;
        ChatService chat_service_;

        void clean_up();

        bool create_listener();
        bool create_epoll();
        bool create_signal_fd();

        bool add_fd(int fd, std::uint32_t events);
        bool modify_fd(int fd, std::uint32_t events);
        bool remove_fd(int fd);

        void handle_event(const epoll_event &event);
        void handle_signal_event();
        void begin_shutdown();
        void accept_clients();
        bool handle_client_recv(int fd);
        bool handle_client_send(int fd);
        bool update_client_events(int fd);

        bool close_client(int fd);

        bool handle_delivery(chat::Delivery &delivery);
    };
}
