#include "client/chat_client.h"
#include "common/logger.h"

#include <cstdlib>
int main()
{
    chat::ChatClient client("127.0.0.1", 8080);
    if (!client.start())
    {
        chat::log::error("failed to start the client");
        return EXIT_FAILURE;
    }
    client.run();
    return EXIT_SUCCESS;
}
