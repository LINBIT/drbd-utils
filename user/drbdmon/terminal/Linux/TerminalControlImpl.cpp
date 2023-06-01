#include <terminal/Linux/TerminalControlImpl.h>

TerminalControlImpl::TerminalControlImpl()
{
}

TerminalControlImpl::~TerminalControlImpl() noexcept
{
    restore_terminal_impl();
}

// @throws TerminalControl::Exception
void TerminalControlImpl::adjust_terminal()
{
    // Store current terminal settings
    bool mode_set = (tcgetattr(STDIN_FILENO, &orig_termios) == 0);
    if (!mode_set)
    {
        throw TerminalControl::Exception();
    }
    have_orig_termios = true;

    struct termios adjusted_termios;
    adjusted_termios = orig_termios;

    // Non-canonical input without echo
    tcflag_t flags_on = ISIG;
    tcflag_t flags_off = ICANON | ECHO | ECHONL;
    adjusted_termios.c_lflag |= flags_on;
    adjusted_termios.c_lflag = (adjusted_termios.c_lflag | flags_off) ^ flags_off;

    mode_set = (tcsetattr(STDIN_FILENO, TCSANOW, &adjusted_termios) == 0);
    if (!mode_set)
    {
        throw TerminalControl::Exception();
    }
}

// @throws TerminalControl::Exception
void TerminalControlImpl::restore_terminal()
{
    const bool success_flag = restore_terminal_impl();
    if (!success_flag)
    {
        throw TerminalControl::Exception();
    }
}

bool TerminalControlImpl::restore_terminal_impl() noexcept
{
    bool restored = false;
    if (have_orig_termios)
    {
        // Restore original terminal settings
        restored = (tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios) == 0);
    }
    return !have_orig_termios || restored;
}
