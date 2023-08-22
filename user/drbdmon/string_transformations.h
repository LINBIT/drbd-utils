#ifndef STRING_TRANSFORMATIONS_H
#define STRING_TRANSFORMATIONS_H

#include <default_types.h>
#include <string>

namespace string_transformations
{
    std::string uppercase_copy_of(const std::string& text);
    void replace_escape(const std::string& in_text, std::string& out_text);
}

#endif /* STRING_TRANSFORMATIONS_H */
