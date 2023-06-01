#ifndef DISPLAYUPDATEEVENT_H
#define DISPLAYUPDATEEVENT_H

#include <cstdint>

namespace update_event
{
    extern const uint64_t UPDATE_FLAG_DRBD;
    extern const uint64_t UPDATE_FLAG_TASK_QUEUE;
    extern const uint64_t UPDATE_FLAG_MESSAGE_LOG;
    extern const uint64_t UPDATE_FLAG_DEBUG_LOG;
}

#endif /* DISPLAYUPDATEEVENT_H */
