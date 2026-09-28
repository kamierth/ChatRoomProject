#include "server/epoll_server.h"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace
{
    using namespace std::chrono_literals;

    class FileDescriptor
    {
    public:
        explicit FileDescriptor(int fd = -1) noexcept : fd_(fd) {}

        ~FileDescriptor()
        {
            if (fd_ >= 0)
            {
                ::close(fd_);
            }
        }

        FileDescriptor(const FileDescriptor &) = delete;
        FileDescriptor &operator=(const FileDescriptor &) = delete;

        FileDescriptor(FileDescriptor &&other) noexcept : fd_(other.fd_)
        {
            other.fd_ = -1;
        }

        FileDescriptor &operator=(FileDescriptor &&other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }
            if (fd_ >= 0)
            {
                ::close(fd_);
            }
            fd_ = other.fd_;
            other.fd_ = -1;
            return *this;
        }

        int get() const noexcept
        {
            return fd_;
        }

    private:
        int fd_;
    };

    class ServerProcess
    {
    public:
        explicit ServerProcess(std::uint16_t port)
        {
            pid_ = ::fork();
            if (pid_ < 0)
            {
                throw std::runtime_error(std::string("fork failed: ") + std::strerror(errno));
            }

            if (pid_ == 0)
            {
                {
                    chat::EpollServer server(port);
                    if (!server.start())
                    {
                        ::_exit(2);
                    }
                    server.run();
                }
                ::_exit(0);
            }
        }

        ~ServerProcess()
        {
            if (pid_ > 0)
            {
                ::kill(pid_, SIGTERM);
                while (::waitpid(pid_, nullptr, 0) < 0 && errno == EINTR)
                {
                }
            }
        }

        ServerProcess(const ServerProcess &) = delete;
        ServerProcess &operator=(const ServerProcess &) = delete;

        int stop()
        {
            if (pid_ <= 0)
            {
                throw std::runtime_error("server process is not running");
            }
            if (::kill(pid_, SIGTERM) == -1)
            {
                throw std::runtime_error(std::string("kill failed: ") + std::strerror(errno));
            }

            int status = 0;
            while (::waitpid(pid_, &status, 0) < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                throw std::runtime_error(std::string("waitpid failed: ") + std::strerror(errno));
            }
            pid_ = -1;
            return status;
        }

    private:
        pid_t pid_{-1};
    };

    void require(bool condition, std::string_view message)
    {
        if (!condition)
        {
            throw std::runtime_error(std::string(message));
        }
    }

    std::uint16_t find_available_port()
    {
        FileDescriptor fd(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
        require(fd.get() >= 0, "failed to create port-selection socket");

        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = 0;

        require(::bind(fd.get(), reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0,
                "failed to bind port-selection socket");

        socklen_t address_size = sizeof(address);
        require(::getsockname(fd.get(), reinterpret_cast<sockaddr *>(&address), &address_size) == 0,
                "failed to read selected port");

        return ntohs(address.sin_port);
    }

    FileDescriptor connect_with_retry(std::uint16_t port)
    {
        sockaddr_in address{};
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(port);

        for (int attempt = 0; attempt < 100; ++attempt)
        {
            FileDescriptor fd(::socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
            require(fd.get() >= 0, "failed to create client socket");

            if (::connect(fd.get(), reinterpret_cast<sockaddr *>(&address), sizeof(address)) == 0)
            {
                return fd;
            }

            if (errno != ECONNREFUSED && errno != EINTR)
            {
                throw std::runtime_error(std::string("connect failed: ") + std::strerror(errno));
            }
            std::this_thread::sleep_for(20ms);
        }

        throw std::runtime_error("server did not become ready in time");
    }

    void send_all(int fd, std::string_view content)
    {
        std::size_t sent = 0;
        while (sent < content.size())
        {
            const ssize_t size = ::send(
                fd,
                content.data() + sent,
                content.size() - sent,
                MSG_NOSIGNAL);

            if (size > 0)
            {
                sent += static_cast<std::size_t>(size);
                continue;
            }
            if (size < 0 && errno == EINTR)
            {
                continue;
            }
            throw std::runtime_error(std::string("send failed: ") + std::strerror(errno));
        }
    }

    std::string receive_until(
        int fd,
        std::string_view expected,
        std::chrono::milliseconds timeout = 2000ms)
    {
        std::string received;
        const auto deadline = std::chrono::steady_clock::now() + timeout;

        while (received.find(expected) == std::string::npos)
        {
            const auto now = std::chrono::steady_clock::now();
            if (now >= deadline)
            {
                throw std::runtime_error(
                    "timeout waiting for '" + std::string(expected) + "', received: " + received);
            }

            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now);
            pollfd event{};
            event.fd = fd;
            event.events = POLLIN;

            int poll_result;
            do
            {
                poll_result = ::poll(&event, 1, static_cast<int>(remaining.count()));
            } while (poll_result < 0 && errno == EINTR);

            require(poll_result > 0, "socket receive timed out");

            char buffer[4096];
            const ssize_t size = ::recv(fd, buffer, sizeof(buffer), 0);
            if (size > 0)
            {
                received.append(buffer, static_cast<std::size_t>(size));
                continue;
            }
            if (size == 0)
            {
                throw std::runtime_error("server closed the connection unexpectedly");
            }
            if (errno == EINTR)
            {
                continue;
            }
            throw std::runtime_error(std::string("recv failed: ") + std::strerror(errno));
        }

        return received;
    }

    void expect_connection_closed(int fd)
    {
        pollfd event{};
        event.fd = fd;
        event.events = POLLIN;

        const int result = ::poll(&event, 1, 2000);
        require(result > 0, "server did not close client connection");

        char byte;
        const ssize_t size = ::recv(fd, &byte, 1, 0);
        require(size == 0, "expected an orderly connection close");
    }

    void run_integration_test()
    {
        const std::uint16_t port = find_available_port();
        ServerProcess server(port);

        auto alice = connect_with_retry(port);
        require(receive_until(alice.get(), "Please input your name:").find("Please input your name:") != std::string::npos,
                "alice did not receive welcome message");
        send_all(alice.get(), "alice\n");
        require(receive_until(alice.get(), "alice enters the ChatRoom").find("alice enters the ChatRoom") != std::string::npos,
                "alice did not join");

        auto bob = connect_with_retry(port);
        receive_until(bob.get(), "Please input your name:");
        send_all(bob.get(), "bob\n");
        receive_until(bob.get(), "bob enters the ChatRoom");
        receive_until(alice.get(), "bob enters the ChatRoom");

        send_all(alice.get(), "hello integration\n");
        require(receive_until(alice.get(), "[alice]: hello integration").find("[alice]: hello integration") != std::string::npos,
                "alice did not receive its broadcast");
        require(receive_until(bob.get(), "[alice]: hello integration").find("[alice]: hello integration") != std::string::npos,
                "bob did not receive alice's broadcast");

        send_all(alice.get(), "/msg bob private hello\n");
        require(receive_until(bob.get(), "[alice]: private hello").find("[alice]: private hello") != std::string::npos,
                "bob did not receive the private message");

        send_all(bob.get(), "/rename robert\n");
        receive_until(alice.get(), "bob has renamed to robert");
        receive_until(bob.get(), "bob has renamed to robert");

        send_all(alice.get(), "/list\n");
        const std::string member_list = receive_until(alice.get(), "\n");
        require(member_list.find("Online users:") != std::string::npos,
                "list response is missing its heading");
        require(member_list.find("alice") != std::string::npos,
                "list response is missing alice");
        require(member_list.find("robert") != std::string::npos,
                "list response is missing robert");

        send_all(alice.get(), "/quit\n");
        expect_connection_closed(alice.get());
        receive_until(bob.get(), "alice has left the ChatRoom");

        const int server_status = server.stop();
        expect_connection_closed(bob.get());
        require(WIFEXITED(server_status), "server was terminated by a signal");
        require(WEXITSTATUS(server_status) == 0, "server returned a non-zero exit code");
    }
}

int main()
{
    try
    {
        run_integration_test();
        std::cout << "[PASS] real socket chat flow\n";
        return 0;
    }
    catch (const std::exception &exception)
    {
        std::cerr << "[FAIL] real socket chat flow: " << exception.what() << '\n';
        return 1;
    }
}
