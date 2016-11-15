#include <utils.h>

// @throws NumberFormatException
uint8_t NumberParser::parse_uint8(std::string& number_str)
{
    uint8_t MAX = 0xFF;
    unsigned long long value = parse_unsigned(number_str);
    if (value > MAX)
    {
        throw NumberFormatException();
    }
    return static_cast<uint8_t> (value);
}

// @throws NumberFormatException
uint16_t NumberParser::parse_uint16(std::string& number_str)
{
    uint16_t MAX = ~(static_cast<uint16_t> (0));
    unsigned long long value = parse_unsigned(number_str);
    if (value > MAX)
    {
        throw NumberFormatException();
    }
    return static_cast<uint16_t> (value);
}

// @throws NumberFormatException
int32_t NumberParser::parse_int32(std::string& number_str)
{
    int32_t MAX = 0x7FFFFFFF;
    int32_t MIN = 0xFFFFFFFF;
    signed long long value = parse_signed(number_str);
    if (value < MIN || value > MAX)
    {
        throw NumberFormatException();
    }
    return static_cast<int32_t> (value);
}

// @throws NumberFormatException
unsigned long long NumberParser::parse_unsigned(std::string& number_str)
{
    const char* number_buf = number_str.c_str();
    char* parse_end {nullptr};
    unsigned long long value = strtoull(number_buf, &parse_end, 10);
    if (*parse_end != '\0')
    {
        throw NumberFormatException();
    }
    return value;
}

// @throws NumberFormatException
signed long long NumberParser::parse_signed(std::string& number_str)
{
    const char* number_buf = number_str.c_str();
    char* parse_end {nullptr};
    signed long long value = strtoll(number_buf, &parse_end, 10);
    if (*parse_end != '\0')
    {
        throw NumberFormatException();
    }
    return value;
}

TermSize::TermSize()
{
    term_size.ws_col    = 0;
    term_size.ws_row    = 0;
    term_size.ws_xpixel = 0;
    term_size.ws_ypixel = 0;
}

bool TermSize::probe_terminal_size()
{
    valid = (ioctl(1, TIOCGWINSZ, &term_size) == 0);
    return valid;
}

bool TermSize::is_valid() const
{
    return valid;
}

uint16_t TermSize::get_size_x() const
{
    return static_cast<uint16_t> (term_size.ws_col);
}

uint16_t TermSize::get_size_y() const
{
    return static_cast<uint16_t> (term_size.ws_row);
}
