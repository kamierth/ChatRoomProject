#include <iostream>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <sstream>
std::unordered_map<int, std::string> client_names;
std::mutex client_mutex;
void broadcast(const int &sender_fd, const std::string &message)
{
    // std::cout << message << '\n';
    std::vector<int> targets;
    {
        std::lock_guard<std::mutex> lock(client_mutex);
        for (const auto &[client_fd, _] : client_names)
        {
            if (client_fd != sender_fd)
            {
                targets.push_back(client_fd);
            }
        }
    }
    for (const auto &client_fd : targets)
    {
        send(client_fd, message.c_str(), message.size(), 0);
    }
}
void clear_message(std::string &msg)
{
    while (!msg.empty() &&
           (msg.back() == '\n' || msg.back() == '\r'))
    {
        msg.pop_back();
    }
}
bool dispatch_command(int client_fd, const std::string &line, std::string &client_name)
{
    std::istringstream ss(line);
    std::string cmd;
    ss >> cmd;
    if (cmd == "/quit" || cmd == "/exit")
    {
        return false;
    }
    else if (cmd == "/list")
    {
        // Handle list command
    }
    else if (cmd == "/rename")
    {
        std::string new_name;
        ss >> new_name;
        if (new_name.empty())
        {
            std::string msg = "Invalid username.\n";
            send(client_fd, msg.c_str(), msg.size(), 0);
        }
        std::string old_name = client_name;
        {
            std::lock_guard<std::mutex> lock(client_mutex);
            client_names[client_fd] = new_name;
            client_name = new_name;
        }
        std::string msg = "Username changed to " + new_name + ".\n";
        send(client_fd, msg.c_str(), msg.size(), 0);
        msg = "User " + old_name + " is now known as " + new_name + ".\n";
        broadcast(client_fd, msg);
    }
    else if (cmd == "/msg")
    {
        std::string target_name;
        ss >> target_name;
        std::string message;
        std::getline(ss, message);
        clear_message(message);
        int target_fd = -1;
        {
            std::lock_guard<std::mutex> lock(client_mutex);
            for (const auto &[fd, name] : client_names)
            {
                if (name == target_name)
                {
                    target_fd = fd;
                    break;
                }
            }
        }
        if (target_fd != -1)
        {
            send(target_fd, (client_name + ": " + message + "\n").c_str(), (client_name + ": " + message + "\n").size(), 0);
        }
        else
        {
            send(client_fd, ("User not found: " + target_name + "\n").c_str(), ("User not found: " + target_name + "\n").size(), 0);
        }
    }
    else
    {
        send(client_fd, ("Unknown command: " + cmd + "\n").c_str(), ("Unknown command: " + cmd + "\n").size(), 0);
    }
    return true;
}
void handle_chat_message(int client_fd, const std::string &line, const std::string &client_name)
{
    broadcast(client_fd, client_name + ": " + line + "\n");
}
bool handle_line(int client_fd, const std::string &line, std::string &client_name)
{
    // Handle each line of message
    if (!line.empty() && line[0] == '/')
    {
        return dispatch_command(client_fd, line, client_name);
    }
    else
    {
        handle_chat_message(client_fd, line, client_name);
        return true;
    }
}
void handle_client(const int &c)
{
    char buffer[1024];
    std::string msg = "Welcome to the chatroom!\n";
    std::string client_name_str;
    std::string recv_buffer;
    while (true)
    {
        ssize_t n = recv(c, buffer, sizeof(buffer), 0);
        if (n <= 0)
        {
            break;
        }
        recv_buffer.append(buffer, n);
        size_t pos;
        bool flag = 1;
        while ((pos = recv_buffer.find('\n')) != std::string::npos)
        {
            std::string line = recv_buffer.substr(0, pos);
            recv_buffer.erase(0, pos + 1);
            // 处理每一行消息
            clear_message(line);
            if (client_name_str.empty())
            {
                client_name_str = line;
                {
                    std::lock_guard<std::mutex> lock(client_mutex);
                    client_names[c] = client_name_str;
                }
                std::cout << "New client connected: " << client_name_str << std::endl;
                broadcast(c, client_name_str + " has joined the chat.\n");
            }
            else
            {
                if (!handle_line(c, line, client_name_str))
                {
                    flag = 0;
                    break;
                }
            }
        }
        if (!flag)
        {
            break;
        }
    }
    {
        std::lock_guard<std::mutex> lock(client_mutex);
        client_names.erase(c);
    }
    broadcast(c, client_name_str + " has left the chat.\n");
    close(c);
}
int main()
{
    // 1) 创建 TCP socket
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
        std::perror("socket");
        return 1;
    }
    // 允许快速重启服务，避免端口处于 TIME_WAIT 时 bind 失败
    int opt = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        std::perror("setsockopt(SO_REUSEADDR)");
        close(s);
        return 1;
    }
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(1234);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0)
    {
        std::perror("bind");
        close(s);
        return 1;
    }

    if (listen(s, 5) < 0)
    {
        std::perror("listen");
        close(s);
        return 1;
    }

    std::cout << "Server listening on 0.0.0.0:1234" << std::endl;

    while (true)
    {
        int c = accept(s, nullptr, nullptr);
        if (c < 0)
        {
            std::perror("accept");
            continue; // 接受失败不该退出服务器，而是等下一个
        }
        // 只通过线程启动
        std::thread(handle_client, c).detach();
    }
    close(s);
    return 0;
}
