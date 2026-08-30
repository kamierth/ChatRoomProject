#include "server/chat_room.h"

chat::JoinResult chat::ChatRoom::join(int fd, std::string &name)
{
    if (name_by_fd_.find(fd) != name_by_fd_.end())
    {
        return chat::JoinResult::AlreadyJoin;
    }
    if (name.empty())
    {
        return chat::JoinResult::EmptyName;
    }
    if (fd_by_name_.find(name) != fd_by_name_.end())
    {
        return chat::JoinResult::NameTaken;
    }
    name_by_fd_[fd] = name;
    fd_by_name_[name] = fd;
    return chat::JoinResult::Success;
}

std::optional<std::string> chat::ChatRoom::leave(int fd)
{
    if (name_by_fd_.find(fd) == name_by_fd_.end())
    {
        return std::nullopt;
    }
    std::string name = name_by_fd_[fd];
    name_by_fd_.erase(fd);
    fd_by_name_.erase(name);
    return name;
}

chat::RenameResult chat::ChatRoom::rename(int fd, std::string &new_name)
{
    if (name_by_fd_.find(fd) == name_by_fd_.end())
    {
        return chat::RenameResult::ClientNotFound;
    }
    std::string old_name = name_by_fd_[fd];
    if (new_name.empty())
    {
        return chat::RenameResult::Emptyname;
    }
    if (fd_by_name_.find(new_name) != fd_by_name_.end())
    {
        return chat::RenameResult::NameTaken;
    }
    name_by_fd_[fd] = new_name;
    fd_by_name_.erase(old_name);
    fd_by_name_[new_name] = fd;
    return chat::RenameResult::Success;
}
std::optional<int> chat::ChatRoom::private_message(std::string &name)
{
    if (fd_by_name_.find(name) == fd_by_name_.end())
        return std::nullopt;
    return fd_by_name_[name];
}

std::optional<int> chat::ChatRoom::find_fd(std::string &name)
{
    if (fd_by_name_.find(name) != fd_by_name_.end())
    {
        return fd_by_name_[name];
    }
    return std::nullopt;
}
std::optional<std::string> chat::ChatRoom::find_name(int fd)
{
    if (name_by_fd_.find(fd) != name_by_fd_.end())
    {
        return name_by_fd_[fd];
    }
    return std::nullopt;
}

std::vector<int> chat::ChatRoom::get_fds()
{
    std::vector<int> fds;
    for (auto &[fd, name] : name_by_fd_)
    {
        fds.push_back(fd);
    }
    return fds;
}
std::vector<std::string> chat::ChatRoom::get_names()
{
    std::vector<std::string> names;
    for (auto &[fd, name] : name_by_fd_)
    {
        names.push_back(name);
    }
    return names;
}