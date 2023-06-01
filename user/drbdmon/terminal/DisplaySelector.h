#ifndef DISPLAYSELECTOR_H
#define DISPLAYSELECTOR_H

#include <terminal/DisplayId.h>
#include <terminal/ModularDisplay.h>
#include <string>

class DisplaySelector
{
  public:
    virtual ~DisplaySelector() noexcept
    {
    }

    virtual bool switch_to_display(const std::string& display_name) = 0;
    virtual void switch_to_display(const DisplayId::display_page display_id) = 0;
    virtual void leave_display() = 0;
    virtual bool can_leave_display() = 0;
    virtual DisplayId::display_page get_active_page() = 0;
    virtual ModularDisplay& get_active_display() = 0;
    virtual void synchronize_displays() = 0;

    // Called to indicate that the display controller should refresh the
    // display after handling key or mouse actions
    virtual void refresh_display() = 0;
};

#endif /* DISPLAYSELECTOR_H */
