#ifndef SYSTEMAPI_H
#define SYSTEMAPI_H

#include <memory>
#include <string>
#include <MessageLog.h>
#include <subprocess/SubProcess.h>
#include <terminal/TerminalControl.h>

class SystemApi
{
  public:
    virtual ~SystemApi() noexcept
    {
    }

    virtual void pre_thread_invocation()
    {
    }

    virtual void post_thread_invocation()
    {
    }

    virtual std::unique_ptr<SubProcess> create_subprocess_handler() = 0;
    virtual std::unique_ptr<TerminalControl> create_terminal_control() = 0;
    virtual std::string get_config_file_path() = 0;
    virtual bool is_file_accessible(const char* const file_path) = 0;
};

namespace system_api
{
    std::unique_ptr<SystemApi> create_system_api();
    bool init_security(MessageLog& log);
    void init_node_name(std::unique_ptr<std::string>& node_name_mgr);
}

#endif /* SYSTEMAPI_H */
