#include "server/chat_room.h"
#include "test_helpers.h"

#include <algorithm>
#include <string>
#include <vector>

namespace
{
    template <typename T>
    bool contains(const std::vector<T> &values, const T &expected)
    {
        return std::find(values.begin(), values.end(), expected) != values.end();
    }

    void test_join_and_lookup()
    {
        chat::ChatRoom room;
        std::string name = "alice";

        EXPECT_EQ(room.join(10, name), chat::JoinResult::Success);

        auto stored_name = room.find_name(10);
        auto stored_fd = room.find_fd(name);

        EXPECT_TRUE(stored_name.has_value());
        EXPECT_TRUE(stored_fd.has_value());
        if (stored_name)
        {
            EXPECT_EQ(*stored_name, std::string("alice"));
        }
        if (stored_fd)
        {
            EXPECT_EQ(*stored_fd, 10);
        }
    }

    void test_join_rejections()
    {
        chat::ChatRoom room;
        std::string empty_name;
        std::string alice = "alice";
        std::string duplicate_alice = "alice";
        std::string second_name = "bob";

        EXPECT_EQ(room.join(10, empty_name), chat::JoinResult::EmptyName);
        EXPECT_EQ(room.join(10, alice), chat::JoinResult::Success);
        EXPECT_EQ(room.join(20, duplicate_alice), chat::JoinResult::NameTaken);
        EXPECT_EQ(room.join(10, second_name), chat::JoinResult::AlreadyJoin);
        EXPECT_FALSE(room.find_name(20).has_value());
    }

    void test_rename_success_updates_both_indexes()
    {
        chat::ChatRoom room;
        std::string old_name = "alice";
        std::string new_name = "ally";

        room.join(10, old_name);

        EXPECT_EQ(room.rename(10, new_name), chat::RenameResult::Success);
        EXPECT_FALSE(room.find_fd(old_name).has_value());

        auto stored_name = room.find_name(10);
        auto stored_fd = room.find_fd(new_name);
        EXPECT_TRUE(stored_name.has_value());
        EXPECT_TRUE(stored_fd.has_value());
        if (stored_name)
        {
            EXPECT_EQ(*stored_name, std::string("ally"));
        }
        if (stored_fd)
        {
            EXPECT_EQ(*stored_fd, 10);
        }
    }

    void test_rename_rejections()
    {
        chat::ChatRoom room;
        std::string alice = "alice";
        std::string bob = "bob";
        std::string empty_name;
        std::string duplicate_name = "bob";
        std::string missing_client_name = "charlie";

        room.join(10, alice);
        room.join(20, bob);

        EXPECT_EQ(room.rename(10, empty_name), chat::RenameResult::Emptyname);
        EXPECT_EQ(room.rename(10, duplicate_name), chat::RenameResult::NameTaken);
        EXPECT_EQ(room.rename(30, missing_client_name), chat::RenameResult::ClientNotFound);
    }

    void test_leave_removes_user()
    {
        chat::ChatRoom room;
        std::string alice = "alice";
        room.join(10, alice);

        auto removed_name = room.leave(10);

        EXPECT_TRUE(removed_name.has_value());
        if (removed_name)
        {
            EXPECT_EQ(*removed_name, std::string("alice"));
        }
        EXPECT_FALSE(room.find_name(10).has_value());
        EXPECT_FALSE(room.find_fd(alice).has_value());
        EXPECT_FALSE(room.leave(10).has_value());
    }

    void test_member_snapshots()
    {
        chat::ChatRoom room;
        std::string alice = "alice";
        std::string bob = "bob";
        room.join(10, alice);
        room.join(20, bob);

        const auto fds = room.get_fds();
        const auto names = room.get_names();

        EXPECT_EQ(fds.size(), std::size_t{2});
        EXPECT_EQ(names.size(), std::size_t{2});
        EXPECT_TRUE(contains(fds, 10));
        EXPECT_TRUE(contains(fds, 20));
        EXPECT_TRUE(contains(names, std::string("alice")));
        EXPECT_TRUE(contains(names, std::string("bob")));
    }
}

void run_chat_room_tests()
{
    test::run("ChatRoom joins and looks up users", test_join_and_lookup);
    test::run("ChatRoom rejects invalid joins", test_join_rejections);
    test::run("ChatRoom rename updates both indexes", test_rename_success_updates_both_indexes);
    test::run("ChatRoom rejects invalid renames", test_rename_rejections);
    test::run("ChatRoom leave removes a user", test_leave_removes_user);
    test::run("ChatRoom returns member snapshots", test_member_snapshots);
}
