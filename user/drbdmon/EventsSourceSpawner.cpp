#include "EventsSourceSpawner.h"
#include <cstring>

extern "C"
{
    #include <unistd.h>
    #include <signal.h>
    #include <spawn.h>
    #include <fcntl.h>
    #include <sys/wait.h>
    #include <errno.h>
}

const char* EventsSourceSpawner::EVENTS_PROGRAM = "/usr/sbin/drbdsetup";
const char* EventsSourceSpawner::EVENTS_PROGRAM_ARGS[] =
{
    "drbdsetup",
    "events2",
    "all",
    nullptr
};

const int EventsSourceSpawner::PIPE_READ_SIDE  = 0;
const int EventsSourceSpawner::PIPE_WRITE_SIDE = 1;

EventsSourceSpawner::EventsSourceSpawner()
{
}

EventsSourceSpawner::~EventsSourceSpawner()
{
    try
    {
        cleanup_child_processes_impl();
    }
    catch (EventsSourceException&)
    {
        // Triggered by cleanup_child_processes_impl() if the events source
        // process tracked by this instance exited
    }

    terminate_child_process();
}

pid_t EventsSourceSpawner::get_process_id()
{
    return spawned_pid;
}

int EventsSourceSpawner::get_events_source_fd()
{
    return pipe_fd[PIPE_READ_SIDE];
}

// @throws std::bad_alloc, EventSourceException
int EventsSourceSpawner::spawn_source()
{
    posix_spawn_file_actions_t* pipe_init_actions {nullptr};
    posix_spawnattr_t* spawn_attr {nullptr};
    char** spawn_args {nullptr};

    // Indicates that posix_spawn_file_actions_init() was executed on pipe_init_actions
    // and requires cleanup by executing posix_spawn_file_actions_destroy()
    bool cleanup_pipe_init_actions {false};
    // Indicates that posix_spawnattr_init() was executed on spawn_attr
    // and requires cleanup by executing posix_spawnattr_destroy()
    bool cleanup_spawn_attr {false};

    try
    {
        // Initialize the pipe
        checked_int_rc(pipe2(pipe_fd, 0));
        // Make the pipe's read end nonblocking
        fcntl(pipe_fd[PIPE_READ_SIDE], F_SETFL, O_NONBLOCK);
        // Make stdin nonblocking
        fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);

        // Initialize the datastructures for posix_spawn())
        {
            pipe_init_actions = new posix_spawn_file_actions_t;
            spawn_attr = new posix_spawnattr_t;

            // Initialize a copy of the spawn arguments
            spawn_args = init_spawn_args();

            checked_int_rc(posix_spawn_file_actions_init(pipe_init_actions));
            cleanup_pipe_init_actions = true;

            // Redirect stdout to the pipe's write side
            checked_int_rc(
                posix_spawn_file_actions_adddup2(
                    pipe_init_actions,
                    pipe_fd[PIPE_WRITE_SIDE],
                    STDOUT_FILENO
                )
            );
            // Close the read end of the pipe
            checked_int_rc(
                posix_spawn_file_actions_addclose(
                    pipe_init_actions,
                    pipe_fd[PIPE_READ_SIDE]
                )
            );

            checked_int_rc(posix_spawnattr_init(spawn_attr));
            cleanup_spawn_attr = true;

            // Reset ignored signals to the default action
            sigset_t mask;
            checked_int_rc(sigemptyset(&mask));
            checked_int_rc(sigaddset(&mask, SIGTERM));
            checked_int_rc(sigaddset(&mask, SIGHUP));
            checked_int_rc(sigaddset(&mask, SIGINT));
            checked_int_rc(sigaddset(&mask, SIGWINCH));
            checked_int_rc(sigaddset(&mask, SIGCHLD));
            checked_int_rc(posix_spawnattr_setsigdefault(spawn_attr, &mask));
            checked_int_rc(posix_spawnattr_setflags(spawn_attr, POSIX_SPAWN_SETSIGDEF));

            // Attempt to spawn the events source program
            int spawn_rc = posix_spawn(&spawned_pid, EVENTS_PROGRAM, pipe_init_actions, spawn_attr,
                                       spawn_args, environ);
            if (spawn_rc != 0)
            {
                spawned_pid = -1;
                // TODO: log spawn problem
                throw EventsSourceException();
            }

            destroy_spawn_args(spawn_args);

            // Close the local write side of the pipe
            {
                int close_rc = 0;
                do
                {
                    errno = 0;
                    close_rc = close(pipe_fd[PIPE_WRITE_SIDE]);
                }
                while (close_rc != 0 && errno == EINTR);
            }

            // Cleanup pipe_init_actions
            posix_spawn_file_actions_destroy(pipe_init_actions);
            cleanup_pipe_init_actions = false;
            // Cleanup spawn_attr
            posix_spawnattr_destroy(spawn_attr);
            cleanup_spawn_attr = false;
        }

        delete pipe_init_actions;
        delete spawn_attr;
    }
    catch (EventsSourceException&)
    {
        if (cleanup_pipe_init_actions)
        {
            posix_spawn_file_actions_destroy(pipe_init_actions);
        }
        delete pipe_init_actions;

        if (cleanup_spawn_attr)
        {
            posix_spawnattr_destroy(spawn_attr);
        }
        delete spawn_attr;

        destroy_spawn_args(spawn_args);

        close_pipe();

        if (errno == ENOMEM)
        {
            throw std::bad_alloc();
        }
        throw;
    }
    catch (std::bad_alloc&)
    {
        if (cleanup_pipe_init_actions)
        {
            posix_spawn_file_actions_destroy(pipe_init_actions);
        }
        delete pipe_init_actions;

        if (cleanup_spawn_attr)
        {
            posix_spawnattr_destroy(spawn_attr);
        }
        delete spawn_attr;

        destroy_spawn_args(spawn_args);

        close_pipe();

        throw;
    }

    return pipe_fd[PIPE_READ_SIDE];
}

// @throws EventsSourceException
void EventsSourceSpawner::cleanup_child_processes()
{
    cleanup_child_processes_impl();
}

// @throws EventsSourceException
void EventsSourceSpawner::cleanup_child_processes_impl()
{
    // Cleanup any child processes that have exited (zombie processes))
    pid_t child_pid {0};
    do
    {
        int exit_status;
        child_pid = waitpid(-1, &exit_status, WNOHANG);
        if (spawned_pid != -1 && child_pid == spawned_pid &&
            (WIFEXITED(exit_status) || WIFSIGNALED(exit_status)))
        {
            // Events source process exited, throw an EventsSourceException
            // to trigger reinitialization of the application and
            // thereby a respawn of the events source process
            spawned_pid = -1;
            throw EventsSourceException();
        }
    }
    while (child_pid > 0);
}

void EventsSourceSpawner::terminate_child_process() noexcept
{
    // Unless the child process exited already or was not ever spawned,
    // tell the child process to terminate
    if (spawned_pid != -1)
    {
        static_cast<void> (kill(spawned_pid, SIGTERM));
    }
}

void EventsSourceSpawner::close_pipe()
{
    // Close the pipe file descriptors
    if (pipe_fd[PIPE_READ_SIDE] != -1)
    {
        int rc = 0;
        do
        {
            errno = 0;
            rc = close(pipe_fd[PIPE_READ_SIDE]);
        }
        while (rc != 0 && errno == EINTR);
        pipe_fd[PIPE_READ_SIDE] = -1;
    }
    if (pipe_fd[PIPE_WRITE_SIDE] != -1)
    {
        int rc = 0;
        do
        {
            errno = 0;
            rc = close(pipe_fd[PIPE_WRITE_SIDE]);
        }
        while (rc != 0 && errno == EINTR);
        pipe_fd[PIPE_WRITE_SIDE] = -1;
    }
}

// Throws EventsSourceException if rc is not equal to 0
// @throws EventsSourceException
void EventsSourceSpawner::checked_int_rc(int rc) const
{
    if (rc != 0)
    {
        throw EventsSourceException();
    }
}

// @throws std::bad_alloc
char** EventsSourceSpawner::init_spawn_args() const
{
    char** spawn_args = nullptr;

    try
    {
        // Number of arguments, excluding the trailing null pointer
        size_t args_count = 0;
        while (EVENTS_PROGRAM_ARGS[args_count] != nullptr)
        {
            ++args_count;
        }

        // Allocate slots for the arguments and an additional one for
        // the trailing null pointer
        spawn_args = new char*[args_count + 1];
        for (size_t index = 0; index < args_count; ++index)
        {
            size_t arg_length = std::strlen(EVENTS_PROGRAM_ARGS[index]) + 1;
            // If the allocation fails, make sure there is a null pointer in this slot
            // Required for clean deallocation
            spawn_args[index] = nullptr;
            spawn_args[index] = new char[arg_length];
            std::strcpy(spawn_args[index], EVENTS_PROGRAM_ARGS[index]);
        }
        spawn_args[args_count] = nullptr;
    }
    catch (std::bad_alloc&)
    {
        destroy_spawn_args(spawn_args);
        throw;
    }

    return spawn_args;
}

void EventsSourceSpawner::destroy_spawn_args(char** spawn_args) const noexcept
{
    if (spawn_args != nullptr)
    {
        for (size_t index = 0; spawn_args[index] != nullptr; ++index)
        {
            delete[] spawn_args[index];
        }
        delete[] spawn_args;
    }
}
