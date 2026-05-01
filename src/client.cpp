#include <iostream>
#include <string>
#include <thread>
#include <arpa/inet.h>
#include <unistd.h>
#include<atomic>

#include "utils.h"

std::atomic<bool>running{true};

// 接收消息的线程函数
void receive_handler(int sockfd) {
    char buffer[1024];
    while (running) {
        ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) {
            std::cout << "\n[系统] 与服务器断开连接。" << std::endl;
            running =false;
            break;
        }
        buffer[n] = '\0';
        std::cout << "\r" << buffer<<"> " << std::flush;
    }
}

void request_shutdown(int fd)
{
    running = false;
    shutdown(fd, SHUT_RDWR);
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
    if(!send_text(sockfd, packet)) {
        std::cerr << "Failed to send name." << std::endl;
        running=false;
        close(sockfd);
        return 1;
    }
    // 开启接收线程
    std::thread t(receive_handler, sockfd);

    // 主线程负责发送
    std::string input;
    while (running) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, input)){
            request_shutdown(sockfd);
            break;
        }
        std::string packet=input+"\n";
        if (!input.empty()) {
            if(!send_text(sockfd, packet)) {
                std::cerr << "Failed to send message." << std::endl;
                request_shutdown(sockfd);
                break;
            }
            if(input=="/quit" || input=="/exit") {
                request_shutdown(sockfd);
                break;
            }
        }
    }
    t.join();
    close(sockfd);
    return 0;
}