#include <terminal/AnsiControl.h>

const std::string AnsiControl::ANSI_CLEAR_SCREEN    = "\x1B[f\x1B[0J\x1B[f";
const std::string AnsiControl::ANSI_CLEAR_LINE      = "\x1B[K";
const std::string AnsiControl::ANSI_CURSOR_OFF      = "\x1B[?25l";
const std::string AnsiControl::ANSI_CURSOR_ON       = "\x1B[?25h";
const std::string AnsiControl::ANSI_ALTBFR_ON       = "\x1B[?1049h";
const std::string AnsiControl::ANSI_ALTBFR_OFF      = "\x1B[?1049l";
const std::string AnsiControl::ANSI_MOUSE_ON        = "\x1B[?1000h\x1B[?1006h";
const std::string AnsiControl::ANSI_MOUSE_OFF       = "\x1B[?1000l";

const std::string AnsiControl::ANSI_FMT_CURSOR_POS  = "\x1B[%u;%uf";

AnsiControl::AnsiControl()
{
}

AnsiControl::~AnsiControl() noexcept
{
}
