#include "server/chat_service.h"
#include "test_helpers.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    bool contains_fd(const std::vector<int> &fds, int expected)
    {
        return std::find(fds.begin(), fds.end(), expected) != fds.end();
    }

    chat::Delivery send(chat::ChatService &service, int fd, std::string message)
    {
        return service.handle_message(fd, message);
    }

    void join_two_users(chat::ChatService &service)
    {
        send(service, 10, "alice");
        send(service, 20, "bob");
    }

    void test_join_and_duplicate_name()
    {
        chat::ChatService service;

        auto alice_join = send(service, 10, "alice");
        EXPECT_EQ(alice_join.sender_fd, 10);
        EXPECT_FALSE(alice_join.close_sender_fd);
        EXPECT_EQ(alice_join.content,
                  std::string("[system]: alice enters the ChatRoom.\n"));
        EXPECT_TRUE(contains_fd(alice_join.target_fds, 10));

        auto duplicate = send(service, 20, "alice");
        EXPECT_EQ(duplicate.target_fds.size(), std::size_t{1});
        EXPECT_TRUE(contains_fd(duplicate.target_fds, 20));
        EXPECT_EQ(duplicate.content,
                  std::string("[system]: This name has been taken. Please input again: \n"));
    }

    void test_empty_message_does_nothing()
    {
        chat::ChatService service;
        auto delivery = send(service, 10, "");

        EXPECT_EQ(delivery.sender_fd, 10);
        EXPECT_TRUE(delivery.target_fds.empty());
        EXPECT_TRUE(delivery.content.empty());
        EXPECT_FALSE(delivery.close_sender_fd);
    }

    void test_broadcast_targets_all_users()
    {
        chat::ChatService service;
        join_two_users(service);

        auto delivery = send(service, 10, "hello everyone");

        EXPECT_EQ(delivery.content, std::string("[alice]: hello everyone\n"));
        EXPECT_EQ(delivery.target_fds.size(), std::size_t{2});
        EXPECT_TRUE(contains_fd(delivery.target_fds, 10));
        EXPECT_TRUE(contains_fd(delivery.target_fds, 20));
        EXPECT_FALSE(delivery.close_sender_fd);
    }

    void test_private_message()
    {
        chat::ChatService service;
        join_two_users(service);

        auto delivery = send(service, 10, "/msg bob hello there");

        EXPECT_EQ(delivery.target_fds.size(), std::size_t{1});
        EXPECT_TRUE(contains_fd(delivery.target_fds, 20));
        EXPECT_EQ(delivery.content, std::string("[alice]: hello there\n"));
    }

    void test_private_message_errors()
    {
        chat::ChatService service;
        send(service, 10, "alice");

        auto missing_content = send(service, 10, "/msg nobody");
        EXPECT_TRUE(contains_fd(missing_content.target_fds, 10));
        EXPECT_EQ(missing_content.content,
                  std::string("[system]: Invalid Format. Usage: /msg <user> <message>\n"));

        auto missing_user = send(service, 10, "/msg nobody hello");
        EXPECT_TRUE(contains_fd(missing_user.target_fds, 10));
        EXPECT_EQ(missing_user.content,
                  std::string("[system]: nobody is not in the ChatRoom.\n"));
    }

    void test_rename_and_duplicate_rename()
    {
        chat::ChatService service;
        join_two_users(service);

        auto renamed = send(service, 10, "/rename ally");
        EXPECT_EQ(renamed.content,
                  std::string("[system]: alice has renamed to ally\n"));
        EXPECT_TRUE(contains_fd(renamed.target_fds, 10));
        EXPECT_TRUE(contains_fd(renamed.target_fds, 20));

        auto duplicate = send(service, 10, "/rename bob");
        EXPECT_EQ(duplicate.target_fds.size(), std::size_t{1});
        EXPECT_TRUE(contains_fd(duplicate.target_fds, 10));
        EXPECT_EQ(duplicate.content,
                  std::string("[system]: This name has been taken.\n"));

        auto missing_name = send(service, 10, "/rename");
        EXPECT_EQ(missing_name.content,
                  std::string("[system]: Invalid Format. Usage: /rename <new_name>\n"));
    }

    void test_list_help_and_invalid_command()
    {
        chat::ChatService service;
        join_two_users(service);

        auto list = send(service, 10, "/list");
        EXPECT_EQ(list.target_fds.size(), std::size_t{1});
        EXPECT_TRUE(contains_fd(list.target_fds, 10));
        EXPECT_TRUE(list.content.find("Online users:") != std::string::npos);
        EXPECT_TRUE(list.content.find("alice") != std::string::npos);
        EXPECT_TRUE(list.content.find("bob") != std::string::npos);

        auto help = send(service, 10, "/help");
        EXPECT_TRUE(contains_fd(help.target_fds, 10));
        EXPECT_TRUE(help.content.find("Available commands:") != std::string::npos);
        EXPECT_TRUE(help.content.find("/quit") != std::string::npos);

        auto invalid = send(service, 10, "/unknown");
        EXPECT_TRUE(contains_fd(invalid.target_fds, 10));
        EXPECT_TRUE(invalid.content.find("Invalid command") != std::string::npos);
    }

    void test_quit_removes_user_and_notifies_remaining_users()
    {
        chat::ChatService service;
        join_two_users(service);

        auto quit = send(service, 10, "/quit");

        EXPECT_TRUE(quit.close_sender_fd);
        EXPECT_EQ(quit.target_fds.size(), std::size_t{1});
        EXPECT_TRUE(contains_fd(quit.target_fds, 20));
        EXPECT_EQ(quit.content,
                  std::string("[system]: alice has left the ChatRoom.\n"));

        auto list = send(service, 20, "/list");
        EXPECT_TRUE(list.content.find("bob") != std::string::npos);
        EXPECT_TRUE(list.content.find("alice") == std::string::npos);
    }
}

void run_chat_service_tests()
{
    test::run("ChatService joins and rejects duplicate names", test_join_and_duplicate_name);
    test::run("ChatService ignores empty input", test_empty_message_does_nothing);
    test::run("ChatService broadcasts to all users", test_broadcast_targets_all_users);
    test::run("ChatService sends private messages", test_private_message);
    test::run("ChatService reports private message errors", test_private_message_errors);
    test::run("ChatService renames users", test_rename_and_duplicate_rename);
    test::run("ChatService handles list, help, and invalid commands", test_list_help_and_invalid_command);
    test::run("ChatService removes users on quit", test_quit_removes_user_and_notifies_remaining_users);
}
