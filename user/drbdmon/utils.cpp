#include <utils.h>

TermSize::TermSize()
{
    term_size.ws_col    = 0;
    term_size.ws_row    = 0;
    term_size.ws_xpixel = 0;
    term_size.ws_ypixel = 0;
}

bool TermSize::probe_terminal_size()
{
    valid = (ioctl(1, TIOCGWINSZ, &term_size) == 0);
    return valid;
}

bool TermSize::is_valid() const
{
    return valid;
}

uint16_t TermSize::get_size_x() const
{
    return static_cast<uint16_t> (term_size.ws_col);
}

uint16_t TermSize::get_size_y() const
{
    return static_cast<uint16_t> (term_size.ws_row);
}
