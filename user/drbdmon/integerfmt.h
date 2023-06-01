#ifndef INTEGERFMT_H
#define INTEGERFMT_H

#include <string>
#include <cstdint>

namespace integerfmt
{
    void format_thousands_sep(const uint32_t nr, std::string& text);
}

#endif /* INTEGERFMT_H */
