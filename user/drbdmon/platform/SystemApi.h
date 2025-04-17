#ifndef SYSTEMAPI_H
#define SYSTEMAPI_H

#include <default_types.h>
#include <memory>
#include <string>
#include <stdexcept>
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
    virtual void prepare_save_config_file() = 0;
    virtual std::string file_name_for_path(const std::string path) const = 0;
    virtual bool is_file_accessible(const char* const file_path) const = 0;
};

namespace system_api
{
    std::unique_ptr<SystemApi> create_system_api();
    bool init_security(MessageLog& log);
    void init_node_name(std::string& node_name);
}

#endif /* SYSTEMAPI_H */
