#include <integerfmt.h>

namespace integerfmt
{
    void format_thousands_sep(const uint32_t nr, std::string& text)
    {
        uint32_t div = 1000000000ULL;

        uint32_t remainder = nr;

        int ctr = 0;
        uint32_t part = 0;
        while (div >= 10)
        {
            part = (remainder / div) % 10;
            if (part != 0)
            {
                break;
            }
            div = div / 10;
            ctr = (ctr + 1) % 3;
        }
        while (div >= 10)
        {
            text += (static_cast<char> (part) + '0');
            if (ctr % 3 == 0)
            {
                text += ',';
            }
            ctr = (ctr + 1) % 3;
            div = div / 10;
            if (div >= 10)
            {
                part = (remainder / div) % 10;
            }
        }
        part = remainder % 10;
        text += (static_cast<char> (part) + '0');
    }
}
