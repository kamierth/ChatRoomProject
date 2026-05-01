#pragma once

#include <string>

class ChatServer;
class CommandDispatcher;

class ClientSession
{
public:
    ClientSession(int fd, ChatServer &server, CommandDispatcher &dispatcher);
    void run();

private:
    bool handle_line(const std::string &line);
    void handle_chat_message(const std::string &line);
    void clean_up(const int fd);

    int fd_;
    ChatServer &server_;
    CommandDispatcher &dispatcher_;
    std::string name_;
    std::string recv_buffer_;
};
