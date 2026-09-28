#include "client/chat_client.h"
#include "common/logger.h"

#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <utility>

chat::ChatClient::ChatClient(std::string server_ip, std::uint16_t port)
    : server_ip_(std::move(server_ip)), port_(port) {}

chat::ChatClient::~ChatClient()
{
    clean_up();
}

bool chat::ChatClient::start()
{
    if (!connect_server())
    {
        clean_up();
        return false;
    }
    initialize_poll_fds();
    if (!set_nonblocking())
    {
        clean_up();
        return false;
    }
    running_ = true;
    stdin_open_ = true;
    return true;
}

void chat::ChatClient::run()
{
    if (!running_ || fds_.size() != 2)
    {
        return;
    }
    while (running_)
    {
        int result = ::poll(fds_.data(), static_cast<nfds_t>(fds_.size()), -1);
        if (result < 0)
        {
            if (errno == EINTR)
            {
                continue;
            }
            chat::log::system_error("client poll", errno);
            break;
        }
        if (fds_[0].revents & (POLLIN | POLLHUP))
        {
            if (!handle_stdin())
            {
                break;
            }
        }
        if (fds_[1].revents & POLLIN)
        {
            if (!handle_socket_recv())
            {
                break;
            }
        }
        if (fds_[1].revents & POLLOUT)
        {
            if (!handle_socket_send())
            {
                break;
            }
        }
        if (fds_[1].revents & (POLLERR | POLLHUP | POLLRDHUP | POLLNVAL))
        {
            break;
        }
    }
    clean_up();
}

void chat::ChatClient::stop() noexcept
{
    clean_up();
}

bool chat::ChatClient::connect_server()
{
    if (socket_fd_ != -1)
    {
        return false;
    }
    socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ == -1)
    {
        chat::log::system_error("create client socket", errno);
        return false;
    }
    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    if (::inet_pton(AF_INET, server_ip_.c_str(), &server_addr.sin_addr) != 1)
    {
        chat::log::error("invalid IPv4 address: " + server_ip_);
        return false;
    }
    if (::connect(socket_fd_, reinterpret_cast<sockaddr *>(&server_addr), sizeof(server_addr)) == -1)
    {
        chat::log::system_error("connect", errno);
        return false;
    }
    chat::log::info(
        "connected to " + server_ip_ + ':' + std::to_string(port_));
    return true;
}

bool chat::ChatClient::set_nonblocking()
{
    int flags = ::fcntl(socket_fd_, F_GETFL, 0);
    if (flags == -1)
    {
        chat::log::system_error("fcntl F_GETFL", errno);
        return false;
    }
    if (::fcntl(socket_fd_, F_SETFL, flags | O_NONBLOCK) == -1)
    {
        chat::log::system_error("fcntl F_SETFL", errno);
        return false;
    }
    return true;
}
void chat::ChatClient::initialize_poll_fds()
{
    fds_.clear();

    pollfd stdin_event{};
    stdin_event.fd = STDIN_FILENO;
    stdin_event.events = POLLIN;
    fds_.push_back(stdin_event);

    pollfd socket_event{};
    socket_event.fd = socket_fd_;
    socket_event.events = POLLIN | POLLRDHUP;
    fds_.push_back(socket_event);
}

bool chat::ChatClient::handle_stdin()
{
    char buffer[4096];
    while (true)
    {
        ssize_t size = ::read(STDIN_FILENO, buffer, sizeof(buffer));
        if (size > 0)
        {
            send_buffer_.append(buffer, static_cast<size_t>(size));
            return handle_socket_send();
        }
        if (size == 0)
        {
            // 标准输入已经关闭，例如 Ctrl+D 或管道输入结束
            stdin_open_ = false;
            // poll 会忽略 fd 为 -1 的项目
            fds_[0].fd = -1;
            return true;
        }
        if (errno == EINTR)
        {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return true;
        }
        chat::log::system_error("read standard input", errno);
        return false;
    }
}

void chat::ChatClient::handle_stdout()
{
    while (true)
    {
        auto pos = recv_buffer_.find('\n');
        if (pos == std::string::npos)
        {
            break;
        }
        std::string buffer = recv_buffer_.substr(0, pos);
        recv_buffer_.erase(0, pos + 1);
        std::cout << buffer << '\n';
    }
}
bool chat::ChatClient::handle_socket_recv()
{
    char buffer[4096];
    while (true)
    {
        ssize_t size = ::recv(socket_fd_, buffer, sizeof(buffer), 0);
        if (size > 0)
        {
            recv_buffer_.append(buffer, static_cast<size_t>(size));
            handle_stdout();
            continue;
        }
        if (size == 0)
        {
            chat::log::info("server closed the connection");
            return false;
        }
        if (errno == EINTR)
        {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK)
        {
            return true;
        }
        chat::log::system_error("receive from server", errno);
        return false;
    }
    return false;
}

bool chat::ChatClient::handle_socket_send()
{
    while (send_buffer_.size())
    {
        ssize_t size = ::send(socket_fd_, send_buffer_.data(), send_buffer_.size(), MSG_NOSIGNAL);
        if (size > 0)
        {
            send_buffer_.erase(0, static_cast<size_t>(size));
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
            update_socket_events(POLLIN | POLLOUT | POLLRDHUP);
            return true;
        }
        chat::log::system_error("send to server", errno);
        return false;
    }
    update_socket_events(POLLIN | POLLRDHUP);
    return true;
}

void chat::ChatClient::update_socket_events(int events)
{
    fds_[1].events = static_cast<short>(events);
}

void chat::ChatClient::clean_up() noexcept
{
    running_ = false;
    if (socket_fd_ >= 0)
    {
        int fd = socket_fd_;
        socket_fd_ = -1;
        if (::close(fd) == -1)
        {
            chat::log::system_error("close client socket", errno);
        }
    }
    fds_.clear();
    recv_buffer_.clear();
    send_buffer_.clear();
}
