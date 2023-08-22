#ifndef GENERICDISPLAY_H
#define	GENERICDISPLAY_H

#include <default_types.h>
#include <terminal/MouseEvent.h>

class GenericDisplay
{
  public:
    virtual ~GenericDisplay() noexcept
    {
    }

    virtual void initialize() = 0;
    virtual void enter_initial_display() = 0;
    virtual void exit_initial_display() = 0;
    virtual bool notify_drbd_changed() = 0;
    virtual bool notify_task_queue_changed() = 0;
    virtual bool notify_message_log_changed() = 0;
    virtual void terminal_size_changed() = 0;
    virtual void key_pressed(const uint32_t key) = 0;
    virtual void mouse_action(MouseEvent& mouse) = 0;
};

#endif	/* GENERICDISPLAY_H */
