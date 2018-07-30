#ifndef INTERVALTIMER_H
#define	INTERVALTIMER_H

#include <exceptions.h>
#include <MessageLog.h>
#include <time.h>
#include <signal.h>

class IntervalTimer
{
  public:
    // throws std::bad_alloc, ConfigurationException
    IntervalTimer(MessageLog& log, uint16_t interval_msecs);
    virtual ~IntervalTimer() noexcept;
    IntervalTimer(const IntervalTimer& orig) = delete;
    IntervalTimer& operator=(const IntervalTimer& orig) = delete;
    IntervalTimer(IntervalTimer&& orig) = delete;
    IntervalTimer& operator=(IntervalTimer&& orig) = delete;

    virtual timer_t get_timer_id() noexcept;
    virtual void arm_timer();
    virtual struct timespec get_interval() noexcept;
  private:
    timer_t interval_timer_id;
    struct itimerspec interval_timer_cfg;
};

#endif	/* INTERVALTIMER_H */
