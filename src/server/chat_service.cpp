#include "server/chat_service.h"

#include <sstream>

chat::Delivery chat::ChatService::handle_message(int fd, std::string &message)
{
    Delivery delivery = {fd, {}, "", false};
    if (message.empty())
    {
        return delivery;
    }
    auto it = chat_room_.find_name(fd);
    if (!it.has_value())
    {
        return handle_join(fd, message);
    }
    if (message[0] != '/')
    {
        delivery.target_fds = chat_room_.get_fds();
        delivery.content = "[" + it.value() + "]: " + message + "\n";
        return delivery;
    }
    std::stringstream ss(message);
    std::string command;
    ss >> command;
    if (command == "/quit")
    {
        return handle_leave(fd);
    }
    if (command == "/rename")
    {
        return handle_rename(fd, ss);
    }
    if (command == "/msg")
    {
        return handle_private_message(fd, ss);
    }
    if (command == "/list")
    {
        return handle_list_member(fd);
    }
    if (command == "/help")
    {
        return handle_help(fd);
    }
    return handle_invalid_command(fd);
}

chat::Delivery chat::ChatService::handle_join(int fd, std::string &name)
{
    Delivery delivery = {fd, {}, "", false};
    chat::JoinResult join_result = chat_room_.join(fd, name);
    delivery.target_fds.push_back(fd);
    std::string content;
    switch (join_result)
    {
    case chat::JoinResult::Success:
        delivery.target_fds = chat_room_.get_fds();
        content = "[system]: " + name + " enters the ChatRoom.\n";
        break;
    case chat::JoinResult::EmptyName:
        content = "[system]: Name can't be empty. Please input again: \n";
        break;
    case chat::JoinResult::NameTaken:
        content = "[system]: This name has been taken. Please input again: \n";
        break;
    case chat::JoinResult::AlreadyJoin:
        content = "[system]: You are already in the ChatRoom.\n";
        break;
    default:
        break;
    };
    delivery.content = content;
    return delivery;
}

chat::Delivery chat::ChatService::handle_leave(int fd)
{
    Delivery delivery = {fd, {}, "", true};
    auto name = chat_room_.leave(fd);
    if (!name.has_value())
    {
        return delivery;
    }
    delivery.target_fds = chat_room_.get_fds();
    delivery.content = "[system]: " + name.value() + " has left the ChatRoom.\n";
    return delivery;
}

chat::Delivery chat::ChatService::handle_rename(int fd, std::stringstream &ss)
{
    Delivery delivery = {fd, {}, "", false};
    std::string old_name = chat_room_.find_name(fd).value();
    std::string new_name = "";
    ss >> new_name;
    chat::RenameResult rename_result = chat_room_.rename(fd, new_name);
    switch (rename_result)
    {
    case chat::RenameResult::Success:
        delivery.target_fds = chat_room_.get_fds();
        delivery.content = "[system]: " + old_name + " has renamed to " + new_name + "\n";
        break;
    case chat::RenameResult::Emptyname:
        delivery.target_fds.push_back(fd);
        delivery.content = "[system]: Invalid Format. Usage: /rename <new_name>\n";
        break;
    case chat::RenameResult::NameTaken:
        delivery.target_fds.push_back(fd);
        delivery.content = "[system]: This name has been taken.\n";
        break;
    case chat::RenameResult::ClientNotFound:
        delivery.close_sender_fd = true;
        break;
    default:
        break;
    }
    return delivery;
}

chat::Delivery chat::ChatService::handle_private_message(int fd, std::stringstream &ss)
{
    Delivery delivery = {fd, {}, "", false};
    std::string target_name;
    std::string content = "";
    ss >> target_name;
    std::getline(ss, content);

    auto first = content.find_first_not_of(' ');
    if (first == std::string::npos)
    {
        delivery.target_fds.push_back(fd);
        delivery.content = "[system]: Invalid Format. Usage: /msg <user> <message>\n";
        return delivery;
    }
    content.erase(0, first);

    auto it = chat_room_.private_message(target_name);
    if (!it.has_value())
    {
        delivery.target_fds.push_back(fd);
        delivery.content = "[system]: " + target_name + " is not in the ChatRoom.\n";
        return delivery;
    }

    delivery.target_fds.push_back(it.value());
    auto name = chat_room_.find_name(fd);
    if (!name.has_value())
    {
        return {fd, {}, "", true};
    }
    delivery.content = "[" + name.value() + "]: " + content + "\n";
    return delivery;
}

chat::Delivery chat::ChatService::handle_list_member(int fd)
{
    Delivery delivery = {fd, {fd}, "[system]: Online users: ", false};
    std::vector<std::string> names = chat_room_.get_names();
    for (auto name : names)
    {
        auto it = chat_room_.find_fd(name);
        if (it.has_value())
        {
            delivery.content += (name + " ");
        }
    }
    delivery.content += "\n";
    return delivery;
}

chat::Delivery chat::ChatService::handle_help(int fd)
{
    Delivery delivery = {fd, {fd}, "[system]: \n", false};
    std::string help_text = "Available commands:\n";
    help_text += "/quit - Disconnect from the server\n";
    help_text += "/list - List all connected users\n";
    help_text += "/rename <new_name> - Change your username\n";
    help_text += "/msg <user> <message> - Send a private message to a user\n";
    help_text += "/help - Show this help message\n";
    delivery.content += help_text;
    return delivery;
}

chat::Delivery chat::ChatService::handle_invalid_command(int fd)
{
    Delivery dlivery = {fd, {fd}, "[system]: Invalid command. Please input \"/help\" to gain the commands.\n", false};
    return dlivery;
}