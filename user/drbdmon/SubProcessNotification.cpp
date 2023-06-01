#include <SubProcessNotification.h>
#include <EventsIoWakeup.h>

const uint64_t  SubProcessNotification::MASK_SUBPROCESS_CHANGED     = 1;
const uint64_t  SubProcessNotification::MASK_OUT_OF_MEMORY          = static_cast<uint64_t> (0x8000000000000000ULL);

SubProcessNotification::SubProcessNotification(EventsIoWakeup* const wakeup_target_ref)
{
    wakeup_target = wakeup_target_ref;
}

SubProcessNotification::~SubProcessNotification() noexcept
{
}

void SubProcessNotification::notify_queue_changed() noexcept
{
    {
        std::unique_lock<std::mutex> lock(notification_lock);
        notification_mask |= MASK_SUBPROCESS_CHANGED;
    }
    wakeup_target->wakeup_wait();
}

void SubProcessNotification::notify_out_of_memory() noexcept
{
    {
        std::unique_lock<std::mutex> lock(notification_lock);
        notification_mask |= MASK_OUT_OF_MEMORY;
    }
    wakeup_target->wakeup_wait();
}

uint64_t SubProcessNotification::get_notification_mask() noexcept
{
    uint64_t result = 0;
    {
        std::unique_lock<std::mutex> lock(notification_lock);
        result = notification_mask;
        notification_mask = 0;
    }
    return result;
}
