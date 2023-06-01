#ifndef PERSISTENT_CONFIGURATION_H
#define PERSISTENT_CONFIGURATION_H

#include <configuration/Configuration.h>
#include <configuration/CfgEntryStore.h>
#include <platform/SystemApi.h>
#include <DrbdMonConsts.h>
#include <MonitorEnvironment.h>

namespace configuration
{
    extern const size_t KEY_WIDTH;
    extern const size_t VALUE_WIDTH;
    extern const size_t TYPE_WIDTH;

    void initialize_cfg_entry_store(CfgEntryStore& config_store);
    void load_configuration(
        const std::string   file_path,
        Configuration&      config,
        CfgEntryStore&      config_store,
        SystemApi&          sys_api
    );
    void save_configuration(
        const std::string   file_path,
        Configuration&      config,
        CfgEntryStore&      config_store,
        SystemApi&          sys_api
    );
    void display_configuration(CfgEntryStore& config_store);
    void display_entry(const CfgEntry* const entry);
    void get_configuration_entry(
        CfgEntryStore&      config_store,
        const std::string&  config_key
    );
    void set_configuration_entry(
        CfgEntryStore&      config_store,
        const std::string&  config_key,
        const std::string&  config_value,
        MonitorEnvironment& mon_env
    );
    void configuration_command(
        int                             argc,
        char*                           argv[],
        DrbdMonConsts::run_action_type  action,
        MonitorEnvironment&             mon_env
    );
    std::string get_config_key(int argc, char* argv[]);
    std::string get_config_value(int argc, char* argv[]);
}

#endif // PERSISTENT_CONFIGURATION_H
