#ifndef TERMINALCONTROL_H
#define TERMINALCONTROL_H

#include <default_types.h>
#include <stdexcept>

class TerminalControl
{
  public:
    virtual ~TerminalControl() noexcept
    {
    }

    // @throws TerminalControl::Exception
    virtual void adjust_terminal() = 0;
    // @throws TerminalControl::Exception
    virtual void restore_terminal() = 0;

    class Exception : public std::exception
    {
      public:
        Exception();
        virtual ~Exception() noexcept;

        virtual const char* what() const noexcept;
    };
};

#endif /* TERMINALCONTROL_H */
