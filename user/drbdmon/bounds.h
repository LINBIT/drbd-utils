#ifndef BOUNDS_H
#define BOUNDS_H

#include <default_types.h>

template<typename T>
T bounds(T min_value, T value, T max_value)
{
    T result = (value >= min_value) ? ((value <= max_value) ? value : max_value) : min_value;
    return result;
}

#endif /* BOUNDS_H */
