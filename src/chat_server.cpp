#include "chat_server.h"
#include "utils.h"
#include <sys/socket.h>
#include <iostream>
#include <vector>

void ChatServer::add_client(int fd, const std::string &name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    clients_[fd] = name;
}

void ChatServer::remove_client(int fd)
{
    std::lock_guard<std::mutex> lock(mutex_);
    clients_.erase(fd);
}

void ChatServer::broadcast(int sender_fd, const std::string &message)
{
    std::vector<int> targets;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        for (const auto &[client_fd, _] : clients_)
        {
            if (client_fd != sender_fd)
            {
                targets.push_back(client_fd);
            }
        }
    }
    for (const auto &client_fd : targets)
    {
        if (!send_text(client_fd, message))
        {
            std::cerr << "Failed to send message to client." << std::endl;
        }
    }
}

bool ChatServer::is_name_taken(const std::string &name) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &[_, existing] : clients_)
    {
        if (existing == name)
        {
            return true;
        }
    }
    return false;
}

bool ChatServer::rename_client(int fd, const std::string &new_name, std::string &old_name)
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &[other_fd, existing] : clients_)
    {
        if (other_fd != fd && existing == new_name)
        {
            return false;
        }
    }
    auto it = clients_.find(fd);
    if (it == clients_.end())
    {
        return false;
    }
    old_name = it->second;
    it->second = new_name;
    return true;
}

bool ChatServer::find_client_fd(const std::string &name, int &out_fd) const
{
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &[fd, existing] : clients_)
    {
        if (existing == name)
        {
            out_fd = fd;
            return true;
        }
    }
    return false;
}

std::string ChatServer::list_clients() const
{
    std::string result = "Online users:";
    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto &[_, name] : clients_)
    {
        result += " " + name;
    }
    result += "\n";
    return result;
}
