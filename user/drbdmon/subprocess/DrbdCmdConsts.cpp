#include <subprocess/DrbdCmdConsts.h>

namespace drbdcmd
{
    const std::string   DRBDADM_CMD("/usr/sbin/drbdadm");
    const std::string   DRBDSETUP_CMD("/usr/sbin/drbdsetup");

    const std::string   ARG_START("up");
    const std::string   ARG_STOP("down");
    const std::string   ARG_ADJUST("adjust");
    const std::string   ARG_PRIMARY("primary");
    const std::string   ARG_SECONDARY("secondary");
    const std::string   ARG_CONNECT("connect");
    const std::string   ARG_DISCONNECT("disconnect");
    const std::string   ARG_ATTACH("attach");
    const std::string   ARG_DETACH("detach");
    const std::string   ARG_VERIFY("verify");
    const std::string   ARG_PAUSE_SYNC("pause-sync");
    const std::string   ARG_RESUME_SYNC("resume-sync");
    const std::string   ARG_RESIZE("resize");
    const std::string   ARG_INVALIDATE("invalidate");
    const std::string   ARG_INVALIDATE_REMOTE("invalidate-remote");
    const std::string   ARG_FORCE("--force");
    const std::string   ARG_DISCARD("--discard-my-data");

    const std::string   ARG_ALL("all");
}
