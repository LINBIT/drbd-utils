#include <terminal/MouseEvent.h>

MouseEvent::MouseEvent()
{
}

MouseEvent::~MouseEvent() noexcept
{
}

MouseEvent& MouseEvent::operator=(const MouseEvent& other) noexcept
{
    if (this != &other)
    {
        event = other.event;
        button = other.button;
        coord_column = other.coord_column;
        coord_row = other.coord_row;
    }

    return *this;
}
