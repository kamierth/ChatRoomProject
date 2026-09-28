#include <server/client_connection.h>

#include <string>

int chat::ClientConnection::fd() const noexcept
{
    return fd_;
}

bool chat::ClientConnection::append_recv_buffer(const char *data, size_t size)
{
    if (data == nullptr)
    {
        return false;
    }
    if (size > max_recv_buffer_size - recv_buffer_.size())
    {
        return false;
    }
    recv_buffer_.append(data, size);
    return true;
}

bool chat::ClientConnection::append_send_buffer(const std::string &content)
{
    if (content.size() > max_send_buffer_size - send_buffer_.size())
    {
        return false;
    }
    send_buffer_ += content;
    return true;
}
std::optional<std::string> chat::ClientConnection::pop_recv_buffer()
{
    size_t pos = recv_buffer_.find('\n');
    if (pos == std::string::npos)
    {
        return std::nullopt;
    }
    std::string message = recv_buffer_.substr(0, pos);
    recv_buffer_.erase(0, pos + 1);
    if (!message.empty() && message.back() == '\r')
    {
        message.pop_back();
    }
    return message;
}

bool chat::ClientConnection::has_pending_send() const noexcept
{
    return !send_buffer_.empty();
}
const char *chat::ClientConnection::send_data() const noexcept
{
    return send_buffer_.data();
}
size_t chat::ClientConnection::send_size() const noexcept
{
    return send_buffer_.size();
}
void chat::ClientConnection::consume_sent(size_t size)
{
    if (size >= send_buffer_.size())
    {
        send_buffer_.clear();
        return;
    }
    send_buffer_.erase(0, size);
}

bool chat::ClientConnection::receive_paused() const noexcept
{
    return receive_paused_;
}

bool chat::ClientConnection::refresh_backpressure_state() noexcept
{
    const bool previous = receive_paused_;
    if (!receive_paused_ && send_buffer_.size() >= send_high_watermark)
    {
        receive_paused_ = true;
    }
    else if (receive_paused_ && send_buffer_.size() <= send_low_watermark)
    {
        receive_paused_ = false;
    }
    return previous != receive_paused_;
}
