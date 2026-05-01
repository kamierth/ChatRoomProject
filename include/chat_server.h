#pragma once

#include <mutex>
#include <string>
#include <unordered_map>

class ChatServer
{
public:
    void add_client(int fd, const std::string &name);
    void remove_client(int fd);
    void broadcast(int sender_fd, const std::string &message);
    bool is_name_taken(const std::string &name) const;
    bool rename_client(int fd, const std::string &new_name, std::string &old_name);
    bool find_client_fd(const std::string &name, int &out_fd) const;
    std::string list_clients() const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<int, std::string> clients_;
};
