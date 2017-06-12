#include <StringTokenizer.h>

StringTokenizer::StringTokenizer(const std::string& tokens_line, const std::string& tokens_delimiter):
    line(tokens_line),
    delimiter(tokens_delimiter)
{
    length = line.length();

    find_next_token();
}

bool StringTokenizer::has_next()
{
    return have_token;
}

// @throws std::bad_alloc, std::out_of_range
std::string StringTokenizer::next()
{
    if (!have_token)
    {
        throw std::out_of_range("StringTokenizer.next() called, but no more tokens are available");
    }

    // Save current token substring parameters
    size_t cur_offset = token_offset;
    size_t cur_length = token_length;

    // Prepare next token substring parameters
    find_next_token();

    // Return current token
    return line.substr(cur_offset, cur_length);
}

void StringTokenizer::find_next_token()
{
    have_token = false;
    while (!have_token && index < length)
    {
        token_offset = index;
        index = line.find(delimiter, token_offset);
        if (index == std::string::npos)
        {
            index = length;
        }

        token_length = index - token_offset;
        if (token_length >= 1)
        {
            // Found a non-zero length token
            have_token = true;
        }

        if (index < length)
        {
            ++index;
        }
    }
}
