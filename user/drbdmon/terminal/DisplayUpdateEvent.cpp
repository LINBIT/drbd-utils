#include <terminal/DisplayUpdateEvent.h>

namespace update_event
{
    const uint64_t UPDATE_FLAG_DRBD         = static_cast<uint64_t> (0x1ULL);
    const uint64_t UPDATE_FLAG_TASK_QUEUE   = static_cast<uint64_t> (0x2ULL);
    const uint64_t UPDATE_FLAG_MESSAGE_LOG  = static_cast<uint64_t> (0x4ULL);
    const uint64_t UPDATE_FLAG_DEBUG_LOG    = static_cast<uint64_t> (0x8ULL);
}
