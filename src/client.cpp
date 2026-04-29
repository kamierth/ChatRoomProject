#include <iostream>
#include <string>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>

// 接收消息的线程函数
void receive_handler(int sockfd) {
    char buffer[1024];
    while (true) {
        ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            std::cout << "\n[系统] 与服务器断开连接。" << std::endl;
            exit(0); // 简单粗暴：服务器断了，客户端直接退出
        }
        buffer[n] = '\0';
        // 打印收到的消息（可能会打断你正在输入的文字）
        std::cout << "\r" << buffer<<"> " << std::flush;
    }
}

int main() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    
    sockaddr_in serv_addr{};
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(1234);
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connect failed");
        return 1;
    }

    std::cout << "--- 已进入聊天室 ---" << std::endl;
    std::cout<<"input name: ";
    std::string name;
    std::getline(std::cin, name);
    while (name.empty()) {
        std::cout << "Name cannot be empty. Please enter again: ";
        std::getline(std::cin, name);
    }
    std::string packet=name+"\n";
    send(sockfd, packet.c_str(), packet.size(), 0);
    // 开启接收线程
    std::thread t(receive_handler, sockfd);
    t.detach();

    // 主线程负责发送
    std::string input;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, input)) break;
        std::string packet=input+"\n";
        if (!input.empty()) {
            send(sockfd, packet.c_str(), packet.size(), 0);
        }
    }

    close(sockfd);
    return 0;
}