#ifndef INTEGERFMT_H
#define INTEGERFMT_H

#include <default_types.h>
#include <string>

namespace integerfmt
{
    void format_thousands_sep(const uint32_t nr, std::string& text);
}

#endif /* INTEGERFMT_H */
