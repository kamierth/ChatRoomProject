#pragma once

#include <string>

void clear_message(std::string &msg);
bool send_all(int fd, const char *data, size_t len);
bool send_text(int fd, const std::string &text);