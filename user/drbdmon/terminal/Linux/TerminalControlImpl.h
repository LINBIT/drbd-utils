#ifndef LINUX_TERMINALCONTROL_H
#define LINUX_TERMINALCONTROL_H

#include <default_types.h>
#include <terminal/TerminalControl.h>

extern "C"
{
    #include <unistd.h>
    #include <termios.h>
}

class TerminalControlImpl : public TerminalControl
{
  public:
    TerminalControlImpl();
    virtual ~TerminalControlImpl() noexcept;

    // @throws TerminalControl::Exception
    virtual void adjust_terminal() override;
    // @throws TerminalControl::Exception
    virtual void restore_terminal() override;

  private:
    bool have_orig_termios {false};
    struct termios orig_termios;

    bool restore_terminal_impl() noexcept;
};

#endif /* LINUX_TERMINALCONTROL_H */
