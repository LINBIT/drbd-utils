#include <terminal/TerminalControl.h>

TerminalControl::Exception::Exception()
{
}

TerminalControl::Exception::~Exception() noexcept
{
}

const char* TerminalControl::Exception::what() const noexcept
{
    return "Adjusting the terminal mode failed";
}
