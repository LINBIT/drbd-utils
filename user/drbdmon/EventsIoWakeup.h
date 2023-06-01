#ifndef EVENTSIOWAKEUP_H
#define EVENTSIOWAKEUP_H

class EventsIoWakeup
{
  public:
    virtual ~EventsIoWakeup() noexcept
    {
    }
    virtual void wakeup_wait() noexcept = 0;
};

#endif /* EVENTSIOWAKEUP_H */
