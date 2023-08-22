#ifndef DRBDCMDCONSTS_H
#define DRBDCMDCONSTS_H

#include <default_types.h>
#include <string>

namespace drbdcmd
{
    extern const std::string    DRBDADM_CMD;
    extern const std::string    DRBDSETUP_CMD;

    extern const std::string    ARG_START;
    extern const std::string    ARG_STOP;
    extern const std::string    ARG_ADJUST;
    extern const std::string    ARG_PRIMARY;
    extern const std::string    ARG_SECONDARY;
    extern const std::string    ARG_CONNECT;
    extern const std::string    ARG_DISCONNECT;
    extern const std::string    ARG_ATTACH;
    extern const std::string    ARG_DETACH;
    extern const std::string    ARG_VERIFY;
    extern const std::string    ARG_PAUSE_SYNC;
    extern const std::string    ARG_RESUME_SYNC;
    extern const std::string    ARG_RESIZE;
    extern const std::string    ARG_INVALIDATE;
    extern const std::string    ARG_FORCE;
    extern const std::string    ARG_DISCARD;

    extern const std::string    ARG_ALL;
}

#endif /* DRBDCMDCONSTS_H */
