#ifndef DRBDMONCONSTS_H
#define DRBDMONCONSTS_H

#include <string>

class DrbdMonConsts
{
  public:
    enum run_action_type : uint8_t
    {
        DM_MONITOR      = 0,
        DM_DSP_CONF,
        DM_GET_CONF,
        DM_SET_CONF
    };

    static const std::string PROGRAM_NAME;
    static const std::string PROJECT_VERSION;
    static const std::string UTILS_VERSION;
    static const std::string BUILD_HASH;
};

#endif /* DRBDMONCONSTS_H */
