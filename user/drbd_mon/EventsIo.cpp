#include "EventsIo.h"
#include <cstring>

extern "C"
{
    #include <errno.h>
}

// epoll_event datastructure slot indices
const int EventsIo::EVENTS_CTL_INDEX = 0;
const int EventsIo::SIG_CTL_INDEX    = 1;
const int EventsIo::STDIN_CTL_INDEX  = 2;
// Number of epoll_event datastructure slots
const int EventsIo::CTL_SLOTS_COUNT  = 3;

// @throws std::bad_alloc, std::ios_base::failure
EventsIo::EventsIo(int events_input_fd):
    events_fd(events_input_fd)
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
            throw EventsIoException();
        }

        // Initialize signalfds to enable polling for signals
        // Block the default signal handlers for the same signals
        sigset_t mask;
        checked_int_rc(sigemptyset(&mask));
        checked_int_rc(sigaddset(&mask, SIGTERM));
        checked_int_rc(sigaddset(&mask, SIGHUP));
        checked_int_rc(sigaddset(&mask, SIGINT));
        checked_int_rc(sigaddset(&mask, SIGWINCH));
        checked_int_rc(sigaddset(&mask, SIGCHLD));
        checked_int_rc(sigprocmask(SIG_BLOCK, &mask, nullptr));
        sig_fd = signalfd(-1, &mask, SFD_NONBLOCK | SFD_CLOEXEC);
        if (sig_fd == -1)
        {
            throw EventsIoException();
        }

        // Allocate poll event data structures
        ctl_events = new struct epoll_event[CTL_SLOTS_COUNT];
        static_cast<void> (std::memset(static_cast<void*> (ctl_events), 0,
                                       sizeof (struct epoll_event) * CTL_SLOTS_COUNT));
        fired_events = new struct epoll_event[CTL_SLOTS_COUNT];
        static_cast<void> (std::memset(static_cast<void*> (fired_events), 0,
                                       sizeof (struct epoll_event) * CTL_SLOTS_COUNT));

        events_buffer = new char[MAX_LINE_LENGTH];
        signal_buffer = new struct signalfd_siginfo;

        // Initialize the poll event data structures
        register_poll(events_fd, &(ctl_events[EVENTS_CTL_INDEX]), EPOLLIN);
        register_poll(sig_fd, &(ctl_events[SIG_CTL_INDEX]), EPOLLIN);
        register_poll(stdin_fd, &(ctl_events[STDIN_CTL_INDEX]), EPOLLIN);
    }
    catch (EventsIoException&)
    {
        cleanup();

        if (errno == ENOMEM)
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

// @throws EventsIoException
void EventsIo::register_poll(int fd, struct epoll_event* event_ctl_slot, uint32_t event_mask)
{
    event_ctl_slot->data.fd = fd;
    event_ctl_slot->events = event_mask;
    int rc = epoll_ctl(poll_fd, EPOLL_CTL_ADD, fd, event_ctl_slot);
    if (rc != 0)
    {
        throw EventsIoException();
    }
}

// @throws EventsIoException
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
                event_count = epoll_wait(poll_fd, fired_events, CTL_SLOTS_COUNT, -1);
                if (event_count > 0)
                {
                    pending_events = true;
                }
                else
                if (errno != 0 && errno != EINTR)
                {
                    throw EventsIoException();
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
                throw EventsIoException();
            }

            read_events();
            if (prepare_line())
            {
                event_id = EventsIo::event::EVENT_LINE;
            }
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
            throw EventsIoException();
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

// @throws EventsIoException
int EventsIo::get_signal()
{
    int signal_id = 0;

    errno = 0;
    ssize_t read_size = 0;
    do
    {
        read_size = read(sig_fd, signal_buffer, sizeof (struct signalfd_siginfo));
        if (read_size == sizeof (struct signalfd_siginfo))
        {
            signal_id = static_cast<int> (signal_buffer->ssi_signo);
        }
        else
        if (read_size == 0)
        {
            throw EventsIoException();
        }
    }
    while (read_size == -1 && errno == EINTR);

    if (read_size == -1)
    {
        throw EventsIoException();
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

// @throws EventsIoException
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
                throw EventsIoException();
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
    return event_line;
}

void EventsIo::free_event_line()
{
    delete event_line;
    event_line = nullptr;
}

// @throws std::bad_alloc, EventsIoException
bool EventsIo::prepare_line()
{
    if (!line_pending)
    {
        if (event_line != nullptr)
        {
            delete event_line;
            event_line = nullptr;
        }

        bool have_line = false;
        size_t event_end_pos = 0;
        // Check for new complete lines in the buffer
        for (size_t index = event_begin_pos; index < events_length; ++index)
        {
            if (events_buffer[index] == '\n')
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
                event_line = new std::string(&(events_buffer[event_begin_pos]), event_end_pos - event_begin_pos - 1);
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
                    events_buffer[dst_index] = events_buffer[src_index];
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
                throw EventsIoException();
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
    if (poll_fd != -1)
    {
        close(poll_fd);
    }
    delete[] ctl_events;
    delete[] fired_events;

    delete[] events_buffer;

    // signal_buffer is a struct
    delete   signal_buffer;
    delete   event_line;

    if (events_fd != -1)
    {
        close(events_fd);
    }

    if (sig_fd != -1)
    {
        close(sig_fd);
    }

    // stdin_fd is not closed
}
