#ifndef COMPARATORS_H
#define	COMPARATORS_H

#include <string>

namespace comparators
{
    int compare_uint16(const uint16_t* key_ptr, const uint16_t* other_ptr);
    int compare_string(const std::string* key_ptr, const std::string* other_ptr);
    int compare_char(const char* key_ptr, const char* other_ptr);
}

#endif	/* COMPARATORS_H */
