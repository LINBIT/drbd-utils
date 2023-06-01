#ifndef MOUSEEVENT_H
#define MOUSEEVENT_H

#include <cstdint>

class MouseEvent
{
  public:
    enum event_id
    {
        MOUSE_MOVE      = 0,
        MOUSE_DOWN      = 1,
        MOUSE_RELEASE   = 2,
        SCROLL_UP       = 3,
        SCROLL_DOWN     = 4
    };

    enum button_id
    {
        NONE        = 0,
        // Left mouse button
        BUTTON_01   = 1,
        // Middle mouse button / scroll wheel click
        BUTTON_02   = 2,
        // Right mouse button
        BUTTON_03   = 3
    };

    uint16_t    event           {0};
    uint16_t    button          {0};
    uint16_t    coord_column    {0};
    uint16_t    coord_row       {0};

    MouseEvent();
    virtual ~MouseEvent() noexcept;

    virtual MouseEvent& operator=(const MouseEvent& other) noexcept;
};


#endif /* MOUSEEVENT_H */
