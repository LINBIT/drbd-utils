#ifndef STRINGTOKENIZER_H
#define	STRINGTOKENIZER_H

#include <default_types.h>
#include <string>
#include <stdexcept>

class StringTokenizer
{
  public:
    StringTokenizer(const std::string& tokens_line, const std::string& tokens_delimiter);
    StringTokenizer(const StringTokenizer& orig) = default;
    StringTokenizer& operator=(const StringTokenizer& orig) = default;
    StringTokenizer(StringTokenizer&& orig) = default;
    StringTokenizer& operator=(StringTokenizer&& orig) = default;
    virtual ~StringTokenizer() noexcept
    {
    }

    virtual void restart();
    virtual bool has_next();
    virtual bool advance();
    // @throws std::bad_alloc, std::out_of_range
    virtual std::string next();

  private:
    const std::string& line;
    const std::string& delimiter;
    size_t length {0};
    size_t token_offset {0};
    size_t token_length {0};
    size_t index {0};
    bool have_token {false};

    void find_next_token();
};

#endif	/* STRINGTOKENIZER_H */
