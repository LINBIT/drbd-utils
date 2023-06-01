#ifndef ANSICONTROL_H
#define ANSICONTROL_H

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

    static const std::string ANSI_FMT_CURSOR_POS;

    AnsiControl();
    ~AnsiControl() noexcept;
};

#endif /* ANSICONTROL_H */

