#include <new>
#include <iostream>
#include <cstdlib>
#include <cstdio>
#include <new>
#include <stdexcept>

extern "C"
{
    #include <time.h>
    #include <wait.h>
}

#include <LiveStatus.h>

#include "MessageLog.h"

// LOG_CAPACITY must be >= 1
const size_t LOG_CAPACITY {30};

// Clear screen escape sequence
const char* ANSI_CLEAR = "\x1b[H\x1b[2J";

static void reset_delay(struct timespec& delay);
static void clear_screen();

int main(int argc, char* argv[])
{
    int exit_code = 0;

    // Delay of 15 seconds for delayed restarts
    struct timespec delay;
    struct timespec remaining;
    reset_delay(delay);
    reset_delay(remaining);

    LiveStatus::fail_info fail_data {LiveStatus::fail_info::NONE};
    LiveStatus::finish_action fin_action {LiveStatus::finish_action::RESTART_DELAYED};

    MessageLog* log {nullptr};

    while (fin_action != LiveStatus::finish_action::TERMINATE &&
           fin_action != LiveStatus::finish_action::TERMINATE_NO_CLEAR)
    {
        LiveStatus* ls_instance {nullptr};
        try
        {
            if (log == nullptr)
            {
                // std::out_of_range exception not handled, as it is
                // only thrown if LOG_CAPACITY < 1
                log = new MessageLog(LOG_CAPACITY);
            }

            ls_instance = new LiveStatus(argc, argv, *log, fail_data);
            ls_instance->run();
            fin_action = ls_instance->get_fin_action();
            if (fin_action != LiveStatus::finish_action::TERMINATE_NO_CLEAR)
            {
                clear_screen();
            }
        }
        catch (std::bad_alloc& out_of_memory_exc)
        {
            clear_screen();
            fin_action = LiveStatus::finish_action::RESTART_DELAYED;
            fail_data = LiveStatus::fail_info::OUT_OF_MEMORY;
        }

        // Display any log messages
        if (log != nullptr)
        {
            if (log->has_entries())
            {
                std::fputs("** LiveStatus messages log\n\n", stdout);
                log->display_messages(stderr);
                fputc('\n', stdout);
            }
        }

        if (fail_data == LiveStatus::fail_info::OUT_OF_MEMORY)
        {
            std::fputs("** LiveStatus: Out of memory, trying to restart\n", stdout);
        }

        // Deallocate the LiveStatus instance
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

        if (fin_action == LiveStatus::finish_action::RESTART_DELAYED)
        {
            std::fprintf(stdout, "** LiveStatus: Reinitializing in %u seconds\n",
                         static_cast<unsigned int> (delay.tv_sec));
            // Suspend to delay the restart
            while (nanosleep(&delay, &remaining) != 0)
            {
                delay = remaining;
            }
            reset_delay(delay);
        }
        else
        if (fin_action == LiveStatus::finish_action::RESTART_IMMED)
        {
            std::fputs("** LiveStatus: Reinitializing immediately\n", stdout);
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
