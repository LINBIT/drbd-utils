#ifndef EVENTSIO_H
#define	EVENTSIO_H

#include <new>
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

    // @throws std::bad_alloc, std::ios_base::failure
    explicit EventsIo(int events_input_fd);
    virtual ~EventsIo() noexcept;

    EventsIo(const EventsIo& orig) = delete;
    EventsIo& operator=(const EventsIo& orig) = delete;

    EventsIo(EventsIo&& orig) = default;
    EventsIo& operator=(EventsIo&& orig) = delete;

    // @throws EventsIoException
    virtual event wait_event();

    // Returns the current event line
    virtual std::string* get_event_line();

    // Can be called optionally to free the current event line
    // Otherwise, the current event line will be freed whenever the next
    // event line is prepared, or upon destruction of the EventsIo instance
    virtual void free_event_line();

    virtual int get_signal();

    // @throws EventsIoException
    virtual void adjust_terminal();
    virtual void restore_terminal();

  private:
    // @throws EventsIoException
    void register_poll(int fd, struct epoll_event* event_ctl, uint32_t event_mask);

    // @throws EventsIoException
    void checked_int_rc(int rc) const;
    void cleanup() noexcept;

    // epoll_event datastructure slot indices
    static const int EVENTS_CTL_INDEX;
    static const int SIG_CTL_INDEX;
    static const int STDIN_CTL_INDEX;
    // Number of epoll_event datastructure slots
    static const int CTL_SLOTS_COUNT;

    int poll_fd   {-1};
    int events_fd {-1};
    int sig_fd    {-1};
    int stdin_fd  {STDIN_FILENO};

    int  event_count    {0};
    int  current_event  {0};
    bool pending_events {false};

    struct epoll_event* ctl_events   {nullptr};
    struct epoll_event* fired_events {nullptr};

    struct signalfd_siginfo* signal_buffer {nullptr};

    // Events processing
    char*        events_buffer {nullptr};
    std::string* event_line    {nullptr};

    size_t event_begin_pos {0};
    size_t events_length   {0};
    bool   events_eof      {false};
    bool   discard_line    {false};
    bool   line_pending    {false};
    bool   data_pending    {false};

    bool have_orig_termios {false};
    struct termios orig_termios;
    struct termios adjusted_termios;

    // @throws EventsIoException
    void read_events();

    // @throws std::bad_alloc, EventsIoException
    bool prepare_line();
};

class EventsIoException : public EventException
{
  public:
    EventsIoException() = default;
    virtual ~EventsIoException() noexcept = default;
    EventsIoException(const EventsIoException& orig) = delete;
    EventsIoException& operator=(const EventsIoException& orig) = delete;
    EventsIoException(EventsIoException&& orig) = default;
    EventsIoException& operator=(EventsIoException&& orig) = default;
};

#endif	/* EVENTSIO_H */
