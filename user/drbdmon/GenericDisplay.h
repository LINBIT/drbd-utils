#ifndef GENERICDISPLAY_H
#define	GENERICDISPLAY_H

#include <cstdint>

class GenericDisplay
{
  public:
    virtual ~GenericDisplay() noexcept
    {
    }

    virtual void initial_display() = 0;
    virtual void status_display() = 0;
    virtual void display_header() const = 0;
    virtual void set_terminal_size(uint16_t size_x, uint16_t size_y) = 0;
    virtual void key_pressed(const char key) = 0;
};

#endif	/* GENERICDISPLAY_H */
