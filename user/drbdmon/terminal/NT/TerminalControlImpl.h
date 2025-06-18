#ifndef NT_TERMINALCONTROLIMPL_H
#define NT_TERMINALCONTROLIMPL_H

#include <default_types.h>
#include <terminal/TerminalControl.h>

extern "C"
{
    #include <windows.h>
}

class TerminalControlImpl : public TerminalControl
{
  public:
    TerminalControlImpl();
    virtual ~TerminalControlImpl() noexcept override;

    // @throws TerminalControl::Exception
    virtual void adjust_terminal() override;
    // @throws TerminalControl::Exception
    virtual void restore_terminal() override;

  private:
    bool    have_orig_mode  {false};
    DWORD   orig_mode       {0};

    bool restore_terminal_impl() noexcept;
};

#endif /* NT_TERMINALCONTROLIMPL_H */
