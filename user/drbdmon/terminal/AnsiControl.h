#ifndef ANSICONTROL_H
#define ANSICONTROL_H

#include <default_types.h>
#include <string>

class AnsiControl
{
  public:
    static const std::string ANSI_CLEAR_SCREEN;
    static const std::string ANSI_CLEAR_LINE;
    static const std::string ANSI_CURSOR_OFF;
    static const std::string ANSI_CURSOR_ON;
    static const std::string ANSI_ALTBFR_ON;
    static const std::string ANSI_ALTBFR_OFF;
    static const std::string ANSI_MOUSE_ON;
    static const std::string ANSI_MOUSE_OFF;

    // Synchronized output, DEC private mode 2026. A terminal that supports it does not present
    // any of the intermediate states of the enclosed update, it displays the finished update only.
    // Terminals that do not know the mode ignore the sequences.
    static const std::string ANSI_SYNC_BEGIN;
    static const std::string ANSI_SYNC_END;

    static const std::string ANSI_FMT_CURSOR_POS;

    AnsiControl();
    virtual ~AnsiControl() noexcept;
};

#endif /* ANSICONTROL_H */

