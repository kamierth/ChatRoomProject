#include "server/epoll_server.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <cerrno>
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
        ::close(listen_fd_);
        listen_fd_ = -1;
    }
    if (epoll_fd_ >= 0)
    {
        ::close(epoll_fd_);
        epoll_fd_ = -1;
    }
}
bool chat::EpollServer::start()
{
    if (!create_listener())
    {
        std::perror("create_listener");
        return false;
    }
    if (!create_epoll())
    {
        std::perror("create_epoll");
        return false;
    }
    if (!add_fd(listen_fd_, EPOLLIN))
    {
        std::perror("add_listen_fd");
        return false;
    }
    return true;
}
void chat::EpollServer::run()
{
    if (epoll_fd_ < 0 || events_.empty())
    {
        std::cerr << "EpollServer has not been started correctly\n";
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
            std::perror("epoll_wait failed");
            break;
        }
        for (int i = 0; i < count; ++i)
        {
            handle_event(events_[i]);
        }
    }
    running_ = false;
}
bool chat::EpollServer::create_listener()
{
    if (listen_fd_ != -1)
    {
        std::cerr << "监听 Socket 已经创建\n";
        return false;
    }
    listen_fd_ = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listen_fd_ == -1)
    {
        std::perror("socket");
        return false;
    }
    // 允许服务器退出后快速重新绑定同一个端口
    int reuse_addr = 1;
    if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) == -1)
    {
        std::perror("setsockopt");
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
        std::perror("bind");
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }
    if (::listen(listen_fd_, SOMAXCONN) == -1)
    {
        std::perror("listen");
        ::close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }
    std::cout << "服务器已启动，等待连接 (端口" << port_ << ")...\n";
    return true;
}

bool chat::EpollServer::create_epoll()
{
    if (epoll_fd_ != -1)
    {
        std::cerr << "epoll 实例已经创建\n";
        return false;
    }
    epoll_fd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd_ == -1)
    {
        std::perror("epoll_create1");
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
        std::perror("epoll_ctl ADD");
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
        std::perror("epoll_ctl MOD");
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
        std::perror("epoll_ctl DEL");
        return false;
    }
    return true;
}

void chat::EpollServer::handle_event(const epoll_event &event)
{
    int fd = event.data.fd;
    uint32_t flags = event.events;
    if (fd == listen_fd_)
    {
        if (flags & (EPOLLERR | EPOLLHUP))
        {
            std::cerr << "监听 Socket 发生错误或被挂断\n";
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
            std::perror("accept4");
            break;
        }
        auto client = std::make_unique<ClientConnection>(client_fd);
        auto [it, inserted] = clients_.try_emplace(client_fd, std::move(client));
        if (!inserted)
        {
            std::cerr << "客户端 fd 重复, fd=" << client_fd << '\n';
            ::close(client_fd);
            continue;
        }
        if (!add_fd(client_fd, EPOLLIN | EPOLLRDHUP))
        {
            // add_fd() 内部已经 perror
            clients_.erase(it);
            ::close(client_fd);
            continue;
        }
        std::cout << "客户端连接成功,fd=" << client_fd << '\n';

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
            return modify_fd(fd, EPOLLIN | EPOLLOUT | EPOLLRDHUP);
        }
        return false;
    }
    return modify_fd(fd, EPOLLIN | EPOLLRDHUP);
    /*
    EPOLLIN ：  让内核监听是否有接收到消息，有就反应给我。
    EPOLLOUT ： 监听发送缓冲区是否为空，由于缓冲区基本上都是空的，
                所以一般只在缓冲区满了的且消息没发完才设置监听，
                处理完之后再取消监听，否则会浪费CPU资源。
    EPOLLRDHUP：让内核监听是否有断开连接的请求
    */
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
        std::cerr << "从 epoll 删除客户端失败，fd=" << fd << '\n';
        success = false;
    }
    if (::close(fd) == -1)
    {
        std::perror("close client");
        success = false;
    }
    // 即使前面的操作失败，也必须删除节点，保证清理能够继续
    clients_.erase(it);
    Delivery delivery = chat_service_.handle_leave(fd);
    delivery.close_sender_fd = false;
    handle_delivery(delivery);
    std::cout << "客户端断开连接，fd=" << fd << '\n';
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
                close_client(target_fd);
            }
            if (!handle_client_send(target_fd))
            {
                close_client(target_fd);
            }
        }
    }
    return success;
}