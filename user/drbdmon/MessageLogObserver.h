#ifndef MESSAGELOGOBSERVER_H
#define MESSAGELOGOBSERVER_H

class MessageLogObserver
{
  public:
    virtual ~MessageLogObserver() noexcept
    {
    }

    // Notifies the observer, so that the next call of query_log_changed will return true
    virtual void notify_log_changed() noexcept = 0;
    // Queries and atomically resets the notification state
    virtual bool query_log_changed() noexcept = 0;
};

#endif /* MESSAGELOGOBSERVER_H */
