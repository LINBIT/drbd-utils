#ifndef MODULARDISPLAY_H
#define MODULARDISPLAY_H

#include <default_types.h>
#include <terminal/MouseEvent.h>
#include <string>
#include <StringTokenizer.h>

class ModularDisplay
{
  public:
    virtual ~ModularDisplay() noexcept
    {
    }

    virtual void display() = 0;
    virtual bool key_pressed(const uint32_t key) = 0;
    virtual bool mouse_action(MouseEvent& mouse) = 0;

    // Called when the active display changes to this display
    virtual void display_activated() = 0;

    // Called when the active display is left (switched to another display),
    // but may still reside on the display stack
    virtual void display_deactivated() = 0;

    // Called when the display is left and is not placed on the display stack
    virtual void display_closed() = 0;

    // Called to reset the display, e.g. go back to page 1, clear cursor & selection
    virtual void reset_display() = 0;

    // Called to synchronize the active display's data to the SharedData object that contains
    // data shared between displays, e.g., the resource/connection/volume currently under the
    // cursor.
    // This method should be called before switching displays and before running commands, and
    // more generally, before invoking methods or functions that operate on shared data.
    virtual void synchronize_data() = 0;

    // Called to synchronize data shared between displays to the active display.
    // This method should be called when shared data is updated without the active display's knowledge,
    // for example, if a global command changes which resource is under the cursor while the resource
    // list is being displayed.
    virtual void notify_data_updated() = 0;

    // Called to execute a command line, returns true if the command was accepted, otherwise false
    // (invalid command, etc.)
    virtual bool execute_command(const std::string& command, StringTokenizer& tokenizer) = 0;

    // Returns the display update mask describing the events that will cause this display to be updated
    virtual uint64_t get_update_mask() noexcept = 0;
};

#endif /* MODULARDISPLAY_H */
