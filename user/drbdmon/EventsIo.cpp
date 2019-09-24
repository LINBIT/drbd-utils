#include <EventsIo.h>
#include <utils.h>
#include <cstring>

extern "C"
{
    #include <errno.h>
}

// epoll_event datastructure slot indices
const int EventsIo::EVENTS_CTL_INDEX = 0;
const int EventsIo::ERROR_CTL_INDEX  = 1;
const int EventsIo::SIG_CTL_INDEX    = 2;
const int EventsIo::STDIN_CTL_INDEX  = 3;
// Number of epoll_event datastructure slots
const int EventsIo::CTL_SLOTS_COUNT  = 4;

// @throws std::bad_alloc, std::ios_base::failure
EventsIo::EventsIo(const int events_input_fd, const int events_error_fd):
    events_fd(events_input_fd),
    error_fd(events_error_fd),
    ctl_events(new struct epoll_event[CTL_SLOTS_COUNT]),
    fired_events(new struct epoll_event[CTL_SLOTS_COUNT]),
    signal_buffer(new struct signalfd_siginfo),
    events_buffer(new char[MAX_LINE_LENGTH]),
    error_buffer(new char[ERROR_BUFFER_SIZE])
{
    try
    {
        // Zero termios datastructures
        static_cast<void> (std::memset(static_cast<void*> (&orig_termios), 0,
                                       sizeof (orig_termios)));
        static_cast<void> (std::memset(static_cast<void*> (&adjusted_termios), 0,
                                       sizeof (adjusted_termios)));

        // Initialize the poll file descriptor
        poll_fd = epoll_create1(EPOLL_CLOEXEC);
        if (poll_fd == -1)
        {
            std::string error_msg("I/O channel selector initialization failed");
            std::string debug_info("epoll_create1(...) failed, errno=");
            debug_info += std::to_string(errno);
            throw EventsIoException(&error_msg, &debug_info, nullptr);
        }

        // Initialize signalfds to enable polling for signals
        // Block the default signal handlers for the same signals
        sigset_t mask;
        checked_int_rc(sigemptyset(&mask));
        checked_int_rc(sigaddset(&mask, SIGTERM));
        checked_int_rc(sigaddset(&mask, SIGHUP));
        checked_int_rc(sigaddset(&mask, SIGINT));
        checked_int_rc(sigaddset(&mask, SIGUSR1));
        checked_int_rc(sigaddset(&mask, SIGALRM));
        checked_int_rc(sigaddset(&mask, SIGWINCH));
        checked_int_rc(sigaddset(&mask, SIGCHLD));
        checked_int_rc(sigprocmask(SIG_BLOCK, &mask, nullptr));
        sig_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
        if (sig_fd == -1)
        {
            std::string error_msg("Signal handler initialization failed");
            std::string debug_info("signalfd(...) failed, errno=");
            debug_info += std::to_string(errno);
            throw EventsIoException(&error_msg, &debug_info, nullptr);
        }

        // Initialize poll event data structures
        static_cast<void> (std::memset(static_cast<void*> (ctl_events.get()), 0,
                                       sizeof (struct epoll_event) * CTL_SLOTS_COUNT));
        static_cast<void> (std::memset(static_cast<void*> (fired_events.get()), 0,
                                       sizeof (struct epoll_event) * CTL_SLOTS_COUNT));

        // Register the epoll() events
        register_poll(events_fd, &(ctl_events[EVENTS_CTL_INDEX]), EPOLLIN);
        register_poll(error_fd, &(ctl_events[ERROR_CTL_INDEX]), EPOLLIN);
        register_poll(sig_fd, &(ctl_events[SIG_CTL_INDEX]), EPOLLIN);
        register_poll(stdin_fd, &(ctl_events[STDIN_CTL_INDEX]), EPOLLIN);
    }
    catch (EventsIoException&)
    {
        // Cleanup modifies errno, so the relevant information must be cached
        bool os_call_oom = (errno == ENOMEM);
        cleanup();

        if (os_call_oom)
        {
            throw std::bad_alloc();
        }
        throw;
    }
    catch (std::bad_alloc&)
    {
        cleanup();

        throw;
    }
}

EventsIo::~EventsIo() noexcept
{
    cleanup();
}

// @throws std::bad_alloc, EventsIoException
void EventsIo::register_poll(int fd, struct epoll_event* event_ctl_slot, uint32_t event_mask)
{
    event_ctl_slot->data.fd = fd;
    event_ctl_slot->events = event_mask;
    int rc = epoll_ctl(poll_fd, EPOLL_CTL_ADD, fd, event_ctl_slot);
    if (rc != 0)
    {
        std::string error_msg("I/O channel registration failed");
        std::string debug_info("epoll_ctl(...) failed, errno=");
        debug_info += std::to_string(errno);
        throw EventsIoException(&error_msg, &debug_info, nullptr);
    }
}

// @throws std::bad_alloc, EventsIoException
EventsIo::event EventsIo::wait_event()
{
    EventsIo::event event_id = EventsIo::event::NONE;

    // If data is available in the buffer, look for event lines
    // This check is turned off by prepare_line() if all the available
    // data has been searched and more data must be read to find another
    // event lines
    // read_events() turns this check back on if more data was read
    if (data_pending)
    {
        if (prepare_line())
        {
            event_id = EventsIo::event::EVENT_LINE;
        }
    }

    while (event_id == EventsIo::event::NONE)
    {
        if (!pending_events)
        {
            current_event = 0;
            do
            {
                errno = 0;
                event_count = epoll_wait(poll_fd, fired_events.get(), CTL_SLOTS_COUNT, -1);
                if (event_count > 0)
                {
                    pending_events = true;
                }
                else
                if (errno != 0 && errno != EINTR)
                {
                    std::string error_msg("I/O channel selection failed");
                    std::string debug_info("epoll_wait(...) failed, errno=");
                    debug_info += std::to_string(errno);
                    throw EventsIoException(&error_msg, &debug_info, nullptr);
                }
            }
            while (!pending_events);
        }

        int fired_fd = fired_events[current_event].data.fd;
        if (fired_fd == events_fd)
        {
            // Events source data available
            if (events_eof)
            {
                std::string error_msg("The events stream was closed");
                throw EventsIoException(&error_msg, nullptr, nullptr);
            }

            read_events();
            if (prepare_line())
            {
                event_id = EventsIo::event::EVENT_LINE;
            }
        }
        else
        if (fired_fd == error_fd)
        {
            if (errors_eof)
            {
                std::string error_msg("The events source error stream was closed");
                throw EventsIoException(&error_msg, nullptr, nullptr);
            }

            read_errors();
            std::string error_msg("The events source wrote to the stderr channel");
            throw EventsIoException(&error_msg, nullptr, nullptr);
        }
        else
        if (fired_fd == sig_fd)
        {
            // Signal available
            event_id = EventsIo::event::SIGNAL;
        }
        else
        if (fired_fd == stdin_fd)
        {
            // Data available on stdin
            event_id = EventsIo::event::STDIN;
        }
        else
        {
            // Unknown data source ready, this is not supposed to happen
            std::string error_msg("Internal error: An unregistered I/O channel became ready");
            throw EventsIoException(&error_msg, nullptr, nullptr);
        }

        // Select the next event for processing
        ++current_event;
        if (current_event >= event_count)
        {
            pending_events = false;
        }
    }

    return event_id;
}

// @throws std::bad_alloc, EventsIoException
int EventsIo::get_signal()
{
    int signal_id = 0;

    errno = 0;
    ssize_t read_size = 0;
    do
    {
        read_size = read(sig_fd, signal_buffer.get(), sizeof (struct signalfd_siginfo));
        if (read_size == sizeof (struct signalfd_siginfo))
        {
            signal_id = static_cast<int> (signal_buffer->ssi_signo);
        }
        else
        if (read_size == 0)
        {
            std::string error_msg("The signal handling channel was closed");
            throw EventsIoException(&error_msg, nullptr, nullptr);
        }
    }
    while (read_size == -1 && errno == EINTR);

    if (read_size == -1)
    {
        std::string error_msg("Reading from the signal handling channel failed");
        std::string debug_info("read(sig_fd, ...) failed, errno=");
        debug_info += std::to_string(errno);
        throw EventsIoException(&error_msg, &debug_info, nullptr);
    }

    return signal_id;
}

// @throws EventsIoException
void EventsIo::adjust_terminal()
{
    // Store current terminal settings
    checked_int_rc(tcgetattr(STDIN_FILENO, &orig_termios));
    have_orig_termios = true;
    adjusted_termios = orig_termios;

    // Non-canonical input without echo
    tcflag_t flags_on = ISIG;
    tcflag_t flags_off = ICANON | ECHO | ECHONL;
    adjusted_termios.c_lflag |= flags_on;
    adjusted_termios.c_lflag = (adjusted_termios.c_lflag | flags_off) ^ flags_off;
    checked_int_rc(tcsetattr(STDIN_FILENO, TCSANOW, &adjusted_termios));
}

// @throws EventsIoException
void EventsIo::restore_terminal()
{
    if (have_orig_termios)
    {
        // Restore original terminal settings
        checked_int_rc(tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios));
    }
}

// @throws std::bad_alloc, EventsIoException
void EventsIo::read_events()
{
    // If there is space remaining in the buffer,
    // read data into the buffer.
    // If the buffer is full, it must be compacted or emptied by
    // calls to get_event_line()
    if (events_length < MAX_LINE_LENGTH)
    {
        size_t length = MAX_LINE_LENGTH - events_length;
        int read_count = 0;
        do
        {
            read_count = read(events_fd, &(events_buffer[events_length]), length);
        }
        while (read_count == -1 && errno == EINTR);
        if (read_count == 0)
        {
            events_eof = true;
        }
        else
        if (read_count == -1)
        {
            if (errno != EAGAIN)
            {
                std::string error_msg("Reading from the events channel failed");
                std::string debug_info("read(events_fd, ...) failed, errno=");
                debug_info += std::to_string(errno);
                throw EventsIoException(&error_msg, &debug_info, nullptr);
            }
        }
        else
        {
            // Indicate that more data has been made available and
            // should be checked for complete lines using prepare_line()
            data_pending = true;
            events_length += read_count;
        }
    }
}

// @throws std::bad_alloc, EventsIoException
void EventsIo::read_errors()
{
    ssize_t read_count = 0;
    do
    {
        read_count = read(error_fd, &(error_buffer[0]), ERROR_BUFFER_SIZE);
    }
    while (read_count == -1 && errno == EINTR);
    if (read_count == 0)
    {
        errors_eof = true;
    }
    else
    if (read_count == -1)
    {
        if (errno != EAGAIN)
        {
            std::string error_msg("Reading from the error channel failed");
            std::string debug_info("read(error_fd, ...) failed, errno=");
            debug_info += std::to_string(errno);
            throw EventsIoException(&error_msg, &debug_info, nullptr);
        }
    }
}

// @throws std::bad_alloc, EventsIoException
std::string* EventsIo::get_event_line()
{
    if (event_line == nullptr)
    {
        if (!data_pending)
        {
            read_events();
        }
        prepare_line();
    }

    line_pending = false;
    return event_line.get();
}

void EventsIo::free_event_line()
{
    event_line = nullptr;
}

// @throws std::bad_alloc, EventsIoException
bool EventsIo::prepare_line()
{
    if (!line_pending)
    {
        if (event_line != nullptr)
        {
            event_line = nullptr;
        }

        bool have_line = false;
        size_t event_end_pos = 0;
        // Check for new complete lines in the buffer
        char* input_data = events_buffer.get();
        for (size_t index = event_begin_pos; index < events_length; ++index)
        {
            if (input_data[index] == '\n')
            {
                have_line = true;
                event_end_pos = index + 1;
                break;
            }
        }

        if (have_line)
        {
            if (discard_line)
            {
                discard_line = false;
            }
            else
            {
                event_line = std::unique_ptr<std::string>(
                    new std::string(&(input_data[event_begin_pos]), event_end_pos - event_begin_pos - 1)
                );
                line_pending = true;
            }
            event_begin_pos = event_end_pos;
        }
        else
        {
            // Indicate that all available data has been searched for event lines
            // and more data needs to be read to find another event line
            data_pending = false;

            if (event_begin_pos > 0)
            {
                // No more lines in the buffer, compact buffer
                size_t src_index = event_begin_pos;
                size_t dst_index = 0;
                while (src_index < events_length)
                {
                    input_data[dst_index] = input_data[src_index];
                    ++src_index;
                    ++dst_index;
                }
                events_length = events_length - event_begin_pos;
                event_begin_pos = 0;
            }
            else
            if (events_length == MAX_LINE_LENGTH)
            {
                // Buffer is full, but no lines were found
                // Too long line, discard input
                event_begin_pos = 0;
                events_length = 0;
                discard_line = true;

                // Currently, a too long line is considered an error
                // If throwing the exception is removed, too long lines
                // will simply be discarded instead
                std::string error_msg("Event line exceeded maximum permitted length");
                throw EventsIoException(&error_msg, nullptr, nullptr);
            }
        }
    }

    return line_pending;
}

// Throws EventsIoException if rc is not equal to 0
// @throws EventsIoException
void EventsIo::checked_int_rc(int rc) const
{
    if (rc != 0)
    {
        throw EventsIoException();
    }
}

void EventsIo::cleanup() noexcept
{
    posix::close_fd(poll_fd);
    posix::close_fd(events_fd);
    posix::close_fd(error_fd);
    posix::close_fd(sig_fd);
    // stdin_fd is not closed
}
