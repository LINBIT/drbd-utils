#include <default_types.h>
#include <new>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <thread>
#include <chrono>
#include <new>
#include <memory>
#include <stdexcept>

extern "C"
{
    #include <unistd.h>
    #include <sys/wait.h>
    #include <signal.h>
    #include <sys/utsname.h>
    #include <fcntl.h>
    #include <drbd_buildtag.h>
}

#include <DrbdMon.h>
#include <DrbdMonConsts.h>
#include <MessageLog.h>
#include <configuration/Configuration.h>
#include <configuration/CfgEntryStore.h>
#include <configuration/IoException.h>
#include <persistent_configuration.h>
#include <exceptions.h>

namespace drbdmon
{
    // LOG_CAPACITY and DEBUG_LOG_CAPACITY must be >= 1
    static const size_t LOG_CAPACITY        {100};
    static const size_t DEBUG_LOG_CAPACITY  {100};

    // Clear screen escape sequence
    static const char*  ANSI_CLEAR          = "\x1B[H\x1B[2J";
    static const char*  WINDOW_TITLE_APP    = "LINBIT\xC2\xAE DRBDmon";

    static const int32_t DELAY_MSECS = 3000;

    // Command line was not valid
    static const int    EXIT_ERR_SYNTAX         = 1;

    // Configuration file I/O error
    static const int    EXIT_ERR_CONFIG_FILE    = 2;

    // Security settings initialization error
    static const int    EXIT_ERR_SECURITY       = 3;

    // Insufficient memory
    static const int    EXIT_ERR_NO_MEMORY      = 120;

    static const char* const    ACT_PRM_DSP_CONF    = "settings";
    static const char* const    ACT_PRM_SET_CONF    = "set";
    static const char* const    ACT_PRM_GET_CONF    = "get";

    static void monitor_loop(
        int                 argc,
        char*               argv[],
        MonitorEnvironment& mon_env
    );
    static void clear_screen() noexcept;
    static void cond_print_error_header(
        bool& error_header_printed,
        const std::unique_ptr<std::string>& node_name
    ) noexcept;
    static void set_window_title(const std::string* const node_name);
    static void clear_window_title();
}

int main(int argc, char* argv[])
{
    int exit_code = 0;
    std::ios_base::sync_with_stdio(true);

    try
    {
        std::unique_ptr<MonitorEnvironment> mon_env_mgr(new MonitorEnvironment());
        MonitorEnvironment& mon_env = *mon_env_mgr;

        mon_env.config = std::unique_ptr<Configuration>(new Configuration());

        // Initialize platform abstraction layer
        mon_env.sys_api = system_api::create_system_api();

        // Initialize logs
        // std::out_of_range exception not handled, as it is
        // only thrown if LOG_CAPACITY < 1
        mon_env.log = std::unique_ptr<MessageLog>(new MessageLog(drbdmon::LOG_CAPACITY));
        // std::out_of_range exception not handled, as it is
        // only thrown if LOG_CAPACITY < 1
        mon_env.debug_log = std::unique_ptr<MessageLog>(new MessageLog(drbdmon::DEBUG_LOG_CAPACITY));

        // Security initialization
        bool sec_init = system_api::init_security(*(mon_env.log));
        if (sec_init)
        {

            DrbdMonConsts::run_action_type action = DrbdMonConsts::run_action_type::DM_MONITOR;
            if (argc >= 2)
            {
                std::string action_param(argv[1]);
                if (action_param == drbdmon::ACT_PRM_DSP_CONF)
                {
                    action = DrbdMonConsts::run_action_type::DM_DSP_CONF;
                }
                else
                if (action_param == drbdmon::ACT_PRM_GET_CONF)
                {
                    action = DrbdMonConsts::run_action_type::DM_GET_CONF;
                }
                else
                if (action_param == drbdmon::ACT_PRM_SET_CONF)
                {
                   action = DrbdMonConsts::run_action_type::DM_SET_CONF;
                }
            }

            mon_env.config_file_path = mon_env.sys_api->get_config_file_path();
            if (action == DrbdMonConsts::run_action_type::DM_MONITOR)
            {
                try
                {
                    std::unique_ptr<CfgEntryStore> config_store(new CfgEntryStore());
                    if (mon_env.sys_api->is_file_accessible(mon_env.config_file_path.c_str()))
                    {
                        configuration::initialize_cfg_entry_store(*config_store);
                        configuration::load_configuration(
                            mon_env.config_file_path,
                            *(mon_env.config),
                            *config_store,
                            *(mon_env.sys_api)
                        );
                    }
                }
                catch (IoException& io_exc)
                {
                    std::string error_message;
                    error_message.reserve(500);
                    error_message += "Cannot load the ";
                    error_message += DrbdMonConsts::PROGRAM_NAME;
                    error_message += " configuration store: I/O error\n";
                    const char* const exc_message = io_exc.what();
                    if (exc_message != nullptr)
                    {
                        error_message += "Error details: ";
                        error_message += exc_message;
                        error_message += "\n";
                    }
                    error_message += "File path: ";
                    error_message += mon_env.config_file_path;
                    mon_env.log->add_entry(MessageLog::log_level::ALERT, error_message);
                }

                drbdmon::monitor_loop(argc, argv, mon_env);
            }
            else
            {
                configuration::configuration_command(argc, argv, action, mon_env);
            }
        }
        else
        {
            drbdmon::cond_print_error_header(mon_env.error_header_printed, mon_env.node_name_mgr);
            std::cout << "Application start failed, cannot adjust security settings" << std::endl;
            exit_code = drbdmon::EXIT_ERR_SECURITY;
        }
    }
    catch (std::bad_alloc& alloc_exc)
    {
        std::cout << "** " << DrbdMonConsts::PROGRAM_NAME << " v" << DrbdMonConsts::UTILS_VERSION << '\n';
        std::cout << "Application initialization failed, cannot allocate a sufficient amount of memory" << std::endl;
        exit_code = drbdmon::EXIT_ERR_NO_MEMORY;
    }
    catch (ConfigurationException& conf_exc)
    {
        std::cout << "** " << DrbdMonConsts::PROGRAM_NAME << " v" << DrbdMonConsts::UTILS_VERSION << '\n';
        std::cout << "Invalid command line: " << conf_exc.what() << std::endl;
        exit_code = drbdmon::EXIT_ERR_SYNTAX;
    }

    drbdmon::clear_window_title();

    std::fflush(stdout);
    std::fflush(stderr);

    return exit_code;
}

static void drbdmon::monitor_loop(
    int argc,
    char* argv[],
    MonitorEnvironment& mon_env
)
{
    // Main loop
    while (mon_env.fin_action != DrbdMon::finish_action::TERMINATE &&
           mon_env.fin_action != DrbdMon::finish_action::TERMINATE_NO_CLEAR)
    {
        std::cout.clear();
        std::cerr.clear();

        mon_env.error_header_printed = false;
        try
        {
            if (mon_env.node_name_mgr == nullptr)
            {
                system_api::init_node_name(mon_env.node_name_mgr);
            }
            drbdmon::set_window_title(mon_env.node_name_mgr.get());

            const std::unique_ptr<DrbdMon> dm_instance(
                new DrbdMon(argc, argv, mon_env)
            );
            dm_instance->run();
            if (mon_env.fin_action != DrbdMon::finish_action::TERMINATE_NO_CLEAR)
            {
                drbdmon::clear_screen();
            }
        }
        catch (std::bad_alloc& out_of_memory_exc)
        {
            drbdmon::clear_screen();
            mon_env.fin_action = DrbdMon::finish_action::RESTART_DELAYED;
            mon_env.fail_data = DrbdMon::fail_info::OUT_OF_MEMORY;
        }

        // Clear any possible stream error states
        std::cout.clear();
        std::cerr.clear();

        // Display any log messages
        if (mon_env.log != nullptr)
        {
            if (mon_env.log->has_entries())
            {
                drbdmon::cond_print_error_header(mon_env.error_header_printed, mon_env.node_name_mgr);
                std::cout << "** " << DrbdMonConsts::PROGRAM_NAME << " messages log\n\n";
                mon_env.log->display_messages(std::cout);
                std::cout << std::endl;
            }
        }

        if (mon_env.fail_data == DrbdMon::fail_info::OUT_OF_MEMORY)
        {
            drbdmon::cond_print_error_header(mon_env.error_header_printed, mon_env.node_name_mgr);
            std::cout << "** " << DrbdMonConsts::PROGRAM_NAME <<
                ": Out of memory, trying to restart" << std::endl;
        }
        else
        if (mon_env.fin_action == DrbdMon::finish_action::DEBUG_MODE)
        {
            // FIXME: Move debug mode elsewhere
        }
        else
        if (mon_env.fin_action == DrbdMon::finish_action::RESTART_DELAYED)
        {
            drbdmon::cond_print_error_header(mon_env.error_header_printed, mon_env.node_name_mgr);
            std::cout << "** " << DrbdMonConsts::PROGRAM_NAME << ": Reinitializing in " <<
                (static_cast<unsigned long> (drbdmon::DELAY_MSECS) / 1000) << " seconds" << std::endl;

            // Attempt to unblock signals before waiting, so one can
            // easily exit the program immediately by hitting Ctrl-C,
            // closing the terminal or sending a TERM signal
            sigset_t signal_mask;
            if (sigemptyset(&signal_mask) == 0)
            {
                // These calls are unchecked; if adding a signal fails,
                // it cannot be fixed anyway
                sigaddset(&signal_mask, SIGTERM);
                sigaddset(&signal_mask, SIGINT);
                sigaddset(&signal_mask, SIGHUP);
                sigprocmask(SIG_UNBLOCK, &signal_mask, nullptr);
            }

            // Suspend to delay the restart
            std::this_thread::sleep_for(std::chrono::milliseconds(drbdmon::DELAY_MSECS));
        }
        else
        if (mon_env.fin_action == DrbdMon::finish_action::RESTART_IMMED)
        {
            drbdmon::cond_print_error_header(mon_env.error_header_printed, mon_env.node_name_mgr);
            std::cout << "** " << DrbdMonConsts::PROGRAM_NAME << ": Reinitializing immediately" << std::endl;
        }
    }
}

static void drbdmon::clear_screen() noexcept
{
    std::cout << ANSI_CLEAR << std::flush;
}

static void drbdmon::cond_print_error_header(
    bool& error_header_printed,
    const std::unique_ptr<std::string>& node_name
) noexcept
{
    if (!error_header_printed)
    {
        std::cout << "** " << DrbdMonConsts::PROGRAM_NAME << " v" << DrbdMonConsts::UTILS_VERSION << '\n';
        if (node_name != nullptr)
        {
            std::string* node_name_ptr = node_name.get();
            std::cout << "    Node " << *node_name_ptr << '\n';
        }
        error_header_printed = true;
    }
}

static void drbdmon::set_window_title(const std::string* const node_name)
{
    if (node_name != nullptr)
    {
        std::cout << "\x1B]2;" << WINDOW_TITLE_APP << "(Node " << *node_name << ")\x07";
    }
    else
    {
        std::cout << "\x1B]2;" << WINDOW_TITLE_APP << "\x07";
    }
    std::cout << std::flush;
}

static void drbdmon::clear_window_title()
{
    std::cout << "\x1B]2; \x07" << std::flush;
}

