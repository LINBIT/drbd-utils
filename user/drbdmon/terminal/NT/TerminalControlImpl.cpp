#include <terminal/NT/TerminalControlImpl.h>

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
    const HANDLE console = GetStdHandle(STD_INPUT_HANDLE);
    bool mode_set = (GetConsoleMode(console, &orig_mode) != 0);
    if (!mode_set)
    {
        throw TerminalControl::Exception();
    }
    have_orig_mode = true;

    mode_set = SetConsoleMode(
        console,
        ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT |
        ENABLED_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING
    ) == 0);
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
    if (have_orig_mode)
    {
        const HANDLE console = GetStdHandle(STD_INPUT_HANDLE);
        restored = SetConsoleMode(console, orig_mode) != 0;
    }
    return !have_orig_mode || restored;
}
