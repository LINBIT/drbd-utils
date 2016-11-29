#include <StateFlags.h>

bool StateFlags::has_mark_state() const
{
    return (obj_state != StateFlags::state::NORM);
}

bool StateFlags::has_warn_state() const
{
    return (obj_state == StateFlags::state::WARN ||
            obj_state == StateFlags::state::ALERT);
}

bool StateFlags::has_alert_state() const
{
    return (obj_state == StateFlags::state::ALERT);
}

StateFlags::state StateFlags::get_state() const
{
    return obj_state;
}

void StateFlags::set_mark()
{
    if (obj_state == StateFlags::state::NORM)
    {
        obj_state = StateFlags::state::MARK;
    }
}

void StateFlags::set_warn()
{
    if (obj_state != StateFlags::state::ALERT)
    {
        obj_state = StateFlags::state::WARN;
    }
}

void StateFlags::set_alert()
{
    obj_state = StateFlags::state::ALERT;
}

void StateFlags::clear_state_flags()
{
    obj_state = StateFlags::state::NORM;
}
