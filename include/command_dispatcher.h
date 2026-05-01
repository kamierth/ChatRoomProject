#pragma once

#include <string>

class ChatServer;

class CommandDispatcher
{
public:
    explicit CommandDispatcher(ChatServer &server);
    bool dispatch(int client_fd, const std::string &line, std::string &client_name);

private:
    ChatServer &server_;
};
