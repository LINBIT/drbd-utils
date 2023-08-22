#ifndef SUBPROCESSNOTIFICATION_H
#define SUBPROCESSNOTIFICATION_H

#include <default_types.h>
#include <mutex>
#include <subprocess/SubProcessObserver.h>
#include <EventsIoWakeup.h>

class SubProcessNotification : public SubProcessObserver
{
  public:
    static const uint64_t   MASK_SUBPROCESS_CHANGED;
    static const uint64_t   MASK_OUT_OF_MEMORY;

    SubProcessNotification(EventsIoWakeup* const wakeup_target_ref);
    virtual ~SubProcessNotification() noexcept;

    virtual void notify_queue_changed() noexcept override;
    virtual void notify_out_of_memory() noexcept override;

    virtual uint64_t get_notification_mask() noexcept;

  private:
    std::mutex          notification_lock;
    uint64_t            notification_mask   {0};
    EventsIoWakeup*     wakeup_target;
};

#endif /* SUBPROCESSNOTIFICATION_H */
