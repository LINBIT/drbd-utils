#ifndef COLORMODES_H
#define COLORMODES_H

#include <default_types.h>

enum class color_mode : uint8_t
{
    DARK_BG_256     = 0,
    DARK_BG_16,
    LIGHT_BG_256,
    LIGHT_BG_16
};

#endif /* COLORMODES_H */
