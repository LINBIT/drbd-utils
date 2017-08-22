#ifndef STATEFLAGS_H
#define	STATEFLAGS_H

#include <cstdint>

class StateFlags
{
  public:

    // MARK is used to indicate that sub-objects
    // of the marked object have warnings or alerts
    enum class state : uint16_t
    {
        NORM,
        MARK,
        WARN,
        ALERT
    };

    StateFlags() = default;
    StateFlags(const StateFlags& orig) = default;
    StateFlags& operator=(const StateFlags& orig) = default;
    StateFlags(StateFlags&& orig) = default;
    StateFlags& operator=(StateFlags&& orig) = default;
    virtual ~StateFlags() noexcept
    {
    }

    virtual bool has_mark_state() const;
    virtual bool has_warn_state() const;
    virtual bool has_alert_state() const;
    virtual void set_mark();
    virtual void set_warn();
    virtual void set_alert();
    virtual state get_state() const;
    virtual void clear_state_flags();
    virtual state update_state_flags() = 0;
    virtual state child_state_flags_changed() = 0;

  protected:
    state obj_state {StateFlags::state::ALERT};
};

#endif	/* STATEFLAGS_H */
