#ifndef MONITORENVIRONMENT_H
#define MONITORENVIRONMENT_H

#include <default_types.h>
#include <DrbdMonCore.h>
#include <platform/SystemApi.h>
#include <configuration/Configuration.h>
#include <MessageLog.h>
#include <string>

class MonitorEnvironment
{
  public:
    bool                            error_header_printed    {false};
    DrbdMonCore::fail_info          fail_data               {DrbdMonCore::fail_info::NONE};
    DrbdMonCore::finish_action      fin_action              {DrbdMonCore::finish_action::RESTART_DELAYED};
    std::unique_ptr<SystemApi>      sys_api;
    std::unique_ptr<MessageLog>     log;
    std::unique_ptr<MessageLog>     debug_log;
    std::unique_ptr<Configuration>  config;
    std::unique_ptr<std::string>    node_name_mgr;
    std::string                     config_file_path;
};

#endif /* MONITORENVIRONMENT_H */
