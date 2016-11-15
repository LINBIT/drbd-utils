#ifndef EXCEPTIONS_H
#define	EXCEPTIONS_H

#include <stdexcept>

class NumberFormatException : public std::exception
{
  public:
    NumberFormatException() = default;
    NumberFormatException(const NumberFormatException& orig) = default;
    NumberFormatException& operator=(const NumberFormatException& orig) = default;
    NumberFormatException(NumberFormatException&& orig) = default;
    NumberFormatException& operator=(NumberFormatException&& orig) = default;
    virtual ~NumberFormatException() noexcept
    {
    }
};

class EventException : public std::exception
{
  public:
    EventException() = default;
    virtual ~EventException() noexcept
    {
    }
    EventException(const EventException& orig) = default;
    EventException& operator=(const EventException& orig) = default;
    EventException(EventException&& orig) = default;
    EventException& operator=(EventException&& orig) = default;
};

// Thrown to indicate malformed / unparsable 'drbdsetup events2' lines
class EventMessageException : public EventException
{
  public:
    EventMessageException() = default;
    EventMessageException(const EventMessageException& orig) = default;
    EventMessageException& operator=(const EventMessageException& orig) = default;
    EventMessageException(EventMessageException&& orig) = default;
    EventMessageException& operator=(EventMessageException&& orig) = default;
    virtual ~EventMessageException() noexcept
    {
    }
};

// Thrown to indicate that a 'drbdsetup events2' line references an object
// that does not exist
class EventObjectException : public EventException
{
  public:
    EventObjectException() = default;
    EventObjectException(const EventObjectException& orig) = default;
    EventObjectException& operator=(const EventObjectException& orig) = default;
    EventObjectException(EventObjectException&& orig) = default;
    EventObjectException& operator=(EventObjectException&& orig) = default;
    virtual ~EventObjectException() noexcept
    {
    }
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

#endif	/* EXCEPTIONS_H */
