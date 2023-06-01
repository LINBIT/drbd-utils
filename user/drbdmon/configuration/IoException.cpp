#include <configuration/IoException.h>

IoException::IoException():
    std::exception()
{
}

IoException::IoException(const std::string& message):
    std::exception(),
    error_message(message)
{
}

IoException::~IoException() noexcept
{
}

const char* IoException::what() const noexcept
{
    return error_message.c_str();
}
