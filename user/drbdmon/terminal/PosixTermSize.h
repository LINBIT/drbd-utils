#ifndef POSIXTERMSIZE_H
#define POSIXTERMSIZE_H

#include <terminal/TermSize.h>
#include <cstdint>

extern "C"
{
    #include <sys/ioctl.h>
}

class PosixTermSize : public TermSize
{
  public:
    PosixTermSize();
    virtual ~PosixTermSize() noexcept;
    PosixTermSize(const PosixTermSize& other) = default;
    PosixTermSize(PosixTermSize&& orig) = default;
    PosixTermSize& operator=(const PosixTermSize& other) = default;
    PosixTermSize& operator=(PosixTermSize&& orig) = default;

    virtual bool probe_terminal_size() noexcept override;
    virtual bool is_valid() const noexcept override;
    virtual uint16_t get_size_x() const noexcept override;
    virtual uint16_t get_size_y() const noexcept override;
  private:
    bool            valid       {false};
    struct winsize  term_size;
};


#endif /* POSIXTERMSIZE_H */

