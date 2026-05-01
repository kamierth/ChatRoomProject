#include "client_session.h"

#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <string>

#include "chat_server.h"
#include "command_dispatcher.h"
#include "utils.h"

ClientSession::ClientSession(int fd, ChatServer &server, CommandDispatcher &dispatcher)
    : fd_(fd), server_(server), dispatcher_(dispatcher)
{
}

void ClientSession::run()
{
    char buffer[1024];
    std::string welcome = "Welcome to the chatroom!\n";
    if (!send_text(fd_, welcome))
    {
        std::cerr << "Failed to send welcome message." << std::endl;
        return;
    }

    bool keep_running = true;
    while (keep_running)
    {
        ssize_t n = recv(fd_, buffer, sizeof(buffer), 0);
        if (n <= 0)
        {
            break;
        }
        recv_buffer_.append(buffer, n);
        size_t pos;
        while ((pos = recv_buffer_.find('\n')) != std::string::npos)
        {
            std::string line = recv_buffer_.substr(0, pos);
            recv_buffer_.erase(0, pos + 1);
            clear_message(line);

            if (name_.empty())
            {
                if (line.empty())
                {
                    std::string msg = "Invalid username.\n";
                    if (!send_text(fd_, msg))
                    {
                        std::cerr << "Failed to send invalid username message." << std::endl;
                    }
                    continue;
                }
                if (server_.is_name_taken(line))
                {
                    std::string msg = "Username already taken.\n";
                    if (!send_text(fd_, msg))
                    {
                        std::cerr << "Failed to send username taken message." << std::endl;
                    }
                    continue;
                }
                name_ = line;
                server_.add_client(fd_, name_);
                std::cout << "New client connected: " << name_ << std::endl;
                server_.broadcast(fd_, name_ + " has joined the chat.\n");
                continue;
            }

            if (!handle_line(line))
            {
                keep_running = false;
                break;
            }
        }
    }

    if (!name_.empty())
    {
        server_.remove_client(fd_);
        server_.broadcast(fd_, name_ + " has left the chat.\n");
    }
    close(fd_);
}

bool ClientSession::handle_line(const std::string &line)
{
    if (!line.empty() && line[0] == '/')
    {
        return dispatcher_.dispatch(fd_, line, name_);
    }
    handle_chat_message(line);
    return true;
}

void ClientSession::handle_chat_message(const std::string &line)
{
    server_.broadcast(fd_, name_ + ": " + line + "\n");
}
