#include <terminal/TextColumn.h>
#include <bounds.h>

const size_t TextColumn::MIN_LINE_LENGTH    = 40;
const size_t TextColumn::MAX_LINE_LENGTH    = 400;
const size_t TextColumn::DFLT_LINE_LENGTH   = 120;

TextColumn::TextColumn(const ComponentsHub& comp_hub):
    dsp_comp_hub(comp_hub)
{
}

TextColumn::~TextColumn() noexcept
{
}

void TextColumn::reset()
{
    text        = nullptr;
    text_chars  = nullptr;
}

void TextColumn::restart()
{
    offset      = 0;
    indent      = 0;
    line_start  = 0;
    line_end    = 0;
    is_new_line = true;
}

void TextColumn::set_text(const std::string* const new_text)
{
    if (new_text != nullptr)
    {
        text        = new_text;
        text_chars  = text->c_str();
        text_length = text->length();
        offset      = 0;
        indent      = 0;
        line_start  = 0;
        line_end    = 0;
        is_new_line = true;
    }
}

void TextColumn::set_line_length(const size_t new_length)
{
    line_space  = bounds(MIN_LINE_LENGTH, new_length, MAX_LINE_LENGTH);
}

bool TextColumn::skip_line()
{
    return prepare_line();
}

bool TextColumn::next_line(std::string& line, const std::string& text_color)
{
    line.clear();
    const bool have_content = prepare_line();
    if (have_content)
    {
        line.append(indent, ' ');
        if (start_emphasis == 0xFF)
        {
            line.append(text_color);
        }
        else
        {
            if (start_emphasis >= 1 && start_emphasis <= ColorTable::EMP_CLM_TEXT_SIZE)
            {
                line.append(dsp_comp_hub.active_color_table->emp_clm_text[start_emphasis - 1]);
            }
        }

        size_t idx = line_start;
        size_t frag_idx = line_start;
        bool escaped = false;
        while (idx < line_end)
        {
            if (escaped)
            {
                const uint8_t cur_emphasis = static_cast<uint8_t> (text_chars[idx]);
                escaped = false;

                if (cur_emphasis == 0xFF)
                {
                    line.append(dsp_comp_hub.active_color_table->rst);
                    line.append(text_color);
                }
                else
                if (cur_emphasis >= 1 && cur_emphasis <= ColorTable::EMP_CLM_TEXT_SIZE)
                {
                    line.append(dsp_comp_hub.active_color_table->emp_clm_text[cur_emphasis - 1]);
                }
            }
            else
            if (text_chars[idx] == '\x1B')
            {
                line.append(*text, frag_idx, idx - frag_idx);
                frag_idx = (line_end - idx >= 2 ? idx + 2 : line_end);
                escaped = true;
            }
            ++idx;
        }

        if (frag_idx < line_end)
        {
            line.append(*text, frag_idx, line_end - frag_idx);
        }
    }
    return have_content;
}

bool TextColumn::prepare_line()
{
    bool have_content = false;
    if (text_chars != nullptr)
    {
        start_emphasis = end_emphasis;
        if (is_new_line)
        {
            // Remember a limited indent, discard leading space
            bool escaped = false;
            size_t new_indent = 0;
            line_start = offset;
            while (offset < text_length && text_chars[offset] == ' ')
            {
                if (escaped)
                {
                    // Set emphasis for the current line and remember emphasis for the next line
                    start_emphasis = static_cast<uint8_t> (text_chars[offset]);
                    end_emphasis = start_emphasis;
                    escaped = false;
                }
                if (text_chars[offset] == ' ')
                {
                    ++new_indent;
                }
                else
                if (text_chars[offset] == '\x1B')
                {
                    escaped = true;
                }
                else
                {
                    break;
                }
                ++offset;
            }
            indent = std::min(new_indent, line_space  >> 1);
            is_new_line = false;
        }

        const size_t remain_length = line_space  - indent;
        size_t cur_length = 0;
        bool have_space = false;
        bool ign_space = false;
        bool escaped = false;
        // Emphasis at the position of the last space
        uint8_t saved_emphasis = end_emphasis;

        have_content = offset < text_length;
        line_start = offset;
        size_t line_length = 0;
        // Search one further than the maximum line length to detect trailing space & newline
        while (offset < text_length && cur_length <= remain_length)
        {
            // Store the location of the first space after the last word
            if (escaped)
            {
                end_emphasis = static_cast<uint8_t> (text_chars[offset]);
                escaped = false;
            }
            else
            if (text_chars[offset] == ' ')
            {
                if (!ign_space)
                {
                    saved_emphasis = end_emphasis;
                    line_length = cur_length;
                    line_end = offset;
                    have_space = true;
                }
                ign_space = true;
                ++cur_length;
            }
            else
            if (text_chars[offset] == '\n')
            {
                line_length = cur_length;
                line_end = offset;
                is_new_line = true;
                ++offset;
                break;
            }
            else
            if (text_chars[offset] == '\x1B')
            {
                escaped = true;
            }
            else
            {
                ign_space = false;
                ++cur_length;
            }
            ++offset;
        }

        if (cur_length > remain_length)
        {
            // Line ended due to the length limit
            if (!have_space || line_length < (line_space >> 2))
            {
                // Line break potentially in the middle of a word, due to
                // minimum line length constraints
                // Search forward from line_start by remain_length characters,
                // ignoring escape sequences
                escaped = false;
                line_end = line_start;
                size_t idx = 0;
                while (line_end < text_length && idx < remain_length)
                {
                    if (escaped)
                    {
                        escaped = false;
                    }
                    else
                    if (text_chars[line_end] == '\x1B')
                    {
                        escaped = true;
                    }
                    else
                    {
                        ++idx;
                    }
                    ++line_end;
                }
            }
            else
            {
                // Line ended at a saved position between words, use saved emphasis
                end_emphasis = saved_emphasis;
            }
            // Reset cursor to the current line end
            offset = line_end;
        }
        else
        if (offset >= text_length)
        {
            line_end = offset;
        }

        if (!is_new_line)
        {
            // Not at the beginning of a new line, discard trailing space, but save any emphasis found
            escaped = false;
            while (offset < text_length)
            {
                if (escaped)
                {
                    end_emphasis = static_cast<uint8_t> (text_chars[offset]);
                    escaped = false;
                }
                else
                if (text_chars[offset] == '\x1B')
                {
                    escaped = true;
                }
                else
                if (text_chars[offset] != ' ')
                {
                    break;
                }
                ++offset;
            }
        }
    }
    return have_content;
}

size_t TextColumn::get_indent() const
{
    return indent;
}
