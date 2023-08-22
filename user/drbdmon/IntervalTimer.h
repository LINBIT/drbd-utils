#ifndef INTERVALTIMER_H
#define	INTERVALTIMER_H

#include <default_types.h>
#include <exceptions.h>
#include <MessageLog.h>
#include <time.h>
#include <signal.h>

class IntervalTimer
{
  public:
    // throws std::bad_alloc, ConfigurationException
    IntervalTimer(MessageLog& log, const uint16_t interval_msecs);
    virtual ~IntervalTimer() noexcept;
    IntervalTimer(const IntervalTimer& orig) = delete;
    IntervalTimer& operator=(const IntervalTimer& orig) = delete;
    IntervalTimer(IntervalTimer&& orig) = delete;
    IntervalTimer& operator=(IntervalTimer&& orig) = delete;

    virtual void set_interval(const uint16_t interval_msecs) noexcept;
    virtual timer_t get_timer_id() noexcept;
    virtual void arm_timer();
    virtual struct timespec get_interval() noexcept;
  private:
    timer_t interval_timer_id;
    struct itimerspec interval_timer_cfg;

    void set_interval_impl(const uint16_t interval_msecs) noexcept;
};

#endif	/* INTERVALTIMER_H */
