#include <platform/Linux/SystemApiImpl.h>
#include <subprocess/Linux/SubProcessLx.h>
#include <terminal/Linux/TerminalControlImpl.h>
#include <exceptions.h>
#include <cstring>

extern "C"
{
    #include <unistd.h>
    #include <sys/utsname.h>
    #include <sys/stat.h>
}

const char* const   LinuxApi::CONFIG_FILE_NAME  = ".drbdmon.ces";
const char* const   LinuxApi::ENV_KEY_HOME      = "HOME";

extern char** environ;

LinuxApi::LinuxApi()
{
    sigemptyset(&saved_mask);
}

LinuxApi::~LinuxApi() noexcept
{
}

void LinuxApi::pre_thread_invocation()
{
    sigset_t thread_mask;
    sigfillset(&thread_mask);
    sigemptyset(&saved_mask);
    sigprocmask(SIG_BLOCK, &thread_mask, &saved_mask);
}

void LinuxApi::post_thread_invocation()
{
    sigprocmask(SIG_SETMASK, &saved_mask, nullptr);
}

std::unique_ptr<SubProcess> LinuxApi::create_subprocess_handler()
{
    return std::unique_ptr<SubProcess>(dynamic_cast<SubProcess*> (new SubProcessLx()));
}

std::unique_ptr<TerminalControl> LinuxApi::create_terminal_control()
{
    return std::unique_ptr<TerminalControl>(dynamic_cast<TerminalControl*> (new TerminalControlImpl()));
}

bool LinuxApi::is_file_accessible(const char* const file_path)
{
    struct stat file_info;
    std::memset(&file_info, 0, sizeof (file_info));

    const int stat_rc = stat(file_path, &file_info);
    return stat_rc == 0;
}

std::string LinuxApi::get_config_file_path()
{
    std::string path;
    if (environ != nullptr)
    {
        const char* const home_dir = getenv(ENV_KEY_HOME);
        if (home_dir != nullptr)
        {
            path.append(home_dir);
            path.append("/");
            path.append(CONFIG_FILE_NAME);
        }
    }
    return path;
}

namespace system_api
{
    std::unique_ptr<SystemApi> create_system_api()
    {
        return std::unique_ptr<SystemApi>(dynamic_cast<SystemApi*> (new LinuxApi()));
    }

    bool init_security(MessageLog& log)
    {
        bool ids_safe = false;
        try
        {
            // Get the user ids of the current process
            uid_t real_user {0};
            uid_t eff_user {0};
            uid_t saved_user {0};
            if (getresuid(&real_user, &eff_user, &saved_user) != 0)
            {
                throw SysException();
            }

            // Get the group ids of the current process
            gid_t real_group {0};
            gid_t eff_group {0};
            gid_t saved_group {0};
            if (getresgid(&real_group, &eff_group, &saved_group) != 0)
            {
                throw SysException();
            }

            // Unless already equal, set effective user id = real user id, or
            // if the real user id is root, then this program is probably setuid
            // to some non-root account and being executed by root in an attempt
            // to drop root privileges, therefore set the real user id to the
            // effective user id in this case.
            if (eff_user != real_user)
            {
                if (real_user == 0)
                {
                    real_user = eff_user;
                }
                else
                {
                    eff_user = real_user;
                }

                if (setresuid(real_user, eff_user, -1) != 0)
                {
                    throw SysException();
                }
            }
            // Set saved user id = real user id unless equal already
            if (saved_user != real_user)
            {
                // Set saved user id = real user id
                saved_user = real_user;

                if (setresuid(-1, -1, saved_user) != 0)
                {
                    throw SysException();
                }
            }

            // Analogous to adjusting the user ids, adjust the group ids to
            // non-root if real group id and effective group id do not match
            if (eff_group != real_group)
            {
                if (real_group == 0)
                {
                    real_group = eff_group;
                }
                else
                {
                    eff_group = real_group;
                }

                if (setresgid(real_group, eff_group, -1) != 0)
                {
                    throw SysException();
                }
            }
            // Set saved group id = real group id unless equal already
            if (saved_group != real_group)
            {
                // Set saved group id = real group id
                saved_group = real_group;

                if (setresgid(-1, -1, saved_group) != 0)
                {
                    throw SysException();
                }
            }

            // Cross-check adjusted set user & group ids
            if (getresuid(&real_user, &eff_user, &saved_user) != 0 ||
                getresgid(&real_group, &eff_group, &saved_group) != 0)
            {
                throw SysException();
            }
            if (real_user == eff_user && real_user == saved_user &&
                real_group == eff_group && real_group == saved_group)
            {
                ids_safe = true;
            }
            else
            {
                throw SysException();
            }
        }
        catch (SysException&)
        {
            log.add_entry(
                MessageLog::log_level::ALERT,
                "Adjusting the process' user/group ids failed"
            );
        }
        return ids_safe;
    }

    void init_node_name(std::unique_ptr<std::string>& node_name_mgr)
    {
        std::unique_ptr<struct utsname> uname_buffer(new struct utsname);
        int rc = uname(uname_buffer.get());
        if (rc == 0)
        {
            node_name_mgr = std::unique_ptr<std::string>(new std::string(uname_buffer->nodename));
        }
    }
}
