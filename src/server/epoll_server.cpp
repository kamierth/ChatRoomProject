#include "server/epoll_server.h"
#include "common/logger.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cerrno>
#include <csignal>
#include <cstring>

chat::EpollServer::EpollServer(uint16_t port, size_t max_events)
    : port_(port), max_events_(max_events), events_(max_events), chat_service_() {
      };
chat::EpollServer::~EpollServer()
{
    clean_up();
}
void chat::EpollServer::clean_up()
{
    running_ = false;
    while (!clients_.empty())
    {
        int fd = clients_.begin()->first;
        close_client(fd);
    }
    if (listen_fd_ >= 0)
    {
        remove_fd(listen_fd_);
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    if (signal_fd_ >= 0)
    {
        remove_fd(signal_fd_);
        ::close(signal_fd_);
        signal_fd_ = -1;
    }
    if (epoll_fd_ >= 0)
    {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
}
bool chat::EpollServer::start()
{
    stopping_ = false;
    if (!create_listener())
    {
        return false;
    }
    if (!create_epoll())
    {
        return false;
    }
    if (!add_fd(listen_fd_, EPOLLIN))
    {
        return false;
    }
    if (!create_signal_fd())
    {
        return false;
    }
    return true;
}
void chat::EpollServer::run()
{
    if (epoll_fd_ < 0 || events_.empty())
    {
        chat::log::error("EpollServer has not been started correctly");
        return;
    }
    running_ = true;
    while (running_)
    {
        int count = epoll_wait(epoll_fd_, events_.data(), static_cast<int>(events_.size()), -1);
        if (count < 0)
        {
            // 执行时被信号中断
            if (errno == EINTR)
            {
                continue;
            }
            chat::log::system_error("epoll_wait", errno);
            break;
        }
        const auto event_count = static_cast<std::size_t>(count);
        for (std::size_t i = 0; i < event_count; ++i)
        {
            handle_event(events_[i]);
            if (!running_)
            {
                break;
            }
        }
    }
    running_ = false;
}
bool chat::EpollServer::create_listener()
{
    if (listen_fd_ != -1)
    {
        chat::log::warning("listener socket already exists");
        return false;
    }
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listen_fd_ == -1)
    {
        chat::log::system_error("create listener socket", errno);
        return false;
    }
    // 允许服务器退出后快速重新绑定同一个端口
    int reuse_addr = 1;
    if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) == -1)
    {
        chat::log::system_error("setsockopt SO_REUSEADDR", errno);
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);
    if (::bind(listen_fd_, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        chat::log::system_error("bind listener socket", errno);
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }
    if (::listen(listen_fd_, SOMAXCONN) == -1)
    {
        chat::log::system_error("listen", errno);
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }
    chat::log::info("server started port=" + std::to_string(port_));
    return true;
}

bool chat::EpollServer::create_epoll()
{
    if (epoll_fd_ != -1)
    {
        chat::log::warning("epoll instance already exists");
        return false;
    }
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1)
    {
        chat::log::system_error("epoll_create1", errno);
        return false;
    }
    return true;
}

bool chat::EpollServer::create_signal_fd()
{
    if (signal_fd_ != -1)
    {
        chat::log::warning("signalfd already exists");
        return false;
    }

    sigset_t mask;
    if (::sigemptyset(&mask) == -1)
    {
        chat::log::system_error("sigemptyset", errno);
        return false;
    }
    if (::sigaddset(&mask, SIGINT) == -1 ||
        ::sigaddset(&mask, SIGTERM) == -1)
    {
        chat::log::system_error("sigaddset", errno);
        return false;
    }
    if (::sigprocmask(SIG_BLOCK, &mask, nullptr) == -1)
    {
        chat::log::system_error("sigprocmask", errno);
        return false;
    }

    signal_fd_ = ::signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
    if (signal_fd_ == -1)
    {
        chat::log::system_error("signalfd", errno);
        return false;
    }
    if (!add_fd(signal_fd_, EPOLLIN))
    {
        ::close(signal_fd_);
        signal_fd_ = -1;
        return false;
    }
    return true;
}

bool chat::EpollServer::add_fd(int fd, std::uint32_t events)
{
    epoll_event event{};
    event.events = events;
    event.data.fd = fd;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_ADD, fd, &event) == -1)
    {
        chat::log::system_error("epoll_ctl ADD", errno);
        return false;
    }
    return true;
}
bool chat::EpollServer::modify_fd(int fd, std::uint32_t events)
{
    epoll_event event{};
    event.events = events;
    event.data.fd = fd;
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_MOD, fd, &event) == -1)
    {
        chat::log::system_error("epoll_ctl MOD", errno);
        return false;
    }
    return true;
}
bool chat::EpollServer::remove_fd(int fd)
{
    if (fd < 0 || epoll_fd_ < 0)
    {
        return true;
    }
    if (::epoll_ctl(epoll_fd_, EPOLL_CTL_DEL, fd, nullptr) == -1)
    {
        // ENOENT 表示该 fd 已经不在 epoll 中
        if (errno == ENOENT)
        {
            return true;
        }
        chat::log::system_error("epoll_ctl DEL", errno);
        return false;
    }
    return true;
}

void chat::EpollServer::handle_event(const epoll_event &event)
{
    int fd = event.data.fd;
    uint32_t flags = event.events;
    if (fd == signal_fd_)
    {
        handle_signal_event();
        return;
    }
    if (fd == listen_fd_)
    {
        if (flags & (EPOLLERR | EPOLLHUP))
        {
            chat::log::error("listener socket reported an error or hangup");
            remove_fd(listen_fd_);
            ::close(listen_fd_);
            listen_fd_ = -1;
            // 没有监听 socket 后，服务器无法接受新连接
            running_ = false;
            return;
        }
        if (flags & EPOLLIN)
        {
            accept_clients();
        }
    }
    else
    {
        if (flags & EPOLLERR)
        {
            close_client(fd);
            return;
        }
        if (flags & EPOLLIN)
        {
            if (!handle_client_recv(fd))
            {
                close_client(fd);
            }
            if (clients_.find(fd) == clients_.end())
            {
                return;
            }
        }
        // 客户端 socket 可以继续发送
        if (flags & EPOLLOUT)
        {
            if (!handle_client_send(fd))
            {
                close_client(fd);
            }
            // 发送失败时可能已经关闭客户端
            if (clients_.find(fd) == clients_.end())
            {
                return;
            }
        }
        if (flags & (EPOLLHUP | EPOLLRDHUP))
        {
            close_client(fd);
        }
    }
}

void chat::EpollServer::handle_signal_event()
{
    while (true)
    {
        signalfd_siginfo signal_info{};
        const ssize_t size = ::read(signal_fd_, &signal_info, sizeof(signal_info));

        if (size == static_cast<ssize_t>(sizeof(signal_info)))
        {
            if (signal_info.ssi_signo == SIGINT ||
                signal_info.ssi_signo == SIGTERM)
            {
                begin_shutdown();
                return;
            }
            continue;
        }
        if (size < 0 && errno == EINTR)
        {
            continue;
        }
        if (size < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
        {
            return;
        }

        if (size < 0)
        {
            chat::log::system_error("read signalfd", errno);
        }
        else
        {
            chat::log::error("read signalfd returned an incomplete record");
        }
        begin_shutdown();
        return;
    }
}

void chat::EpollServer::begin_shutdown()
{
    if (stopping_)
    {
        return;
    }

    stopping_ = true;
    chat::log::info("server shutdown requested");

    if (listen_fd_ >= 0)
    {
        remove_fd(listen_fd_);
        ::close(listen_fd_);
        listen_fd_ = -1;
    }

    running_ = false;
}

void chat::EpollServer::accept_clients()
{
    if (listen_fd_ == -1)
    {
        return;
    }
    while (true)
    {
        int client_fd = ::accept4(listen_fd_, nullptr, nullptr, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (client_fd == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // 当前待处理连接已经全部取完
                break;
            }
            if (errno == EINTR)
            {
                // 被信号中断，重新 accept
                continue;
            }
            chat::log::system_error("accept4", errno);
            break;
        }
        auto client = std::make_unique<ClientConnection>(client_fd);
        auto [it, inserted] = clients_.try_emplace(client_fd, std::move(client));
        if (!inserted)
        {
            chat::log::warning(
                "duplicate client descriptor fd=" + std::to_string(client_fd));
            ::close(client_fd);
            continue;
        }
        if (!add_fd(client_fd, EPOLLIN | EPOLLRDHUP))
        {
            // add_fd() 内部已经记录错误
            clients_.erase(it);
            ::close(client_fd);
            continue;
        }
        chat::log::info("client connected fd=" + std::to_string(client_fd));

        std::string welcome_message = "Please input your name: \n";
        if (!clients_[client_fd]->append_send_buffer(welcome_message))
        {
            close_client(client_fd);
        }
        if (!handle_client_send(client_fd))
        {
            close_client(client_fd);
        }
    }
}
bool chat::EpollServer::handle_client_recv(int fd)
{
    char buffer[4096];
    while (true)
    {
        ssize_t size = ::recv(fd, buffer, 4096, 0);
        if (size > 0)
        {
            auto it = clients_.find(fd);
            if (it == clients_.end())
            {
                return true;
            }
            if (!it->second->append_recv_buffer(buffer, static_cast<size_t>(size)))
            {
                chat::log::warning(
                    "receive buffer limit exceeded fd=" + std::to_string(fd) +
                    " limit=" + std::to_string(ClientConnection::max_recv_buffer_size));
                return false;
            }
            while (true)
            {
                auto current = clients_.find(fd);
                if (current == clients_.end())
                {
                    return true;
                }
                auto message = current->second->pop_recv_buffer();
                if (!message)
                {
                    break;
                }
                Delivery delivery = chat_service_.handle_message(fd, message.value());
                if (!handle_delivery(delivery))
                {
                    return false;
                }
                if (clients_.find(fd) == clients_.end())
                {
                    return true;
                }
            }
            auto current = clients_.find(fd);
            if (current != clients_.end() && current->second->receive_paused())
            {
                // 已进入用户态缓冲区的完整消息已处理完；剩余数据留在内核
                // 接收缓冲区，恢复 EPOLLIN 后会再次触发读取事件。
                return true;
            }
            continue;
        }
        if (size == 0)
        {
            return false;
        }
        if (errno == EINTR)
        {
            continue;
        }
        /*
            errno == EAGAIN || errno == EWOULDBLOCK
            解释：socket 是非阻塞的，并且接收缓冲区现在没有更多数据可读，不是非法错误
        */
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return true;
        }
        return false;
    }
}
bool chat::EpollServer::handle_client_send(int fd)
{
    auto it = clients_.find(fd);
    if (it == clients_.end())
    {
        return false;
    }
    ClientConnection &client = *it->second;
    while (client.has_pending_send())
    {
        ssize_t size = ::send(fd, client.send_data(), client.send_size(), MSG_NOSIGNAL);
        // MSG_NOSIGNAL是为了避免客服端关闭时服务器使用send导致系统产生SIGPIPE而引发的服务端进程停止
        if (size > 0)
        {
            client.consume_sent(static_cast<size_t>(size));
            continue;
        }
        if (size == 0)
        {
            return false;
        }
        if (errno == EINTR)
        {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return update_client_events(fd);
        }
        chat::log::system_error("send to client fd=" + std::to_string(fd), errno);
        return false;
    }
    return update_client_events(fd);
    /*
    EPOLLIN ：  让内核监听是否有接收到消息，有就反应给我。
    EPOLLOUT ： 监听发送缓冲区是否为空，由于缓冲区基本上都是空的，
                所以一般只在缓冲区满了的且消息没发完才设置监听，
                处理完之后再取消监听，否则会浪费CPU资源。
    EPOLLRDHUP：让内核监听是否有断开连接的请求
    */
}

bool chat::EpollServer::update_client_events(int fd)
{
    auto it = clients_.find(fd);
    if (it == clients_.end())
    {
        return false;
    }

    ClientConnection &client = *it->second;
    const bool state_changed = client.refresh_backpressure_state();
    if (state_changed)
    {
        const std::string state = client.receive_paused() ? "paused" : "resumed";
        const std::string message =
            "client receive " + state + " by backpressure fd=" + std::to_string(fd) +
            " queued_bytes=" + std::to_string(client.send_size());
        if (client.receive_paused())
        {
            chat::log::warning(message);
        }
        else
        {
            chat::log::info(message);
        }
    }

    std::uint32_t events = EPOLLRDHUP;
    if (!client.receive_paused())
    {
        events |= EPOLLIN;
    }
    if (client.has_pending_send())
    {
        events |= EPOLLOUT;
    }
    return modify_fd(fd, events);
}

bool chat::EpollServer::close_client(int fd)
{
    auto it = clients_.find(fd);
    if (it == clients_.end())
    {
        return true;
    }
    bool success = true;
    if (!remove_fd(fd))
    {
        chat::log::warning(
            "failed to remove client from epoll fd=" + std::to_string(fd));
        success = false;
    }
    if (::close(fd) == -1)
    {
        chat::log::system_error("close client socket", errno);
        success = false;
    }
    // 即使前面的操作失败，也必须删除节点，保证清理能够继续
    clients_.erase(it);
    Delivery delivery = chat_service_.handle_leave(fd);
    delivery.close_sender_fd = false;
    handle_delivery(delivery);
    chat::log::info("client disconnected fd=" + std::to_string(fd));
    return success;
}

bool chat::EpollServer::handle_delivery(chat::Delivery &delivery)
{
    bool success = true;
    if (delivery.close_sender_fd)
    {
        if (!close_client(delivery.sender_fd))
        {
            success = false;
        }
    }
    if (delivery.content != "")
    {
        for (int target_fd : delivery.target_fds)
        {
            auto it = clients_.find(target_fd);
            if (it == clients_.end())
            {
                continue;
            }
            if (!it->second->append_send_buffer(delivery.content))
            {
                chat::log::warning(
                    "disconnecting slow client fd=" + std::to_string(target_fd) +
                    " queued_bytes=" + std::to_string(it->second->send_size()) +
                    " incoming_bytes=" + std::to_string(delivery.content.size()) +
                    " limit=" + std::to_string(ClientConnection::max_send_buffer_size));
                close_client(target_fd);
                continue;
            }
            if (!handle_client_send(target_fd))
            {
                close_client(target_fd);
            }
        }
    }
    return success;
}
