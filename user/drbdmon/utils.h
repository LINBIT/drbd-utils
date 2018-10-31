#ifndef UTILS_H
#define	UTILS_H

#include <string>
#include <cstdlib>
#include <stdexcept>

#include <exceptions.h>

extern "C"
{
    #include <sys/ioctl.h>
}

class TermSize
{
  public:
    TermSize();
    TermSize(const TermSize& orig) = default;
    TermSize& operator=(const TermSize& orig) = default;
    TermSize(TermSize&& orig) = default;
    TermSize& operator=(TermSize&& orig) = default;
    virtual ~TermSize() noexcept
    {
    }

    virtual bool probe_terminal_size();
    virtual bool is_valid() const;
    virtual uint16_t get_size_x() const;
    virtual uint16_t get_size_y() const;

  private:
    struct winsize term_size;
    bool valid {false};
};

#endif	/* UTILS_H */
