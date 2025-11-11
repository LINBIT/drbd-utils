#ifndef STRING_TRANSFORMATIONS_H
#define STRING_TRANSFORMATIONS_H

#include <default_types.h>
#include <string>

namespace string_transformations
{
    extern const char* const DIGIT_CHARS;

    std::string uppercase_copy_of(const std::string& text);
    void replace_escape(const std::string& in_text, std::string& out_text);

    // Sets output to a string representation of value, including thousands separators.
    // If align_right is true, leading spaces are added, and the output string
    // has a fixed length of 26 characters, otherwise, output is left-aligned and the
    // maximum length is 26 characters.
    void format_uint64(const uint64_t value, std::string& output, const bool align_right);
}

#endif /* STRING_TRANSFORMATIONS_H */
