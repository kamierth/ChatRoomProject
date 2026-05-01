#include <arpa/inet.h>
#include <iostream>
#include <thread>
#include <unistd.h>

#include "chat_server.h"
#include "client_session.h"
#include "command_dispatcher.h"

int main()
{
    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0)
    {
        std::perror("socket");
        return 1;
    }

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

    ChatServer server;
    CommandDispatcher dispatcher(server);

    while (true)
    {
        int c = accept(s, nullptr, nullptr);
        if (c < 0)
        {
            std::perror("accept");
            continue;
        }
        std::thread([c, &server, &dispatcher]()
                    {
            ClientSession session(c, server, dispatcher);
            session.run(); })
            .detach();
    }

    close(s);
    return 0;
}
