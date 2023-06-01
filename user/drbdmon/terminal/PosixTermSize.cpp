#include <terminal/PosixTermSize.h>

extern "C"
{
    #include <unistd.h>
}

PosixTermSize::PosixTermSize()
{
    term_size.ws_col    = 0;
    term_size.ws_row    = 0;
    term_size.ws_xpixel = 0;
    term_size.ws_ypixel = 0;
}

PosixTermSize::~PosixTermSize() noexcept
{
}

bool PosixTermSize::probe_terminal_size() noexcept
{
    valid = (ioctl(STDOUT_FILENO, TIOCGWINSZ, &term_size) == 0);
    return valid;
}

bool PosixTermSize::is_valid() const noexcept
{
    return valid;
}

uint16_t PosixTermSize::get_size_x() const noexcept
{
    return static_cast<uint16_t> (term_size.ws_col);
}

uint16_t PosixTermSize::get_size_y() const noexcept
{
    return static_cast<uint16_t> (term_size.ws_row);
}
