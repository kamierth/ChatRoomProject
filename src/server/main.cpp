#include "server/epoll_server.h"
#include "common/logger.h"

#include <cstdlib>

int main()
{
    const std::uint16_t port = 8080;
    chat::EpollServer server(port);
    if (!server.start())
    {
        chat::log::error("failed to start the server");
        return EXIT_FAILURE;
    }
    server.run();
    return EXIT_SUCCESS;
}
