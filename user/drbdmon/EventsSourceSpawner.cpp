#include <EventsSourceSpawner.h>
#include <memory>
#include <cstring>
#include <utils.h>

extern "C"
{
    #include <unistd.h>
    #include <signal.h>
    #include <spawn.h>
    #include <fcntl.h>
    #include <sys/wait.h>
    #include <errno.h>
}

const char* EventsSourceSpawner::EVENTS_PROGRAM = "drbdsetup";
const char* EventsSourceSpawner::EVENTS_PROGRAM_ARGS[] =
{
    "drbdsetup",
    "events2",
    "all",
    nullptr
};

EventsSourceSpawner::EventsSourceSpawner(MessageLog& logRef):
    log(logRef)
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

    int io_flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, io_flags & ~O_NONBLOCK);
}

pid_t EventsSourceSpawner::get_process_id()
{
    return spawned_pid;
}

int EventsSourceSpawner::get_events_out_fd()
{
    return out_pipe_fd[posix::PIPE_READ_SIDE];
}

int EventsSourceSpawner::get_events_err_fd()
{
    return err_pipe_fd[posix::PIPE_READ_SIDE];
}

// @throws std::bad_alloc, EventSourceException
void EventsSourceSpawner::spawn_source()
{
    // Initialize the pipes
    posix::Pipe out_pipe_mgr(&out_pipe_fd);
    posix::Pipe err_pipe_mgr(&err_pipe_fd);

    // Make the pipes' read end nonblocking
    set_nonblocking(out_pipe_fd[posix::PIPE_READ_SIDE]);
    set_nonblocking(err_pipe_fd[posix::PIPE_READ_SIDE]);

    // Make stdin nonblocking
    set_nonblocking(STDIN_FILENO);

    // Initialize the datastructures for posix_spawn())
    posix::SpawnFileActions pipe_init;
    posix::SpawnAttr spawn_attr;
    posix::SpawnArgs spawn_args(EVENTS_PROGRAM_ARGS);

    // Redirect stdout to the pipes write side
    checked_int_rc(
        posix_spawn_file_actions_adddup2(
            pipe_init.actions,
            out_pipe_fd[posix::PIPE_WRITE_SIDE],
            STDOUT_FILENO
        )
    );
    checked_int_rc(
        posix_spawn_file_actions_adddup2(
            pipe_init.actions,
            err_pipe_fd[posix::PIPE_WRITE_SIDE],
            STDERR_FILENO
        )
    );
    // Close the read end of the pipes
    checked_int_rc(
        posix_spawn_file_actions_addclose(
            pipe_init.actions,
            out_pipe_fd[posix::PIPE_READ_SIDE]
        )
    );
    checked_int_rc(
        posix_spawn_file_actions_addclose(
            pipe_init.actions,
            err_pipe_fd[posix::PIPE_READ_SIDE]
        )
    );

    // Reset ignored signals to the default action
    sigset_t mask;
    checked_int_rc(sigemptyset(&mask));
    checked_int_rc(sigaddset(&mask, SIGTERM));
    checked_int_rc(sigaddset(&mask, SIGHUP));
    checked_int_rc(sigaddset(&mask, SIGINT));
    checked_int_rc(sigaddset(&mask, SIGWINCH));
    checked_int_rc(sigaddset(&mask, SIGCHLD));
    checked_int_rc(posix_spawnattr_setsigdefault(spawn_attr.attr, &mask));
    checked_int_rc(posix_spawnattr_setflags(spawn_attr.attr, POSIX_SPAWN_SETSIGDEF));

    // Attempt to spawn the events source program
    int spawn_rc = posix_spawnp(&spawned_pid, EVENTS_PROGRAM, pipe_init.actions, spawn_attr.attr,
                                spawn_args.args, environ);
    if (spawn_rc != 0)
    {
        spawned_pid = -1;
        log.add_entry(
            MessageLog::log_level::ALERT,
            "Spawning the events source process failed"
        );
        throw EventsSourceException();
    }

    // Close the local write side of the pipes
    out_pipe_mgr.close_write_side();
    err_pipe_mgr.close_write_side();

    // Keep the pipes open when leaving scope
    out_pipe_mgr.release();
    err_pipe_mgr.release();
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

// Sets O_NONBLOCK on the specified filedescriptor
void EventsSourceSpawner::set_nonblocking(int fd)
{
    int io_flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, io_flags | O_NONBLOCK);
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
