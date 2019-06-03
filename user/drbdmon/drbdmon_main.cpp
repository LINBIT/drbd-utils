#include <new>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <new>
#include <stdexcept>
#include <memory>

extern "C"
{
    #include <unistd.h>
    #include <time.h>
    #include <sys/wait.h>
    #include <signal.h>
    #include <sys/utsname.h>
    #include <fcntl.h>
    #include <drbd_buildtag.h>
}

#include <DrbdMon.h>
#include <MessageLog.h>
#include <colormodes.h>

// LOG_CAPACITY must be >= 1
const size_t LOG_CAPACITY {10};
const size_t DEBUG_LOG_CAPACITY {100};

// Clear screen escape sequence
const char* ANSI_CLEAR = "\x1B[H\x1B[2J";

const char* WINDOW_TITLE_APP = "LINBIT\xC2\xAE DrbdMon";

// Delay of 3 seconds for delayed restarts
static const time_t DELAY_SECS = 3;
static const long DELAY_NANOSECS = 0;

static void reset_delay(struct timespec& delay) noexcept;
static void clear_screen() noexcept;
static void cond_print_error_header(
    bool& error_header_printed,
    const std::unique_ptr<std::string>& node_name
) noexcept;
static bool adjust_ids(MessageLog* log, bool& ids_safe);
static void query_node_name(std::unique_ptr<std::string>& node_name);
static void set_window_title(const std::unique_ptr<std::string>& node_name);
static void clear_window_title();
static color_mode get_color_mode(const char*& invalid_value) noexcept;

int main(int argc, char* argv[])
{
    int exit_code {0};

    struct timespec delay;
    struct timespec remaining;
    reset_delay(delay);
    reset_delay(remaining);

    DrbdMon::fail_info fail_data {DrbdMon::fail_info::NONE};
    DrbdMon::finish_action fin_action {DrbdMon::finish_action::RESTART_DELAYED};

    std::unique_ptr<MessageLog> log;
    std::unique_ptr<MessageLog> debug_log;
    std::unique_ptr<std::string> node_name;

    std::ios_base::sync_with_stdio(true);

    bool ids_safe {false};
    while (fin_action != DrbdMon::finish_action::TERMINATE &&
           fin_action != DrbdMon::finish_action::TERMINATE_NO_CLEAR)
    {
        std::cout.clear();
        std::cerr.clear();

        bool error_header_printed {false};
        try
        {
            const char* invalid_color_value = nullptr;
            color_mode colors = get_color_mode(invalid_color_value);
            if (log == nullptr)
            {
                // std::out_of_range exception not handled, as it is
                // only thrown if LOG_CAPACITY < 1
                log = std::unique_ptr<MessageLog>(new MessageLog(LOG_CAPACITY));
            }
            if (debug_log == nullptr)
            {
                // std::out_of_range exception not handled, as it is
                // only thrown if LOG_CAPACITY < 1
                debug_log = std::unique_ptr<MessageLog>(new MessageLog(DEBUG_LOG_CAPACITY));
            }

            if (invalid_color_value != nullptr)
            {
                std::string error_msg("Invalid environment value \"");
                error_msg += invalid_color_value;
                error_msg += "\" for key ";
                error_msg += DrbdMon::ENV_COLOR_MODE;
                error_msg += " ignored.";
                log->add_entry(MessageLog::log_level::WARN, error_msg);
            }

            if (node_name == nullptr)
            {
                query_node_name(node_name);
            }
            set_window_title(node_name);

            if (ids_safe || adjust_ids(log.get(), ids_safe))
            {
                const std::unique_ptr<DrbdMon> dm_instance(
                    new DrbdMon(argc, argv, *log, *debug_log, fail_data, node_name.get(), colors)
                );
                dm_instance->run();
                fin_action = dm_instance->get_fin_action();
                if (fin_action != DrbdMon::finish_action::TERMINATE_NO_CLEAR)
                {
                    clear_screen();
                }
            }
            else
            {
                fin_action = DrbdMon::finish_action::TERMINATE;
            }
        }
        catch (std::bad_alloc& out_of_memory_exc)
        {
            clear_screen();
            fin_action = DrbdMon::finish_action::RESTART_DELAYED;
            fail_data = DrbdMon::fail_info::OUT_OF_MEMORY;
        }

        // Clear any possible stream error states
        std::cout.clear();
        std::cerr.clear();

        // Display any log messages
        if (log != nullptr)
        {
            if (log->has_entries())
            {
                cond_print_error_header(error_header_printed, node_name);
                std::cout << "** " << DrbdMon::PROGRAM_NAME << " messages log\n\n";
                log->display_messages(std::cout);
                std::cout << std::endl;
            }
        }

        if (fail_data == DrbdMon::fail_info::OUT_OF_MEMORY)
        {
            cond_print_error_header(error_header_printed, node_name);
            std::cout << "** " << DrbdMon::PROGRAM_NAME << ": Out of memory, trying to restart" << std::endl;
        }

        // Cleanup any zombie child processes
        pid_t child_pid {0};
        do
        {
            child_pid = waitpid(-1, nullptr, WNOHANG);
        }
        while (child_pid > 0);

        if (fin_action == DrbdMon::finish_action::DEBUG_MODE)
        {
            std::cout << "** " << DrbdMon::PROGRAM_NAME << " v" << DrbdMon::VERSION << " (" GITHASH << ")\n";
            std::cout << "** Debug messages log\n";
            if (debug_log->has_entries())
            {
                debug_log->display_messages(std::cout);
            }
            else
            {
                std::cout << "The log contains no debug messages" << std::endl;
            }

            // Attempt to set blocking mode on stdin
            int io_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
            if (io_flags != -1)
            {
                fcntl(STDIN_FILENO, F_SETFL, io_flags & ~O_NONBLOCK);
            }

            bool skip_prompt = false;
            while (fin_action == DrbdMon::finish_action::DEBUG_MODE)
            {
                std::cout.clear();
                if (skip_prompt)
                {
                    skip_prompt = false;
                }
                else
                {
                    std::cout << "\n[C] Clear debug messages, [Q] Quit, [R] Restart" << std::endl;
                }

                int key = std::fgetc(stdin);
                if (key == 'c' || key == 'C')
                {
                    debug_log->clear();
                    std::cout << "Debug messages log cleared" << std::endl;
                }
                else
                if (key == 'q' || key == 'Q')
                {
                    fin_action = DrbdMon::finish_action::TERMINATE;
                }
                else
                if (key == 'r' || key == 'R')
                {
                    fin_action = DrbdMon::finish_action::RESTART_IMMED;
                }
                else
                if (key == '\n')
                {
                    // Entering characters in canonical mode requires pressing enter,
                    // thereby causing the next read from stdin to read a newline
                    // Avoid printing the prompt twice in this case
                    skip_prompt = true;
                }
            }
        }

        if (fin_action == DrbdMon::finish_action::RESTART_DELAYED)
        {
            cond_print_error_header(error_header_printed, node_name);
            std::cout << "** " << DrbdMon::PROGRAM_NAME << ": Reinitializing in " <<
                static_cast<unsigned int> (delay.tv_sec) << " seconds" << std::endl;

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
            while (nanosleep(&delay, &remaining) != 0)
            {
                delay = remaining;
            }
            reset_delay(delay);
        }
        else
        if (fin_action == DrbdMon::finish_action::RESTART_IMMED)
        {
            cond_print_error_header(error_header_printed, node_name);
            std::cout << "** " << DrbdMon::PROGRAM_NAME << ": Reinitializing immediately" << std::endl;
        }
    }
    clear_window_title();

    std::fflush(stdout);
    std::fflush(stderr);

    return exit_code;
}

static void reset_delay(struct timespec& delay) noexcept
{
    delay.tv_sec  = DELAY_SECS;
    delay.tv_nsec = DELAY_NANOSECS;
}

static void clear_screen() noexcept
{
    std::cout << ANSI_CLEAR << std::flush;
}

static void cond_print_error_header(
    bool& error_header_printed, const std::unique_ptr<std::string>& node_name
) noexcept
{
    if (!error_header_printed)
    {
        std::cout << "** " << DrbdMon::PROGRAM_NAME << " v" << DrbdMon::VERSION << '\n';
        if (node_name != nullptr)
        {
            std::string* node_name_ptr = node_name.get();
            std::cout << "    Node " << *node_name_ptr << '\n';
        }
        error_header_printed = true;
    }
}

// @throws std::bad_alloc
static bool adjust_ids(MessageLog* log, bool& ids_safe)
{
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
        log->add_entry(
            MessageLog::log_level::ALERT,
            "Adjusting the process' user/group ids failed"
        );
    }
    return ids_safe;
}

// @throws std::bad_alloc
static void query_node_name(std::unique_ptr<std::string>& node_name)
{
    std::unique_ptr<struct utsname> uname_buffer(new struct utsname);
    if (uname(uname_buffer.get()) == 0)
    {
        node_name = std::unique_ptr<std::string>(new std::string(uname_buffer->nodename));
    }
}

static void set_window_title(const std::unique_ptr<std::string>& node_name)
{
    if (node_name != nullptr)
    {
        std::string* node_name_ptr = node_name.get();
        std::cout << "\x1B]2;" << WINDOW_TITLE_APP << "(Node " << *node_name_ptr << ")\x07";
    }
    else
    {
        std::cout << "\x1B]2;" << WINDOW_TITLE_APP << "\x07";
    }
    std::cout << std::flush;
}

static void clear_window_title()
{
    std::cout << "\x1B]2; \x07" << std::flush;
}

static color_mode get_color_mode(const char*& invalid_value) noexcept
{
    // Default to extended color mode
    color_mode result = color_mode::EXTENDED;
    if (environ != nullptr)
    {
        const char* env_value = getenv(DrbdMon::ENV_COLOR_MODE);
        if (env_value != nullptr)
        {
            // If the environment variable is set, but something else than extended color mode is selected,
            // default to basic color mode
            if (std::strcmp(DrbdMon::COLOR_MODE_EXTENDED, env_value) != 0)
            {
                result = color_mode::BASIC;
                if (std::strcmp(DrbdMon::COLOR_MODE_BASIC, env_value) != 0)
                {
                    invalid_value = env_value;
                }
            }
        }
    }
    return result;
}
