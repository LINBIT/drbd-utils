#ifndef COMPARATORS_H
#define	COMPARATORS_H

#include <string>

namespace comparators
{
    int compare_string(const std::string* key_ptr, const std::string* other_ptr);

    template<typename T>
    int compare(const T* const key_ptr, const T* const other_ptr)
    {
        int result = 0;
        if (*key_ptr < *other_ptr)
        {
            result = -1;
        }
        else
        if (*key_ptr > *other_ptr)
        {
            result = 1;
        }
        return result;
    }
}

#endif	/* COMPARATORS_H */
