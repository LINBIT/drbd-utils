#include <MessageLogNotification.h>
#include <stdexcept>

MessageLogNotification::MessageLogNotification(MessageLog& log_ref, EventsIoWakeup* const wakeup_target_ref):
    log(log_ref)
{
    wakeup_target = wakeup_target_ref;
}

MessageLogNotification::~MessageLogNotification() noexcept
{
    log.cancel_observer();
}

void MessageLogNotification::notify_log_changed() noexcept
{
    wakeup_target->wakeup_wait();
}

bool MessageLogNotification::query_log_changed() noexcept
{
    bool result = false;
    try
    {
        // Not actually supposed to throw, but not noexcept qualified
        result = notified_flag.exchange(false);
    }
    catch (std::exception&)
    {
    }
    return result;
}
