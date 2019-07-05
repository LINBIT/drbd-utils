#ifndef DRBDVSN_H
#define DRBDVSN_H

#include <cstdint>

enum class DrbdVersion : uint16_t
{
    UNDETERMINED    = 0,
    UNRECOGNIZED    = 1,
    DRBD_8_4        = 2,
    DRBD_9_0        = 3
};

// @throws std::bad_alloc
DrbdVersion probe_drbd_version();

#endif /* DRBDVSN_H */
