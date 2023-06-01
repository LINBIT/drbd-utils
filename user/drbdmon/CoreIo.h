#ifndef COREIO_H
#define COREIO_H

#include <terminal/MouseEvent.h>

class CoreIo
{
  public:
    enum class event : uint32_t
    {
        NONE            = 0,
        EVENT_LINE      = 1,
        SIGNAL          = 2,
        STDIN           = 3,
        WAKEUP          = 4
    };

    enum class signal_type : uint32_t
    {
        SIGNAL_GENERIC      = 0,
        SIGNAL_EXIT,
        SIGNAL_TIMER,
        SIGNAL_CHILD_PROC,
        SIGNAL_WINDOW_SIZE,
        SIGNAL_DEBUG
    };

    virtual ~CoreIo() noexcept
    {
    }

    // @throws std::bad_alloc, EventsIoException
    virtual event wait_event() = 0;

    // Returns the current event line
    // @throws std::bad_alloc, EventsIoException
    virtual std::string* get_event_line() = 0;

    // Can be called optionally to free the current event line
    // Otherwise, the current event line will be freed whenever the next
    // event line is prepared, or upon destruction of the EventsIo instance
    virtual void free_event_line() = 0;

    // @throws std::bad_alloc, EventsIoException
    virtual signal_type get_signal() = 0;

    virtual uint32_t get_key_pressed() = 0;
    virtual MouseEvent& get_mouse_action() = 0;
};

#endif /* COREIO_H */
