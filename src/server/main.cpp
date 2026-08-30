#include "server/epoll_server.h"

#include <iostream>
#include <cstdlib>

int main()
{
    const std::uint16_t port = 8080;
    chat::EpollServer server(port);
    if (!server.start())
    {
        std::cerr << "服务器启动失败\n";
        return EXIT_FAILURE;
    }
    server.run();
    return EXIT_SUCCESS;
}