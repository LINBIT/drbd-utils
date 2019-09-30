#include <IntervalTimer.h>
#include <cstring>

// throws std::bad_alloc, ConfigurationException
IntervalTimer::IntervalTimer(MessageLog& log, uint16_t interval_msecs)
{
    std::unique_ptr<struct sigevent> timer_evt_cfg_mgr(new struct sigevent);

    struct sigevent* timer_evt_cfg = timer_evt_cfg_mgr.get();
    std::memset(timer_evt_cfg, 0, sizeof (struct sigevent));
    timer_evt_cfg->sigev_notify = SIGEV_SIGNAL;
    timer_evt_cfg->sigev_signo = SIGALRM;
    int rc = timer_create(CLOCK_MONOTONIC, timer_evt_cfg, &interval_timer_id);
    if (rc != 0)
    {
        std::string error_message("Timer setup for limited frequency display update failed (errno=");
        error_message += errno;
        error_message += ")";
        log.add_entry(MessageLog::log_level::ALERT, error_message);
        throw ConfigurationException();
    }

    interval_timer_cfg.it_interval.tv_nsec = 0;
    interval_timer_cfg.it_interval.tv_sec = 0;
    interval_timer_cfg.it_value.tv_sec = interval_msecs / 1000;
    interval_timer_cfg.it_value.tv_nsec = (interval_msecs % 1000) * 1000000;
}

IntervalTimer::~IntervalTimer() noexcept
{
    timer_delete(interval_timer_id);
}

// @throws TimerException
void IntervalTimer::arm_timer()
{
    if (timer_settime(interval_timer_id, 0, &interval_timer_cfg, nullptr) != 0)
    {
        throw TimerException();
    }
}

timer_t IntervalTimer::get_timer_id() noexcept
{
    return interval_timer_id;
}

struct timespec IntervalTimer::get_interval() noexcept
{
    return interval_timer_cfg.it_value;
}
