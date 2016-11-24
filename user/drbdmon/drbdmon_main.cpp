#include <new>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <new>
#include <stdexcept>

extern "C"
{
    #include <time.h>
    #include <sys/wait.h>
    #include <signal.h>
}

#include <DrbdMon.h>
#include <MessageLog.h>

// LOG_CAPACITY must be >= 1
const size_t LOG_CAPACITY {30};

// Clear screen escape sequence
const char* ANSI_CLEAR = "\x1b[H\x1b[2J";

static void reset_delay(struct timespec& delay);
static void clear_screen();
static bool adjust_ids(MessageLog* log, bool& ids_safe);

int main(int argc, char* argv[])
{
    int exit_code {0};

    // Delay of 15 seconds for delayed restarts
    struct timespec delay;
    struct timespec remaining;
    reset_delay(delay);
    reset_delay(remaining);

    DrbdMon::fail_info fail_data {DrbdMon::fail_info::NONE};
    DrbdMon::finish_action fin_action {DrbdMon::finish_action::RESTART_DELAYED};

    MessageLog* log {nullptr};

    bool ids_safe {false};
    while (fin_action != DrbdMon::finish_action::TERMINATE &&
           fin_action != DrbdMon::finish_action::TERMINATE_NO_CLEAR)
    {
        DrbdMon* ls_instance {nullptr};
        try
        {
            if (log == nullptr)
            {
                // std::out_of_range exception not handled, as it is
                // only thrown if LOG_CAPACITY < 1
                log = new MessageLog(LOG_CAPACITY);
            }

            if (ids_safe || adjust_ids(log, ids_safe))
            {
                ls_instance = new DrbdMon(argc, argv, *log, fail_data);
                ls_instance->run();
                fin_action = ls_instance->get_fin_action();
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

        // Display any log messages
        if (log != nullptr)
        {
            if (log->has_entries())
            {
                std::fputs("** DrbdMon messages log\n\n", stdout);
                log->display_messages(stderr);
                fputc('\n', stdout);
            }
        }

        if (fail_data == DrbdMon::fail_info::OUT_OF_MEMORY)
        {
            std::fputs("** DrbdMon: Out of memory, trying to restart\n", stdout);
        }

        // Deallocate the DrbdMon instance
        if (ls_instance != nullptr)
        {
            delete ls_instance;
            ls_instance = nullptr;
        }

        // Cleanup any zombie child processes
        pid_t child_pid {0};
        do
        {
            child_pid = waitpid(-1, nullptr, WNOHANG);
        }
        while (child_pid > 0);

        if (fin_action == DrbdMon::finish_action::RESTART_DELAYED)
        {
            std::fprintf(stdout, "** DrbdMon: Reinitializing in %u seconds\n",
                         static_cast<unsigned int> (delay.tv_sec));

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
            std::fputs("** DrbdMon: Reinitializing immediately\n", stdout);
        }
    }

    delete log;

    return exit_code;
}

static void reset_delay(struct timespec& delay)
{
    delay.tv_sec  = 15;
    delay.tv_nsec =  0;
}

static void clear_screen()
{
    std::fputs(ANSI_CLEAR, stdout);
    std::fflush(stdout);
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
