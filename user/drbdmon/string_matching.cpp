#include <string_matching.h>

namespace string_matching
{
    const uint16_t  PATTERN_LIMIT   = 20;

    PatternItem::PatternItem(const std::string& init_text):
        text(init_text)
    {
    }

    PatternItem::~PatternItem() noexcept
    {
    }

    PatternLimitException::PatternLimitException()
    {
    }

    PatternLimitException::~PatternLimitException() noexcept
    {
    }

    bool is_pattern(const std::string& pattern)
    {
        return pattern.find('*') != std::string::npos;
    }

    // @throws std::bad_alloc, PatternLimitException
    void process_pattern(const std::string& pattern, std::unique_ptr<PatternItem>& pattern_list)
    {
        std::unique_ptr<PatternItem>*   next_item = &pattern_list;

        // Split pattern
        size_t offset = 0;
        size_t split_idx = 0;
        uint16_t pattern_count = 0;
        do
        {
            split_idx = pattern.find('*', offset);
            size_t length = split_idx != std::string::npos ? split_idx - offset : pattern.length() - offset;
            std::string fragment(pattern.substr(offset, length));
            if (fragment.length() != 0 || offset == 0 || offset == pattern.length())
            {
                ++pattern_count;
                if (pattern_count > PATTERN_LIMIT)
                {
                    throw PatternLimitException();
                }
                (*next_item) = std::unique_ptr<PatternItem>(new PatternItem(fragment));
                PatternItem* const current_item = (*next_item).get();
                next_item = &(current_item->next);
            }
            if (split_idx != std::string::npos)
            {
                offset = split_idx + 1;
            }
        }
        while (split_idx != std::string::npos);
    }

    bool match_text(const std::string& text, const PatternItem* const pattern_list)
    {
        bool matched = true;
        const PatternItem* item = pattern_list;
        if (item != nullptr)
        {
            size_t offset = 0;
            bool anchor_start = true;
            do
            {
                const PatternItem* const next_item = item->next.get();
                const bool anchor_end = next_item == nullptr;
                const size_t idx = anchor_end ? text.rfind(item->text) : text.find(item->text, offset);

                matched = idx != std::string::npos &&
                    (!anchor_start || idx == 0) &&
                    (!anchor_end || idx == text.length() - item->text.length());

                if (matched)
                {
                    offset = idx + item->text.length();
                    anchor_start = false;
                    item = next_item;
                }
            }
            while (matched && item != nullptr);
        }
        return matched;
    }
}
