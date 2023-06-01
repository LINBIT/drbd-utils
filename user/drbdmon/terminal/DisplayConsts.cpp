#include <terminal/DisplayConsts.h>

namespace DisplayConsts
{
    const uint32_t  MAX_PAGE_NR         = 99999;
    const uint8_t   PAGE_NAV_X          = 22;
    const uint8_t   PAGE_NAV_Y          = 2;
    const uint8_t   CURSOR_LABEL_X      = 31;
    const uint8_t   SLCT_MODE_X         = 41;
    const uint8_t   PRB_MODE_X          = 55;
    const uint8_t   PAGE_NAV_CURSOR_X   = 13;
    const uint8_t   CMD_LINE_X          = 11;
    const uint8_t   CMD_LINE_Y          = 1;
    const uint16_t  MAX_CMD_LENGTH      = 400;
    const uint16_t  HOURGLASS_X         = 10;
    const uint16_t  MAX_SELECT_X        = 3;

    const uint16_t  VLM_NONE            = static_cast<uint16_t> (0xFFFF);

    const std::string   CMD_TOKEN_DELIMITER     = {" "};

    const std::string   CMD_EXIT                = {"EXIT"};
    const std::string   CMD_DISPLAY             = {"DISPLAY"};
    const std::string   CMD_SELECT              = {"SELECT"};
    const std::string   CMD_DESELECT            = {"DESELECT"};
    const std::string   CMD_CLEAR_SELECT        = {"CLEAR-SELECTION"};
    const std::string   CMD_FILTER_INC          = {"INCLUDE"};
    const std::string   CMD_FILTER_EXC          = {"EXCLUDE"};
    const std::string   CMD_SUSPEND             = {"SUSPEND"};
    const std::string   CMD_EXECUTE             = {"EXECUTE"};
    const std::string   CMD_QUEUE               = {"QUEUE"};
    const std::string   CMD_DRBD_ADJUST         = {"/ADJUST"};
    const std::string   CMD_DRBD_DOWN           = {"/DOWN"};
    const std::string   CMD_DRBD_CONNECT        = {"/CONNECD"};
    const std::string   CMD_DRBD_DISCONNECT     = {"/DISCONNECT"};
    const std::string   CMD_DRBD_DETACH         = {"/DETACH"};
    const std::string   CMD_DRBD_ATTACH         = {"/ATTACH"};
    const std::string   CMD_DRBD_CONNECT_DSC    = {"/DISCARD"};
    const std::string   CMD_DRBD_PAUSE_SYNC     = {"/PAUSE-SYNC"};
    const std::string   CMD_DRBD_RESUME_SYNC    = {"/RESUME-SYNC"};
    const std::string   CMD_DRBD_RESIZE         = {"/RESIZE"};
    const std::string   CMD_DRBD_VERIFY         = {"/VERIFY"};
    const std::string   CMD_DRBD_INVALIDATE     = {"/INVALIDATE"};

    const size_t        ACTION_DESC_PREALLOC    = 200;
}
