#include <string>
#include <cstring>
#include <algorithm>
#include <stdexcept>
#include <subprocess/Linux/SubProcessLx.h>

extern "C"
{
    #include <unistd.h>
    #include <spawn.h>
    #include <signal.h>
    #include <fcntl.h>
    #include <sys/wait.h>
    #include <sys/epoll.h>
    #include <errno.h>
}

extern char** environ;

const size_t    SubProcessLx::OBSERVED_EVENTS_COUNT     = 3;
const size_t    SubProcessLx::FIRED_EVENTS_COUNT        = 3;

// Buffer capacity increment steps. Increment steps must be larger than the buffer size for efficient operation.
// 16 kiB, 64 kiB
const size_t    SubProcessLx::BUFFER_CAP[]              = {1UL << 14, 1UL << 16};
const size_t    SubProcessLx::BUFFER_CAP_SIZE           = sizeof (SubProcessLx::BUFFER_CAP) / sizeof (size_t);

SubProcessLx::SubProcessLx():
    SubProcess::SubProcess()
{
    subproc_stdout_pipe[PIPE_READ]  = -1;
    subproc_stdout_pipe[PIPE_WRITE] = -1;
    subproc_stderr_pipe[PIPE_READ]  = -1;
    subproc_stderr_pipe[PIPE_WRITE] = -1;
    wakeup_pipe[PIPE_READ]          = -1;
    wakeup_pipe[PIPE_WRITE]         = -1;
}

SubProcessLx::~SubProcessLx() noexcept
{
    close_fd(poll_fd);
    close_fd(subproc_stdout_pipe[PIPE_READ]);
    close_fd(subproc_stdout_pipe[PIPE_WRITE]);
    close_fd(subproc_stderr_pipe[PIPE_READ]);
    close_fd(subproc_stderr_pipe[PIPE_WRITE]);
    close_fd(wakeup_pipe[PIPE_READ]);
    close_fd(wakeup_pipe[PIPE_WRITE]);
}

uint64_t SubProcessLx::get_pid() const noexcept
{
    proc_lock.lock();
    const uint64_t generic_pid = static_cast<uint64_t> (subproc_id);
    proc_lock.unlock();
    return generic_pid;
}

// @throws SubProcess::Exception
void SubProcessLx::execute(const CmdLine& cmd)
{
    std::unique_ptr<SubProcessLx::SysExecArgs> sys_cmd_line(new SubProcessLx::SysExecArgs(cmd));

    std::unique_ptr<posix_spawn_file_actions_t> file_actions_mgr(new posix_spawn_file_actions_t);
    std::unique_ptr<posix_spawnattr_t> spawn_attr_mgr(new posix_spawnattr_t);

    posix_spawn_file_actions_t* const file_actions = file_actions_mgr.get();
    posix_spawnattr_t* const spawn_attr = spawn_attr_mgr.get();

    std::exception_ptr stored_exc;

    bool have_file_actions = false;
    bool have_spawn_attr = false;
    try
    {
        throw_if_nonzero(
            pipe(subproc_stdout_pipe),
            "Creation of the stdout pipe failed"
        );

        throw_if_nonzero(
            pipe(subproc_stderr_pipe),
            "Creation of the stderr pipe failed"
        );

        throw_if_nonzero(
            pipe(wakeup_pipe),
            "Creation of the wakeup pipe failed"
        );

        {
            const int read_io_flags = fcntl(subproc_stdout_pipe[PIPE_READ], F_GETFL, 0);
            fcntl(subproc_stdout_pipe[PIPE_READ], F_SETFL, read_io_flags | O_NONBLOCK);
        }
        {
            const int read_io_flags = fcntl(subproc_stderr_pipe[PIPE_READ], F_GETFL, 0);
            fcntl(subproc_stderr_pipe[PIPE_READ], F_SETFL, read_io_flags | O_NONBLOCK);
        }
        {
            const int read_io_flags = fcntl(wakeup_pipe[PIPE_READ], F_GETFL, 0);
            fcntl(wakeup_pipe[PIPE_READ], F_SETFL, read_io_flags | O_NONBLOCK);
            const int write_io_flags = fcntl(wakeup_pipe[PIPE_WRITE], F_GETFL, 0);
            fcntl(wakeup_pipe[PIPE_WRITE], F_SETFL, write_io_flags | O_NONBLOCK);
        }

        throw_if_nonzero(
            posix_spawn_file_actions_init(file_actions),
            "Initialization of the file_actions data structure failed"
        );
        have_file_actions = true;

        throw_if_nonzero(
            posix_spawnattr_init(spawn_attr),
            "Initialization of the spawn_attr data structure failed"
        );
        have_spawn_attr = true;

        throw_if_nonzero(
            posix_spawn_file_actions_adddup2(
                file_actions,
                subproc_stdout_pipe[PIPE_WRITE],
                STDOUT_FILENO
            ),
            "Connecting the stdout pipe failed"
        );
        throw_if_nonzero(
            posix_spawn_file_actions_adddup2(
                file_actions,
                subproc_stderr_pipe[PIPE_WRITE],
                STDERR_FILENO
            ),
            "Connecting the stderr pipe failed"
        );
        throw_if_nonzero(
            posix_spawn_file_actions_addclose(
                file_actions,
                subproc_stdout_pipe[PIPE_READ]
            ),
            "Closing the stdout pipe read end failed"
        );
        throw_if_nonzero(
            posix_spawn_file_actions_addclose(
                file_actions,
                subproc_stderr_pipe[PIPE_READ]
            ),
            "Closing the stderr pipe read end failed"
        );
        throw_if_nonzero(
            posix_spawn_file_actions_addclose(
                file_actions,
                wakeup_pipe[PIPE_READ]
            ),
            "Closing the wakeup pipe read end failed"
        );
        throw_if_nonzero(
            posix_spawn_file_actions_addclose(
                file_actions,
                wakeup_pipe[PIPE_WRITE]
            ),
            "Closing the wakeup pipe write end failed"
        );

        char** exec_args = sys_cmd_line->get_exec_args();
        int spawn_rc = 1;
        proc_lock.lock();
        if (enable_spawn)
        {
            spawn_rc = posix_spawn(&subproc_id, exec_args[0], file_actions, spawn_attr, exec_args, environ);
        }
        proc_lock.unlock();
        if (spawn_rc == 0)
        {
            close_fd(subproc_stdout_pipe[PIPE_WRITE]);
            close_fd(subproc_stderr_pipe[PIPE_WRITE]);

            read_subproc_output();

            int local_exit_status = -1;
            int wait_result = -1;
            do
            {
                errno = 0;
                wait_result = waitpid(subproc_id, &local_exit_status, 0);
                if (wait_result > 0)
                {
                    const int subproc_exit_status = WEXITSTATUS(local_exit_status);
                    exit_status.store(subproc_exit_status);
                }
            }
            while ((wait_result == -1 && errno == EINTR) ||
                   (wait_result >= 0 && WIFEXITED(local_exit_status) == 0 && WIFSIGNALED(local_exit_status) == 0));
            // The Linux docs say these macros "return true" if their defined condition is met,
            // but that seems to be inaccurate.
            // The Open Group Base Specification (IEEE Std 1003.1-2017) says they "return non-zero".
            if (wait_result <= 0)
            {
                exit_status.store(EXIT_STATUS_FAILED);
            }

            close_fd(subproc_stdout_pipe[PIPE_READ]);
            close_fd(subproc_stderr_pipe[PIPE_READ]);
            close_fd(wakeup_pipe[PIPE_READ]);
            close_fd(wakeup_pipe[PIPE_WRITE]);
        }
    }
    catch (std::exception& exc)
    {
        stored_exc = std::current_exception();
    }

    if (have_file_actions)
    {
        posix_spawn_file_actions_destroy(file_actions);
    }
    if (have_spawn_attr)
    {
        posix_spawnattr_destroy(spawn_attr);
    }

    if (stored_exc != nullptr)
    {
        std::rethrow_exception(stored_exc);
    }
}

void SubProcessLx::terminate(const bool force)
{
    // Load/store of exit status and process ID races with the thread that executes the external process.
    const int local_exit_status = exit_status.load();
    proc_lock.lock();
    const pid_t local_pid = subproc_id;
    enable_spawn = false;
    proc_lock.unlock();

    if (local_pid != -1 && local_exit_status == SubProcess::EXIT_STATUS_NONE)
    {
        kill(local_pid, (force ? SIGKILL : SIGTERM));
        const char wakeup_char = 0;
        ssize_t write_count = 0;
        do
        {
            errno = 0;
            write_count = write(wakeup_pipe[PIPE_WRITE], &wakeup_char, 1);
        }
        while (write_count == -1 && errno == EINTR);
    }
}

// @throws std::bad_alloc
void SubProcessLx::read_subproc_output()
{
    std::unique_ptr<char[]> read_buffer(new char[READ_BUFFER_SIZE]);

    std::unique_ptr<struct epoll_event[]>   observed_events(new struct epoll_event[OBSERVED_EVENTS_COUNT]);
    std::unique_ptr<struct epoll_event[]>   fired_events(new struct epoll_event[FIRED_EVENTS_COUNT]);

    struct epoll_event* const observed_events_ptr   = observed_events.get();
    struct epoll_event* const fired_events_ptr      = fired_events.get();

    char* const     read_buffer_ptr     = read_buffer.get();

    poll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (poll_fd == -1)
    {
        throw SubProcessLx::Exception("I/O channel polling initialization failed");
    }

    try
    {
        observed_events_ptr[0].data.fd = subproc_stdout_pipe[PIPE_READ];
        observed_events_ptr[0].events = EPOLLIN;
        throw_if_nonzero(
            epoll_ctl(poll_fd, EPOLL_CTL_ADD, subproc_stdout_pipe[PIPE_READ], &(observed_events_ptr[0])),
            "I/O event setup for the stdout pipe failed"
        );
        observed_events_ptr[1].data.fd = subproc_stderr_pipe[PIPE_READ];
        observed_events_ptr[1].events = EPOLLIN;
        throw_if_nonzero(
            epoll_ctl(poll_fd, EPOLL_CTL_ADD, subproc_stderr_pipe[PIPE_READ], &(observed_events_ptr[1])),
            "I/O event setup for the stderr pipe failed"
        );
        observed_events_ptr[2].data.fd = wakeup_pipe[PIPE_READ];
        observed_events_ptr[2].events = EPOLLIN;
        throw_if_nonzero(
            epoll_ctl(poll_fd, EPOLL_CTL_ADD, wakeup_pipe[PIPE_READ], &(observed_events_ptr[2])),
            "I/O event setup for the wakeup pipe failed"
        );
    }
    catch (SubProcessLx::Exception&)
    {
        close_fd(poll_fd);
        throw;
    }

    bool poll_fds = false;
    int event_count = 0;
    int epoll_errno = 0;
    do
    {
        const bool have_stdout = subproc_stdout_pipe[PIPE_READ] != -1;
        const bool have_stderr = subproc_stderr_pipe[PIPE_READ] != -1;
        poll_fds = have_stdout || have_stderr;

        epoll_errno = 0;
        if (poll_fds)
        {
            errno = 0;
            event_count = epoll_wait(poll_fd, fired_events_ptr, FIRED_EVENTS_COUNT, -1);
            epoll_errno = errno;
            if (event_count > -1)
            {
                for (size_t idx = 0; idx < static_cast<size_t> (event_count); ++idx)
                {
                    if (fired_events_ptr[idx].data.fd == subproc_stdout_pipe[PIPE_READ])
                    {
                        if ((fired_events_ptr[idx].events & (EPOLLERR | EPOLLHUP)) == 0)
                        {
                            read_subproc_fd(subproc_stdout_pipe[PIPE_READ], read_buffer_ptr,
                                            subproc_out, out_buffer_cap_idx, SUBPROC_OUT_MAX_SIZE);
                        }
                        else
                        {
                            epoll_ctl(poll_fd, EPOLL_CTL_DEL, subproc_stdout_pipe[PIPE_READ], nullptr);
                            close_fd(subproc_stdout_pipe[PIPE_READ]);
                        }
                    }
                    else
                    if (fired_events_ptr[idx].data.fd == subproc_stderr_pipe[PIPE_READ])
                    {
                        if ((fired_events_ptr[idx].events & (EPOLLERR | EPOLLHUP)) == 0)
                        {
                            read_subproc_fd(subproc_stderr_pipe[PIPE_READ], read_buffer_ptr,
                                            subproc_err, err_buffer_cap_idx, SUBPROC_ERR_MAX_SIZE);
                        }
                        else
                        {
                            epoll_ctl(poll_fd, EPOLL_CTL_DEL, subproc_stderr_pipe[PIPE_READ], nullptr);
                            close_fd(subproc_stderr_pipe[PIPE_READ]);
                        }
                    }
                    else
                    if (fired_events_ptr[idx].data.fd == wakeup_pipe[PIPE_READ])
                    {
                        if ((fired_events_ptr[idx].events & (EPOLLERR | EPOLLHUP)) == 0)
                        {
                            ssize_t read_count = 0;
                            do
                            {
                                errno = 0;
                                read_count = read(wakeup_pipe[PIPE_READ], read_buffer_ptr, READ_BUFFER_SIZE);
                            }
                            while (read_count >= 1 || (read_count == -1 && errno == EINTR));
                        }

                        epoll_ctl(poll_fd, EPOLL_CTL_DEL, wakeup_pipe[PIPE_READ], nullptr);
                        poll_fds = false;
                    }
                }
            }
        }
    }
    while (poll_fds && (event_count != -1 || epoll_errno == EINTR));

    close_fd(poll_fd);
}

// @throws std::bad_alloc
void SubProcessLx::read_subproc_fd(
    int&            subproc_fd,
    char* const     read_buffer_ptr,
    std::string&    dst_buffer,
    size_t&         dst_buffer_cap_idx,
    const size_t    max_size
)
{
    errno = 0;
    const ssize_t read_count = read(subproc_fd, read_buffer_ptr, READ_BUFFER_SIZE);
    if (read_count >= 1)
    {
        const size_t current_length = dst_buffer.length();
        if (current_length < max_size)
        {
            const size_t copy_length = std::min(
                static_cast<ssize_t> (max_size - current_length), read_count
            );
            const size_t result_length = current_length + copy_length;
            if (result_length > dst_buffer.capacity())
            {
                if (dst_buffer_cap_idx < BUFFER_CAP_SIZE)
                {
                    dst_buffer.reserve(std::min(BUFFER_CAP[dst_buffer_cap_idx], max_size));
                    ++dst_buffer_cap_idx;
                }
                else
                {
                    dst_buffer.reserve(max_size);
                }
            }
            dst_buffer.append(read_buffer_ptr, copy_length);
        }
    }
    else
    if (read_count == 0 || errno != EINTR)
    {
        epoll_ctl(poll_fd, EPOLL_CTL_DEL, subproc_fd, nullptr);
        close_fd(subproc_fd);
    }
}

// @throws SubProcess::Exception
void SubProcessLx::throw_if_nonzero(const int rc, const char* const error_msg)
{
    if (rc != 0)
    {
        throw SubProcessLx::Exception(error_msg);
    }
}

void SubProcessLx::close_fd(int& fd) noexcept
{
    if (fd != -1)
    {
        int rc = 0;
        do
        {
            errno = 0;
            rc = close(fd);
        }
        while (rc != 0 && errno == EINTR);
        fd = -1;
    }
}

SubProcessLx::SysExecArgs::SysExecArgs(const CmdLine& cmd):
    arg_count(cmd.get_argument_count())
{
    try
    {
        arg_array_mgr = std::unique_ptr<char*[]>(new char*[arg_count + 1]);
        char** arg_array = arg_array_mgr.get();
        for (size_t arg_idx = 0; arg_idx <= arg_count; ++arg_idx)
        {
            arg_array[arg_idx] = nullptr;
        }

        CmdLine::StringList::ValuesIterator arg_iter(cmd.get_argument_iterator());
        for (size_t arg_idx = 0; arg_iter.has_next() && arg_idx < arg_count; ++arg_idx)
        {
            const std::string* const cur_arg = arg_iter.next();
            if (cur_arg != nullptr)
            {
                const size_t cur_arg_length = cur_arg->length();
                arg_array[arg_idx] = new char[cur_arg_length + 1];
                std::memcpy(arg_array[arg_idx], cur_arg->c_str(), cur_arg_length);
                arg_array[arg_idx][cur_arg_length] = '\0';
            }
        }
    }
    catch (std::bad_alloc&)
    {
        cleanup();
        throw;
    }
}

SubProcessLx::SysExecArgs::~SysExecArgs() noexcept
{
    cleanup();
}

char** SubProcessLx::SysExecArgs::get_exec_args() noexcept
{
    return arg_array_mgr.get();
}

void SubProcessLx::SysExecArgs::cleanup() noexcept
{
    for (size_t arg_idx = 0; arg_idx < arg_count; ++arg_idx)
    {
        char* cur_arg = arg_array_mgr[arg_idx];
        if (cur_arg != nullptr)
        {
            delete[] cur_arg;
            cur_arg = nullptr;
        }
    }
}
