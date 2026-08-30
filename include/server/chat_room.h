#pragma once

#include <string>
#include <unordered_map>
#include <optional>
#include <vector>

namespace chat
{
    enum class JoinResult
    {
        Success,
        EmptyName,
        NameTaken,
        AlreadyJoin
    };

    enum class RenameResult
    {
        Success,
        Emptyname,
        NameTaken,
        ClientNotFound
    };

    class ChatRoom
    {
    public:
        JoinResult join(int fd, std::string &name);
        std::optional<std::string> leave(int fd);
        RenameResult rename(int fd, std::string &new_name);
        std::optional<int> private_message(std::string &name);

        std::optional<int> find_fd(std::string &name);
        std::optional<std::string> find_name(int fd);

        std::vector<int> get_fds();
        std::vector<std::string> get_names();

    private:
        std::unordered_map<int, std::string> name_by_fd_;
        std::unordered_map<std::string, int> fd_by_name_;
    };
}