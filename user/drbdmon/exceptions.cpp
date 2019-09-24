#include <exceptions.h>

//@throws std::bad_alloc
EventException::EventException(
    const std::string* const error_msg_ref,
    const std::string* const debug_info_ref,
    const std::string* const event_line_ref
)
{
    if (error_msg_ref != nullptr)
    {
        error_msg = std::unique_ptr<std::string>(new std::string(*error_msg_ref));
    }
    if (debug_info_ref != nullptr)
    {
        debug_info = std::unique_ptr<std::string>(new std::string(*debug_info_ref));
    }
    if (event_line_ref != nullptr)
    {
        event_line = std::unique_ptr<std::string>(new std::string(*event_line_ref));
    }
}

EventException::~EventException() noexcept
{
}

// @throws std::bad_alloc
void EventException::set_debug_info(const std::string* const debug_info_ref)
{
    if (debug_info_ref != nullptr)
    {
        debug_info = std::unique_ptr<std::string>(new std::string(*debug_info_ref));
    }
    else
    {
        debug_info = nullptr;
    }
}

// @throws std::bad_alloc
void EventException::set_event_line(const std::string* const event_line_ref)
{
    if (event_line_ref != nullptr)
    {
        event_line = std::unique_ptr<std::string>(new std::string(*event_line_ref));
    }
    else
    {
        event_line = nullptr;
    }
}

const std::string* EventException::get_error_msg() const noexcept
{
    return error_msg.get();
}

const std::string* EventException::get_debug_info() const noexcept
{
    return debug_info.get();
}

const std::string* EventException::get_event_line() const noexcept
{
    return event_line.get();
}

// @throws std::bad_alloc
EventMessageException::EventMessageException(
    const std::string* const error_msg_ref,
    const std::string* const debug_info_ref,
    const std::string* const event_line_ref
):
    EventException::EventException(error_msg_ref, debug_info_ref, event_line_ref)
{
}

EventMessageException::~EventMessageException() noexcept
{
}

// @throws std::bad_alloc
EventObjectException::EventObjectException(
    const std::string* const error_msg_ref,
    const std::string* const debug_info_ref,
    const std::string* const event_line_ref
):
    EventException::EventException(error_msg_ref, debug_info_ref, event_line_ref)
{
}

EventObjectException::~EventObjectException() noexcept
{
}

// @throws std::bad_alloc
EventsSourceException::EventsSourceException(
    const std::string* const error_msg_ref,
    const std::string* const debug_info_ref,
    const std::string* const event_line_ref
):
    EventException::EventException(error_msg_ref, debug_info_ref, event_line_ref)
{
}

EventsSourceException::~EventsSourceException() noexcept
{
}

// @throws std::bad_alloc
EventsIoException::EventsIoException(
    const std::string* const error_msg_ref,
    const std::string* const debug_info_ref,
    const std::string* const event_line_ref
):
    EventException::EventException(error_msg_ref, debug_info_ref, event_line_ref)
{
}

EventsIoException::~EventsIoException() noexcept
{
}
