#ifndef STRING_MATCHING_H
#define STRING_MATCHING_H

#include <string>
#include <memory>
#include <stdexcept>
#include <cstdint>

namespace string_matching
{
    extern const uint16_t   PATTERN_LIMIT;

    class PatternItem
    {
      public:
        PatternItem(const std::string& init_text);
        virtual ~PatternItem() noexcept;

        std::string                     text;
        std::unique_ptr<PatternItem>    next;
    };

    class PatternLimitException : public std::exception
    {
      public:
        PatternLimitException();
        virtual ~PatternLimitException() noexcept;
    };

    bool is_pattern(const std::string& pattern);

    // @throws std::bad_alloc, PatternLimitException
    void process_pattern(const std::string& pattern, std::unique_ptr<PatternItem>& pattern_list);

    bool match_text(const std::string& text, const PatternItem* const pattern_list);
}

#endif // STRING_MATCHING_H
