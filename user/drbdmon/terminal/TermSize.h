#ifndef TERMSIZE_H
#define TERMSIZE_H

#include <cstdint>

class TermSize
{
  public:
    virtual ~TermSize() noexcept
    {
    }

    virtual bool probe_terminal_size() noexcept = 0;
    virtual bool is_valid() const noexcept = 0;
    virtual uint16_t get_size_x() const noexcept = 0;
    virtual uint16_t get_size_y() const noexcept = 0;
};

#endif /* TERMSIZE_H */

