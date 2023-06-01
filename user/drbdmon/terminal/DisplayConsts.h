#ifndef DISPLAYCONSTS_H
#define DISPLAYCONSTS_H

#include <cstdint>
#include <string>

namespace DisplayConsts
{
    enum class task_queue_type : uint8_t
    {
        ACTIVE_QUEUE,
        PENDING_QUEUE,
        SUSPENDED_QUEUE,
        FINISHED_QUEUE
    };

    extern const uint32_t   MAX_PAGE_NR;
    extern const uint8_t    PAGE_NAV_X;
    extern const uint8_t    PAGE_NAV_Y;
    extern const uint8_t    CURSOR_LABEL_X;
    extern const uint8_t    SLCT_MODE_X;
    extern const uint8_t    PRB_MODE_X;
    extern const uint8_t    PAGE_NAV_CURSOR_X;
    extern const uint8_t    CMD_LINE_X;
    extern const uint8_t    CMD_LINE_Y;
    extern const uint16_t   MAX_CMD_LENGTH;
    extern const uint16_t   HOURGLASS_X;
    extern const uint16_t   MAX_SELECT_X;

    extern const uint16_t       VLM_NONE;

    extern const std::string    CMD_TOKEN_DELIMITER;

    extern const std::string    CMD_EXIT;
    extern const std::string    CMD_DISPLAY;
    extern const std::string    CMD_SELECT;
    extern const std::string    CMD_DESELECT;
    extern const std::string    CMD_CLEAR_SELECT;
    extern const std::string    CMD_FILTER_INC;
    extern const std::string    CMD_FILTER_EXC;
    extern const std::string    CMD_SUSPEND;
    extern const std::string    CMD_EXECUTE;
    extern const std::string    CMD_QUEUE;
    extern const std::string    CMD_DRBD_ADJUST;
    extern const std::string    CMD_DRBD_DOWN;
    extern const std::string    CMD_DRBD_CONNECT;
    extern const std::string    CMD_DRBD_DISCONNECT;
    extern const std::string    CMD_DRBD_DETACH;
    extern const std::string    CMD_DRBD_ATTACH;
    extern const std::string    CMD_DRBD_CONNECT_DSC;
    extern const std::string    CMD_DRBD_PAUSE_SYNC;
    extern const std::string    CMD_DRBD_RESUME_SYNC;
    extern const std::string    CMD_DRBD_RESIZE;
    extern const std::string    CMD_DRBD_VERIFY;
    extern const std::string    CMD_DRBD_INVALIDATE;

    // Preallocation size for action description texts (optimization)
    extern const size_t         ACTION_DESC_PREALLOC;
}

#endif /* DISPLAYCONSTS_H */
