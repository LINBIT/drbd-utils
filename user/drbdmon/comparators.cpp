#include <comparators.h>

int comparators::compare_string(const std::string* key_ptr, const std::string* other_ptr)
{
    return key_ptr->compare(*other_ptr);
}
