#pragma once

#include <iosfwd>
#include <string>
#include <vector>

#include "server/chat_room.h"

namespace chat
{
    struct Delivery
    {
        int sender_fd;
        std::vector<int> target_fds;
        std::string content;
        bool close_sender_fd;
    };

    class ChatService
    {
    public:
        ChatService() : chat_room_() {}
        Delivery handle_message(int fd, std::string &message);

        Delivery handle_join(int fd, std::string &name);
        Delivery handle_leave(int fd);
        Delivery handle_rename(int fd, std::stringstream &ss);
        Delivery handle_private_message(int fd, std::stringstream &ss);
        Delivery handle_list_member(int fd);
        Delivery handle_help(int fd);
        Delivery handle_invalid_command(int fd);

    private:
        ChatRoom chat_room_;
    };
}