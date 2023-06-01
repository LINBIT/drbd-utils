#include <terminal/EscSeqCodes.h>

const char  EscSeqCodes::ESC            = 0x1B;
const char  EscSeqCodes::SS3            = 0x4F;
const char  EscSeqCodes::CSI            = 0x5B;
const char  EscSeqCodes::CSI_SEP        = 0x3B;

const char  EscSeqCodes::CURSOR_UP      = 0x41;
const char  EscSeqCodes::CURSOR_DOWN    = 0x42;
const char  EscSeqCodes::CURSOR_RIGHT   = 0x43;
const char  EscSeqCodes::CURSOR_LEFT    = 0x44;

const char  EscSeqCodes::FUNC_01        = 0x50;
const char  EscSeqCodes::FUNC_02        = 0x51;
const char  EscSeqCodes::FUNC_03        = 0x52;
const char  EscSeqCodes::FUNC_04        = 0x53;
const char  EscSeqCodes::FUNC_TERM      = 0x7E;

const char EscSeqCodes::MOUSE_EVENT     = 0x3C;
const char EscSeqCodes::MOUSE_RELEASE   = 0x6D;
const char EscSeqCodes::MOUSE_DOWN      = 0x4D;

const char* const   EscSeqCodes::FUNC_05    = "15";
const char* const   EscSeqCodes::FUNC_06    = "17";
const char* const   EscSeqCodes::FUNC_07    = "18";
const char* const   EscSeqCodes::FUNC_08    = "19";
const char* const   EscSeqCodes::FUNC_09    = "20";
const char* const   EscSeqCodes::FUNC_10    = "21";
const char* const   EscSeqCodes::FUNC_11    = "23";
const char* const   EscSeqCodes::FUNC_12    = "24";

const char* const   EscSeqCodes::HOME_1     = "1";
const char* const   EscSeqCodes::INSERT     = "2";
const char* const   EscSeqCodes::DELETE     = "3";
const char* const   EscSeqCodes::END_1      = "4";
const char* const   EscSeqCodes::PG_UP      = "5";
const char* const   EscSeqCodes::PG_DOWN    = "6";
const char* const   EscSeqCodes::HOME_2     = "7";
const char* const   EscSeqCodes::END_2      = "8";
