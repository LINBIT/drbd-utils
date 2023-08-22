#ifndef MESSAGELOGNOTIFICATION_H
#define MESSAGELOGNOTIFICATION_H

#include <default_types.h>
#include <MessageLogObserver.h>
#include <EventsIoWakeup.h>
#include <MessageLog.h>
#include <atomic>

class MessageLogNotification : public MessageLogObserver
{
  public:
    MessageLogNotification(MessageLog& log_ref, EventsIoWakeup* const wakeup_target_ref);
    virtual ~MessageLogNotification() noexcept;

    virtual void notify_log_changed() noexcept override;
    virtual bool query_log_changed() noexcept override;

  private:
    MessageLog&     log;
    EventsIoWakeup* wakeup_target;

    std::atomic<bool>   notified_flag;
};

#endif /* MESSAGELOGNOTIFICATION_H */
