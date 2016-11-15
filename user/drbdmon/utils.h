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

class NumberParser
{
  public:
    // @throws NumberFormatException
    static uint8_t parse_uint8(std::string& number_str);

    // @throws NumberFormatException
    static uint16_t parse_uint16(std::string& number_str);

    // @throws NumberFormatException
    static int32_t parse_int32(std::string& number_str);

  private:
    NumberParser() = default;
    NumberParser(const NumberParser& orig) = default;
    NumberParser& operator=(const NumberParser& orig) = default;
    NumberParser(NumberParser&& orig) = default;
    NumberParser& operator=(NumberParser&& orig) = default;
    virtual ~NumberParser() noexcept
    {
    }

    // @throws NumberFormatException
    static unsigned long long parse_unsigned(std::string& number_str);

    // @throws NumberFormatException
    static signed long long parse_signed(std::string& number_str);
};

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
