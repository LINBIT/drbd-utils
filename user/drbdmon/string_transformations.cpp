#include <string_transformations.h>

#include <new>
#include <memory>

namespace string_transformations
{
    // @throws std::bad_alloc
    std::string uppercase_copy_of(const std::string& text)
    {
        const size_t text_length = text.length();
        const char* const text_chars = text.c_str();
        std::unique_ptr<char[]> uc_text_chars_mgr(new char[text_length + 1]);
        char* const uc_text_chars = uc_text_chars_mgr.get();
        for (size_t idx = 0; idx < text_length; ++idx)
        {
            const char mask = static_cast<char> (0xDF);
            if (text_chars[idx] >= 'a' && text_chars[idx] <= 'z')
            {
                uc_text_chars[idx] = text_chars[idx] & mask;
            }
            else
            {
                uc_text_chars[idx] = text_chars[idx];
            }
        }
        uc_text_chars[text_length] = '\0';
        std::string uc_text(uc_text_chars);
        return uc_text;
    }

    // @throws std::bad_alloc
    void replace_escape(const std::string& in_text, std::string& out_text)
    {
        const size_t copy_length = in_text.length();
        out_text.clear();
        out_text.reserve(copy_length);
        size_t offset = 0;
        size_t idx = in_text.find(static_cast<char> (0x1B));
        while (idx != std::string::npos)
        {
            out_text.append(in_text.substr(offset, idx - offset));
            offset = idx;
            while (idx < copy_length && in_text[idx] == static_cast<char> (0x1B))
            {
                ++idx;
            }
            out_text.append(idx - offset, '?');
            offset = idx;
            idx = in_text.find(static_cast<char> (0x1B), offset);
        }
        out_text.append(in_text.substr(offset, std::string::npos));
    }
}
