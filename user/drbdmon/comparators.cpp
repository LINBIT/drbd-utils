#include <comparators.h>

int comparators::compare_uint16(const uint16_t* key_ptr, const uint16_t* other_ptr)
{
    int rc = 0;

    int key = *key_ptr;
    int other = *other_ptr;

    if (key < other)
    {
        rc = -1;
    }
    else
    if (key > other)
    {
        rc = 1;
    }

    return rc;
}

int comparators::compare_string(const std::string* key_ptr, const std::string* other_ptr)
{
    return key_ptr->compare(*other_ptr);
}

int comparators::compare_char(const char* key_ptr, const char* other_ptr)
{
    int rc = 0;

    char key = *key_ptr;
    char other = *other_ptr;

    if (key < other)
    {
        rc = -1;
    }
    else
    if (key > other)
    {
        rc = 1;
    }

    return rc;
}
