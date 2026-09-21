#include "client/chat_client.h"

#include <iostream>
#include <cstdlib>
int main()
{
    chat::ChatClient client("127.0.0.1", 8080);
    if (!client.start())
    {
        std::cout << "Failed to start the client!\n";
        return EXIT_FAILURE;
    }
    client.run();
    return EXIT_SUCCESS;
}