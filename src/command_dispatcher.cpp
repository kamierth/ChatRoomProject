#include "command_dispatcher.h"

#include <sstream>
#include <string>

#include <sys/socket.h>
#include <iostream>
#include "chat_server.h"
#include "utils.h"

CommandDispatcher::CommandDispatcher(ChatServer &server)
    : server_(server)
{
}
/*
命令列表：
1./quit
2./list
3./rename
4./msg
5./help
*/
bool CommandDispatcher::dispatch(int client_fd, const std::string &line, std::string &client_name)
{
    std::istringstream ss(line);
    std::string cmd;
    ss >> cmd;

    if(cmd=="/help")
    {
        std::string help_text = "Available commands:\n";
        help_text += "/quit - Disconnect from the server\n";
        help_text += "/list - List all connected users\n";
        help_text += "/rename <new_name> - Change your username\n";
        help_text += "/msg <user> <message> - Send a private message to a user\n";
        help_text += "/help - Show this help message\n";
        if (!send_text(client_fd, help_text))
        {
            std::cerr << "Failed to send help message." << std::endl;
            return false;
        }
        return true;
    }
    if (cmd == "/quit" || cmd == "/exit")
    {
        return false;
    }

    if (cmd == "/list")
    {
        std::string list = server_.list_clients();
        if (!send_text(client_fd, list))
        {
            std::cerr << "Failed to send client list." << std::endl;
            return false;
        }
        return true;
    }

    if (cmd == "/rename")
    {
        std::string new_name;
        ss >> new_name;
        if (new_name.empty())
        {
            std::string msg = "Invalid username.\n";
            if (!send_text(client_fd, msg))
            {
                std::cerr << "Failed to send invalid username message." << std::endl;
                return false;
            }
            return true;
        }
        if (server_.is_name_taken(new_name))
        {
            std::string msg = "Username already taken.\n";
            if (!send_text(client_fd, msg))
            {
                std::cerr << "Failed to send username taken message." << std::endl;
                return false;
            }
            return true;
        }
        std::string old_name;
        if (!server_.rename_client(client_fd, new_name, old_name))
        {
            std::string msg = "Username change failed.\n";
            if (!send_text(client_fd, msg))
            {
                std::cerr << "Failed to send username change failed message." << std::endl;
                return false;
            }
            return true;
        }
        client_name = new_name;
        std::string msg = "Username changed to " + new_name + ".\n";
        if (!send_text(client_fd, msg))
        {
            std::cerr << "Failed to send username changed message." << std::endl;
            return false;
        }
        msg = "User " + old_name + " is now known as " + new_name + ".\n";
        server_.broadcast(client_fd, msg);
        return true;
    }

    if (cmd == "/msg")
    {
        std::string target_name;
        ss >> target_name;
        std::string message;
        std::getline(ss, message);
        clear_message(message);
        size_t first = message.find_first_not_of(' ');
        if (first == std::string::npos)
        {
            message.clear();
        }
        else
        {
            message.erase(0, first);
        }

        if (target_name.empty() || message.empty())
        {
            std::string msg = "Usage: /msg <user> <message>\n";
            if (!send_text(client_fd, msg))
            {
                std::cerr << "Failed to send /msg usage message." << std::endl;
                return false;
            }
            return true;
        }

        int target_fd = -1;
        if (server_.find_client_fd(target_name, target_fd))
        {
            std::string payload = client_name + ": " + message + "\n";
            if (!send_text(target_fd, payload))
            {
                std::cerr << "Failed to send private message." << std::endl;
                return true;
            }
            std::string receipt = "Message sent to " + target_name + ".\n";
            if (!send_text(client_fd, receipt))
            {
                std::cerr << "Failed to send message receipt." << std::endl;
                return false;
            }
        }
        else
        {
            std::string msg = "User not found: " + target_name + "\n";
            if (!send_text(client_fd, msg))
            {
                std::cerr << "Failed to send user not found message." << std::endl;
                return false;
            }
        }
        return true;
    }

    std::string msg = "Unknown command: " + cmd + "\n";
    if (!send_text(client_fd, msg))
    {
        std::cerr << "Failed to send unknown command message." << std::endl;
        return false;
    }
    return true;
}
