#include "test_helpers.h"

#include <iostream>

void run_chat_room_tests();
void run_chat_service_tests();
void run_client_connection_tests();
void run_logger_tests();

int main()
{
    run_chat_room_tests();
    run_chat_service_tests();
    run_client_connection_tests();
    run_logger_tests();

    std::cout << "\nTest cases: " << test::case_count
              << ", assertions: " << test::assertion_count
              << ", failures: " << test::failure_count << '\n';

    return test::failure_count == 0 ? 0 : 1;
}
