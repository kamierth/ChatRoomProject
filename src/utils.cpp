#include "utils.h"
#include <cerrno>
#include <sys/socket.h>
void clear_message(std::string &msg)
{
    while (!msg.empty() && (msg.back() == '\n' || msg.back() == '\r'))
    {
        msg.pop_back();
    }
}

bool send_all(int fd, const char *data, size_t len)
{
    size_t sent = 0;
    while (sent < len)
    {
        ssize_t n = send(fd, data + sent, len - sent, 0);
        if (n > 0)
        {
            sent += static_cast<size_t>(n);
            continue;
        }
        if (n < 0 && errno == EINTR)
        {
            continue;
        }
        return false;
    }
    return true;
}

bool send_text(int fd, const std::string &text)
{
    return send_all(fd, text.c_str(), text.size());
}