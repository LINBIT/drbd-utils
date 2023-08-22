#ifndef TEXTCOLUMN_H
#define TEXTCOLUMN_H

#include <default_types.h>
#include <string>
#include <terminal/ComponentsHub.h>

class TextColumn
{
  public:
    static const size_t     MIN_LINE_LENGTH;
    static const size_t     MAX_LINE_LENGTH;
    static const size_t     DFLT_LINE_LENGTH;

    TextColumn(const ComponentsHub& comp_hub);
    virtual ~TextColumn() noexcept;

    virtual void reset();
    virtual void restart();

    virtual void set_text(const std::string* const new_text);
    virtual void set_line_length(const size_t new_length);

    virtual bool skip_line();
    virtual bool next_line(std::string& line, const std::string& text_color);

    virtual size_t get_indent() const;

  private:
    const ComponentsHub&    dsp_comp_hub;

    const std::string*  text            {nullptr};
    const char*         text_chars      {nullptr};

    // Emphasis at the start of the prepared line
    uint8_t             start_emphasis  {0xFF};
    // Emphasis at the end of a prepared line
    // (used as default for the start emphasis when preparing the next line)
    uint8_t             end_emphasis    {0xFF};

    size_t  text_length {0};
    size_t  offset      {0};
    size_t  line_space  {DFLT_LINE_LENGTH};
    size_t  line_start  {0};
    size_t  line_end    {0};
    size_t  indent      {0};
    bool    is_new_line {true};

    bool prepare_line();
};

#endif /* TEXTCOLUMN_H */
