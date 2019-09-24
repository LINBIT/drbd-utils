#ifndef EVENTSIO_H
#define	EVENTSIO_H

#include <new>
#include <memory>
#include <stdexcept>

#include <exceptions.h>

extern "C"
{
    #include <unistd.h>
    #include <signal.h>
    #include <sys/epoll.h>
    #include <sys/signalfd.h>
    #include <termios.h>
}

class EventsIo
{
  public:
    enum class event : uint32_t
    {
        NONE,
        EVENT_LINE,
        SIGNAL,
        STDIN
    };

    static const size_t MAX_LINE_LENGTH {1024};
    static const size_t ERROR_BUFFER_SIZE {1024};

    // @throws std::bad_alloc, std::ios_base::failure
    explicit EventsIo(int events_input_fd, int events_error_fd);
    virtual ~EventsIo() noexcept;

    EventsIo(const EventsIo& orig) = delete;
    EventsIo& operator=(const EventsIo& orig) = delete;

    EventsIo(EventsIo&& orig) = default;
    EventsIo& operator=(EventsIo&& orig) = delete;

    // @throws std::bad_alloc, EventsIoException
    virtual event wait_event();

    // Returns the current event line
    // @throws std::bad_alloc, EventsIoException
    virtual std::string* get_event_line();

    // Can be called optionally to free the current event line
    // Otherwise, the current event line will be freed whenever the next
    // event line is prepared, or upon destruction of the EventsIo instance
    virtual void free_event_line();

    // @throws std::bad_alloc, EventsIoException
    virtual int get_signal();

    // @throws EventsIoException
    virtual void adjust_terminal();
    virtual void restore_terminal();

  private:
    // @throws std::bad_alloc, EventsIoException
    void register_poll(int fd, struct epoll_event* event_ctl, uint32_t event_mask);

    // @throws EventsIoException
    void checked_int_rc(int rc) const;
    void cleanup() noexcept;

    // epoll_event datastructure slot indices
    static const int EVENTS_CTL_INDEX;
    static const int ERROR_CTL_INDEX;
    static const int SIG_CTL_INDEX;
    static const int STDIN_CTL_INDEX;
    // Number of epoll_event datastructure slots
    static const int CTL_SLOTS_COUNT;

    int poll_fd   {-1};
    int events_fd {-1};
    int error_fd  {-1};
    int sig_fd    {-1};
    int stdin_fd  {STDIN_FILENO};

    int  event_count    {0};
    int  current_event  {0};
    bool pending_events {false};

    const std::unique_ptr<struct epoll_event[]> ctl_events;
    const std::unique_ptr<struct epoll_event[]> fired_events;

    const std::unique_ptr<signalfd_siginfo> signal_buffer;

    // Events processing
    const std::unique_ptr<char[]> events_buffer;
    const std::unique_ptr<char[]> error_buffer;
    std::unique_ptr<std::string> event_line;

    size_t event_begin_pos {0};
    size_t events_length   {0};
    bool   events_eof      {false};
    bool   errors_eof      {false};
    bool   discard_line    {false};
    bool   line_pending    {false};
    bool   data_pending    {false};

    bool have_orig_termios {false};
    struct termios orig_termios;
    struct termios adjusted_termios;

    // @throws std::bad_alloc, EventsIoException
    void read_events();

    // @throws std::bad_alloc, EventsIoException
    void read_errors();

    // @throws std::bad_alloc, EventsIoException
    bool prepare_line();
};

#endif	/* EVENTSIO_H */
