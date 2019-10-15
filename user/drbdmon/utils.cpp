#include <utils.h>
#include <exceptions.h>
#include <cstring>

extern "C"
{
    #include <unistd.h>
    #include <spawn.h>
}

TermSize::TermSize()
{
    term_size.ws_col    = 0;
    term_size.ws_row    = 0;
    term_size.ws_xpixel = 0;
    term_size.ws_ypixel = 0;
}

bool TermSize::probe_terminal_size()
{
    valid = (ioctl(1, TIOCGWINSZ, &term_size) == 0);
    return valid;
}

bool TermSize::is_valid() const
{
    return valid;
}

uint16_t TermSize::get_size_x() const
{
    return static_cast<uint16_t> (term_size.ws_col);
}

uint16_t TermSize::get_size_y() const
{
    return static_cast<uint16_t> (term_size.ws_row);
}

namespace posix
{
    const size_t PIPE_READ_SIDE   = 0;
    const size_t PIPE_WRITE_SIDE  = 1;

    void close_fd(int& fd) noexcept
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

    // @throws std::bad_alloc, EventsSourceException
    SpawnFileActions::SpawnFileActions()
    {
        actions_mgr = std::unique_ptr<posix_spawn_file_actions_t>(new posix_spawn_file_actions_t);
        actions = actions_mgr.get();
        if (posix_spawn_file_actions_init(actions) != 0)
        {
            if (errno == ENOMEM)
            {
                throw std::bad_alloc();
            }
            throw EventsSourceException();
        }
    }

    SpawnFileActions::~SpawnFileActions() noexcept
    {
        posix_spawn_file_actions_destroy(actions);
        actions = nullptr;
    }

    // @throws std::bad_alloc, EventsSourceException
    SpawnAttr::SpawnAttr()
    {
        attr_mgr = std::unique_ptr<posix_spawnattr_t>(new posix_spawnattr_t);
        attr = attr_mgr.get();
        if (posix_spawnattr_init(attr) != 0)
        {
            if (errno == ENOMEM)
            {
                throw std::bad_alloc();
            }
            throw EventsSourceException();
        }
    }

    SpawnAttr::~SpawnAttr() noexcept
    {
        posix_spawnattr_destroy(attr);
        attr = nullptr;
    }

    // @throws std::bad_alloc
    SpawnArgs::SpawnArgs(const char* const arg_list[])
    {
        while (arg_list[args_size] != nullptr)
        {
            ++args_size;
        }
        ++args_size;
        try
        {
            args = new char*[args_size];
            for (size_t idx = 0; idx < args_size - 1; ++idx)
            {
                // If the allocation fails, make sure there is a null pointer in this slot
                args[idx] = nullptr;
                size_t arg_length = std::strlen(arg_list[idx]) + 1;
                args[idx] = new char[arg_length];
                std::memcpy(args[idx], arg_list[idx], arg_length);
            }
            args[args_size - 1] = nullptr;
        }
        catch (std::bad_alloc&)
        {
            if (args != nullptr)
            {
                destroy();
            }
            throw;
        }
    }

    SpawnArgs::~SpawnArgs() noexcept
    {
        destroy();
    }

    void SpawnArgs::destroy() noexcept
    {
        for (size_t idx = 0; idx < args_size && args[idx] != nullptr; ++idx)
        {
            delete[] args[idx];
            args[idx] = nullptr;
        }
        delete[] args;
        args = nullptr;
    }

    // @throws EventsSourceException
    Pipe::Pipe(int (*pipe_fd)[2])
    {
        managed_pipe = pipe_fd;
        if (pipe2(*pipe_fd, 0) != 0)
        {
            throw EventsSourceException();
        }
    }

    Pipe::~Pipe() noexcept
    {
        if (managed_pipe != nullptr)
        {
            posix::close_fd((*managed_pipe)[PIPE_READ_SIDE]);
            posix::close_fd((*managed_pipe)[PIPE_WRITE_SIDE]);
        }
    }

    void Pipe::release() noexcept
    {
        managed_pipe = nullptr;
    }

    void Pipe::close_read_side() noexcept
    {
        if (managed_pipe != nullptr)
        {
            posix::close_fd((*managed_pipe)[PIPE_READ_SIDE]);
        }
    }

    void Pipe::close_write_side() noexcept
    {
        if (managed_pipe != nullptr)
        {
            posix::close_fd((*managed_pipe)[PIPE_WRITE_SIDE]);
        }
    }
}
