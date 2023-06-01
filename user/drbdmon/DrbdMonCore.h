#ifndef DRBDMONCORE_H
#define DRBDMONCORE_H

#include <cstdint>
#include <platform/SystemApi.h>

class DrbdMonCore
{
  public:
    enum class fail_info : uint16_t
    {
        NONE            = 0,
        GENERIC,
        OUT_OF_MEMORY,
        EVENTS_SOURCE,
        EVENTS_IO
    };

    enum class finish_action : uint16_t
    {
        RESTART_IMMED,
        RESTART_DELAYED,
        TERMINATE,
        TERMINATE_NO_CLEAR,
        DEBUG_MODE
    };

    virtual ~DrbdMonCore() noexcept
    {
    }
    virtual SystemApi& get_system_api() const noexcept = 0;
    virtual void shutdown(const finish_action action) noexcept = 0;
    virtual uint32_t get_problem_count() const noexcept = 0;
    virtual void notify_config_changed() = 0;
};

#endif /* DRBDMONCORE_H */
