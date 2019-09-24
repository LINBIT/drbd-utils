#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <stdexcept>
#include <memory>
#include <string>

class EventException : public std::exception
{
  private:
    std::unique_ptr<std::string> error_msg;
    std::unique_ptr<std::string> debug_info;
    std::unique_ptr<std::string> event_line;
  public:
    EventException() = default;
    // @throws std::bad_alloc
    EventException(
        const std::string* const error_msg_ref,
        const std::string* const debug_info_ref,
        const std::string* const event_line_ref
    );
    virtual ~EventException() noexcept;
    EventException(const EventException& orig) = default;
    EventException& operator=(const EventException& orig) = default;
    EventException(EventException&& orig) = default;
    EventException& operator=(EventException&& orig) = default;
    // @throws std::bad_alloc
    virtual void set_debug_info(const std::string* const debug_info_ref);
    // @throws std::bad_alloc
    virtual void set_event_line(const std::string* const event_line_ref);
    virtual const std::string* get_error_msg() const noexcept;
    virtual const std::string* get_debug_info() const noexcept;
    virtual const std::string* get_event_line() const noexcept;
};

// Thrown to indicate malformed / unparsable 'drbdsetup events2' lines
class EventMessageException : public EventException
{
  public:
    EventMessageException() = default;
    // @throws std::bad_alloc
    EventMessageException(
        const std::string* const error_msg_ref,
        const std::string* const debug_info_ref,
        const std::string* const event_line_ref
    );
    virtual ~EventMessageException() noexcept;
    EventMessageException(const EventMessageException& orig) = delete;
    EventMessageException& operator=(const EventMessageException& orig) = delete;
    EventMessageException(EventMessageException&& orig) = default;
    EventMessageException& operator=(EventMessageException&& orig) = default;
};

// Thrown to indicate that a 'drbdsetup events2' line references an object
// that does not exist
class EventObjectException : public EventException
{
  public:
    EventObjectException() = default;
    // @throws std::bad_alloc
    EventObjectException(
        const std::string* const error_msg_ref,
        const std::string* const debug_info_ref,
        const std::string* const event_line_ref
    );
    virtual ~EventObjectException() noexcept;
    EventObjectException(const EventObjectException& orig) = delete;
    EventObjectException& operator=(const EventObjectException& orig) = delete;
    EventObjectException(EventObjectException&& orig) = default;
    EventObjectException& operator=(EventObjectException&& orig) = default;
};

class EventsSourceException : public EventException
{
  public:
    EventsSourceException() = default;
    // @throws std::bad_alloc
    EventsSourceException(
        const std::string* const error_msg_ref,
        const std::string* const debug_info_ref,
        const std::string* const event_line_ref
    );
    virtual ~EventsSourceException() noexcept;
    EventsSourceException(const EventsSourceException& orig) = delete;
    EventsSourceException& operator=(const EventsSourceException& orig) = delete;
    EventsSourceException(EventsSourceException&& orig) = default;
    EventsSourceException& operator=(EventsSourceException&& orig) = default;
};

class EventsIoException : public EventException
{
  public:
    EventsIoException() = default;
    // @throws std::bad_alloc
    EventsIoException(
        const std::string* const error_msg_ref,
        const std::string* const debug_info_ref,
        const std::string* const event_line_ref
    );
    virtual ~EventsIoException() noexcept;
    EventsIoException(const EventsIoException& orig) = delete;
    EventsIoException& operator=(const EventsIoException& orig) = delete;
    EventsIoException(EventsIoException&& orig) = default;
    EventsIoException& operator=(EventsIoException&& orig) = default;
};

// Thrown to indicate that DrbdMon should abort configuring options
// and exit
class ConfigurationException : public std::exception
{
  public:
    ConfigurationException() = default;
    ConfigurationException(const ConfigurationException& orig) = default;
    ConfigurationException& operator=(const ConfigurationException& orig) = default;
    ConfigurationException(ConfigurationException&& orig) = default;
    ConfigurationException& operator=(ConfigurationException&& orig) = default;
    virtual ~ConfigurationException() noexcept
    {
    }
};

// Thrown to indicate a problem with the operating system's timer functions
class TimerException : public std::exception
{
  public:
    TimerException() = default;
    TimerException(const TimerException& orig) = default;
    TimerException& operator=(const TimerException& orig) = default;
    TimerException(TimerException&& orig) = default;
    TimerException& operator=(TimerException&& orig) = default;
    virtual ~TimerException() noexcept
    {
    }
};

// Thrown to indicate a problem with the system configuration
// (e.g., an illegal state of the operating system / runtime environment)
class SysException : public std::exception
{
  public:
    SysException() = default;
    SysException(const SysException& orig) = default;
    SysException& operator=(const SysException& orig) = default;
    SysException(SysException&& orig) = default;
    SysException& operator=(SysException&& orig) = default;
    virtual ~SysException() noexcept
    {
    }
};

#endif	/* EXCEPTIONS_H */
