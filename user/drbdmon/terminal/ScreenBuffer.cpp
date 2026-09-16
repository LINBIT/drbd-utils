#include <terminal/ScreenBuffer.h>

#include <algorithm>
#include <cstring>
#include <bounds.h>

ScreenBuffer::ScreenBuffer(const uint16_t init_cols, const uint16_t init_rows):
    cols(bounds(MIN_DIMENSION, init_cols, MAX_DIMENSION)),
    rows(bounds(MIN_DIMENSION, init_rows, MAX_DIMENSION))
{
    const size_t cell_count = static_cast<size_t> (cols) * static_cast<size_t> (rows);
    back_buffer.resize(cell_count);
    front_buffer.resize(cell_count);
    csi_seq.reserve(MAX_CSI_LENGTH);
}

ScreenBuffer::~ScreenBuffer() noexcept
{
}

void ScreenBuffer::set_dimensions(const uint16_t new_cols, const uint16_t new_rows)
{
    const uint16_t adj_cols = bounds(MIN_DIMENSION, new_cols, MAX_DIMENSION);
    const uint16_t adj_rows = bounds(MIN_DIMENSION, new_rows, MAX_DIMENSION);
    if (adj_cols != cols || adj_rows != rows)
    {
        cols = adj_cols;
        rows = adj_rows;
        const size_t cell_count = static_cast<size_t> (cols) * static_cast<size_t> (rows);
        back_buffer.resize(cell_count);
        front_buffer.resize(cell_count);
    }
    clear_buffers();
    invalidate();
}

void ScreenBuffer::invalidate() noexcept
{
    full_update = true;
}

void ScreenBuffer::clear_buffers() noexcept
{
    const Cell blank_cell;
    std::fill(back_buffer.begin(), back_buffer.end(), blank_cell);
    std::fill(front_buffer.begin(), front_buffer.end(), blank_cell);
    cursor_col = 0;
    cursor_row = 0;
    cur_attr = Attributes();
    state = parser_state::TEXT;
    csi_seq.clear();
    utf8_length = 0;
    utf8_expected = 0;
}

void ScreenBuffer::apply(const char* const data, const size_t length) noexcept
{
    for (size_t idx = 0; idx < length; ++idx)
    {
        apply_byte(data[idx]);
    }
}

void ScreenBuffer::apply_byte(const char byte) noexcept
{
    const unsigned char value = static_cast<unsigned char> (byte);
    switch (state)
    {
        case parser_state::TEXT:
        {
            if (value == 0x1B)
            {
                utf8_length = 0;
                utf8_expected = 0;
                state = parser_state::ESCAPE;
            }
            else
            if (utf8_expected >= 1)
            {
                if ((value & 0xC0) == 0x80)
                {
                    utf8_seq[utf8_length] = byte;
                    ++utf8_length;
                    if (utf8_length >= utf8_expected)
                    {
                        apply_char(utf8_seq, utf8_length);
                        utf8_length = 0;
                        utf8_expected = 0;
                    }
                }
                else
                {
                    // Invalid UTF-8 sequence, discard it and reprocess the current byte
                    utf8_length = 0;
                    utf8_expected = 0;
                    apply_byte(byte);
                }
            }
            else
            if (value >= 0xC0 && value <= 0xF7)
            {
                utf8_seq[0] = byte;
                utf8_length = 1;
                utf8_expected = value >= 0xF0 ? 4 : (value >= 0xE0 ? 3 : 2);
            }
            else
            if (value == '\n')
            {
                // The terminal's output post-processing maps this to CR LF
                cursor_col = 0;
                if (cursor_row + 1 < rows)
                {
                    ++cursor_row;
                }
            }
            else
            if (value == '\r')
            {
                cursor_col = 0;
            }
            else
            if (value == '\b')
            {
                if (cursor_col >= 1)
                {
                    --cursor_col;
                }
            }
            else
            if (value == '\t')
            {
                const uint16_t next_col = static_cast<uint16_t> ((cursor_col & ~static_cast<uint16_t> (7)) + 8);
                cursor_col = std::min(next_col, static_cast<uint16_t> (cols - 1));
            }
            else
            if (value >= 0x20 && value <= 0x7E)
            {
                apply_char(&byte, 1);
            }
            // Any other control character is ignored
            break;
        }
        case parser_state::ESCAPE:
        {
            if (value == '[')
            {
                csi_seq.clear();
                state = parser_state::CSI;
            }
            else
            if (value == ']')
            {
                state = parser_state::OSC;
            }
            else
            {
                state = parser_state::TEXT;
            }
            break;
        }
        case parser_state::CSI:
        {
            if (value >= 0x40 && value <= 0x7E)
            {
                apply_csi(byte);
                state = parser_state::TEXT;
            }
            else
            {
                if (csi_seq.length() < MAX_CSI_LENGTH)
                {
                    csi_seq.push_back(byte);
                }
                else
                {
                    // Not a valid CSI sequence, stop interpreting it
                    state = parser_state::TEXT;
                }
            }
            break;
        }
        case parser_state::OSC:
        {
            if (value == 0x07)
            {
                state = parser_state::TEXT;
            }
            else
            if (value == 0x1B)
            {
                state = parser_state::OSC_ESCAPE;
            }
            break;
        }
        case parser_state::OSC_ESCAPE:
            // fall-through
        default:
        {
            // The string terminator is ESC \, anything else continues the OSC sequence
            state = value == '\\' ? parser_state::TEXT : parser_state::OSC;
            break;
        }
    }
}

uint8_t ScreenBuffer::char_width(const char* const text, const uint8_t text_length) noexcept
{
    // Decode the UTF-8 sequence
    uint32_t code_point = 0;
    if (text_length == 1)
    {
        return 1;
    }
    else
    if (text_length == 2)
    {
        code_point = static_cast<uint32_t> (text[0] & 0x1F) << 6;
    }
    else
    if (text_length == 3)
    {
        code_point = static_cast<uint32_t> (text[0] & 0x0F) << 12;
    }
    else
    if (text_length == 4)
    {
        code_point = static_cast<uint32_t> (text[0] & 0x07) << 18;
    }
    else
    {
        return 1;
    }
    for (uint8_t idx = 1; idx < text_length; ++idx)
    {
        code_point |= static_cast<uint32_t> (text[idx] & 0x3F) << ((text_length - idx - 1) * 6);
    }

    // Combining characters are displayed as part of the preceding character
    static const uint32_t ZERO_WIDTH[][2] =
    {
        {0x0300, 0x036F}, {0x0483, 0x0489}, {0x0591, 0x05BD}, {0x0610, 0x061A},
        {0x064B, 0x065F}, {0x0670, 0x0670}, {0x06D6, 0x06DC}, {0x0730, 0x074A},
        {0x07EB, 0x07F3}, {0x0E31, 0x0E31}, {0x0E34, 0x0E3A}, {0x0EB1, 0x0EB1},
        {0x1AB0, 0x1AFF}, {0x1DC0, 0x1DFF}, {0x200B, 0x200F}, {0x20D0, 0x20F0},
        {0xFE00, 0xFE0F}, {0xFE20, 0xFE2F}
    };
    for (const uint32_t* const range : ZERO_WIDTH)
    {
        if (code_point >= range[0] && code_point <= range[1])
        {
            return 0;
        }
    }

    // Characters that the terminal displays in two columns
    static const uint32_t DOUBLE_WIDTH[][2] =
    {
        {0x1100, 0x115F}, {0x231A, 0x231B}, {0x2329, 0x232A}, {0x23E9, 0x23EC},
        {0x23F0, 0x23F0}, {0x23F3, 0x23F3}, {0x25FD, 0x25FE}, {0x2614, 0x2615},
        {0x2648, 0x2653}, {0x267F, 0x267F}, {0x2693, 0x2693}, {0x26A1, 0x26A1},
        {0x26AA, 0x26AB}, {0x26BD, 0x26BE}, {0x26C4, 0x26C5}, {0x26CE, 0x26CE},
        {0x26D4, 0x26D4}, {0x26EA, 0x26EA}, {0x26F2, 0x26F3}, {0x26F5, 0x26F5},
        {0x26FA, 0x26FA}, {0x26FD, 0x26FD}, {0x2705, 0x2705}, {0x270A, 0x270B},
        {0x2728, 0x2728}, {0x274C, 0x274C}, {0x274E, 0x274E}, {0x2753, 0x2755},
        {0x2757, 0x2757}, {0x2795, 0x2797}, {0x27B0, 0x27B0}, {0x27BF, 0x27BF},
        {0x2B1B, 0x2B1C}, {0x2B50, 0x2B50}, {0x2B55, 0x2B55}, {0x2E80, 0x303E},
        {0x3041, 0x33FF}, {0x3400, 0x4DBF}, {0x4E00, 0x9FFF}, {0xA000, 0xA4CF},
        {0xA960, 0xA97F}, {0xAC00, 0xD7A3}, {0xF900, 0xFAFF}, {0xFE10, 0xFE19},
        {0xFE30, 0xFE6F}, {0xFF00, 0xFF60}, {0xFFE0, 0xFFE6},
        {0x16FE0, 0x16FE4}, {0x17000, 0x18AFF}, {0x1B000, 0x1B2FF},
        {0x1F004, 0x1F004}, {0x1F0CF, 0x1F0CF}, {0x1F18E, 0x1F18E},
        {0x1F191, 0x1F19A}, {0x1F200, 0x1F320}, {0x1F32D, 0x1F335},
        {0x1F337, 0x1F37C}, {0x1F37E, 0x1F393}, {0x1F3A0, 0x1F3CA},
        {0x1F3CF, 0x1F3D3}, {0x1F3E0, 0x1F3F0}, {0x1F3F4, 0x1F3F4},
        {0x1F3F8, 0x1F43E}, {0x1F440, 0x1F440}, {0x1F442, 0x1F4FC},
        {0x1F4FF, 0x1F53D}, {0x1F54B, 0x1F54E}, {0x1F550, 0x1F567},
        {0x1F57A, 0x1F57A}, {0x1F595, 0x1F596}, {0x1F5A4, 0x1F5A4},
        {0x1F5FB, 0x1F64F}, {0x1F680, 0x1F6C5}, {0x1F6CC, 0x1F6CC},
        {0x1F6D0, 0x1F6D2}, {0x1F6EB, 0x1F6EC}, {0x1F6F4, 0x1F6FA},
        {0x1F7E0, 0x1F7EB}, {0x1F90D, 0x1F971}, {0x1F973, 0x1F976},
        {0x1F97A, 0x1F9A2}, {0x1F9A5, 0x1F9AA}, {0x1F9AE, 0x1F9CA},
        {0x1F9CD, 0x1F9FF}, {0x20000, 0x2FFFD}, {0x30000, 0x3FFFD}
    };
    for (const uint32_t* const range : DOUBLE_WIDTH)
    {
        if (code_point >= range[0] && code_point <= range[1])
        {
            return 2;
        }
    }

    return 1;
}

void ScreenBuffer::apply_char(const char* const text, const uint8_t text_length) noexcept
{
    const uint8_t width = char_width(text, text_length);
    if (width == 0)
    {
        // A combining character is displayed as part of the preceding character, which this class
        // does not model. Discarding it keeps the buffer's idea of the cursor position correct.
        return;
    }

    if (cursor_col < cols && cursor_row < rows)
    {
        const size_t row_start = static_cast<size_t> (cursor_row) * static_cast<size_t> (cols);
        Cell& cell = back_buffer[row_start + cursor_col];
        const uint8_t safe_length = std::min(text_length, static_cast<uint8_t> (MAX_CHAR_LENGTH));
        std::memcpy(cell.text, text, safe_length);
        std::memset(&(cell.text[safe_length]), 0, MAX_CHAR_LENGTH - safe_length);
        cell.text_length = safe_length;
        cell.fg = cur_attr.fg;
        cell.bg = cur_attr.bg;
        cell.flags = cur_attr.flags;

        if (width == 2 && cursor_col + 1 < cols)
        {
            // The right half of a double width character does not generate any output of its own
            Cell& cont_cell = back_buffer[row_start + cursor_col + 1];
            cont_cell = Cell();
            cont_cell.text_length = 0;
            cont_cell.fg = cur_attr.fg;
            cont_cell.bg = cur_attr.bg;
            cont_cell.flags = cur_attr.flags;
        }
    }

    // Output beyond the last column of a row is discarded rather than wrapped to the next row,
    // so that a display that overruns the width of the terminal cannot shift the entire screen
    const uint16_t remaining = static_cast<uint16_t> (cursor_col < cols ? cols - cursor_col : 0);
    cursor_col = static_cast<uint16_t> (cursor_col + std::min(static_cast<uint16_t> (width), remaining));
}

void ScreenBuffer::parse_params(std::vector<uint32_t>& params, size_t& prefix_length) const
{
    prefix_length = 0;
    while (prefix_length < csi_seq.length() && csi_seq[prefix_length] >= 0x3C && csi_seq[prefix_length] <= 0x3F)
    {
        ++prefix_length;
    }

    uint32_t value = 0;
    bool have_value = false;
    for (size_t idx = prefix_length; idx < csi_seq.length(); ++idx)
    {
        const char cur_char = csi_seq[idx];
        if (cur_char >= '0' && cur_char <= '9')
        {
            if (value <= (UINT32_MAX - 9) / 10)
            {
                value = (value * 10) + static_cast<uint32_t> (cur_char - '0');
            }
            have_value = true;
        }
        else
        if (cur_char == ';' || cur_char == ':')
        {
            params.push_back(value);
            value = 0;
            have_value = false;
        }
        // Any other byte is ignored
    }
    if (have_value || !params.empty())
    {
        params.push_back(value);
    }
}

void ScreenBuffer::apply_csi(const char final_byte) noexcept
{
    try
    {
        std::vector<uint32_t> params;
        size_t prefix_length = 0;
        parse_params(params, prefix_length);

        const uint32_t param_1 = params.empty() ? 0 : params[0];
        const uint32_t param_2 = params.size() >= 2 ? params[1] : 0;
        const bool private_seq = prefix_length >= 1;

        switch (final_byte)
        {
            case 'f':
                // fall-through
            case 'H':
            {
                // Cursor position, 1-based, a missing or zero parameter selects the first row/column
                const uint32_t row_nr = param_1 >= 1 ? param_1 - 1 : 0;
                const uint32_t col_nr = param_2 >= 1 ? param_2 - 1 : 0;
                cursor_row = static_cast<uint16_t> (std::min(row_nr, static_cast<uint32_t> (rows - 1)));
                cursor_col = static_cast<uint16_t> (std::min(col_nr, static_cast<uint32_t> (cols - 1)));
                break;
            }
            case 'A':
            {
                const uint16_t distance = static_cast<uint16_t> (std::min(std::max(param_1, 1u),
                                                                          static_cast<uint32_t> (cursor_row)));
                cursor_row -= distance;
                break;
            }
            case 'B':
            {
                const uint32_t distance = std::max(param_1, 1u);
                cursor_row = static_cast<uint16_t> (std::min(static_cast<uint32_t> (cursor_row) + distance,
                                                             static_cast<uint32_t> (rows - 1)));
                break;
            }
            case 'C':
            {
                const uint32_t distance = std::max(param_1, 1u);
                cursor_col = static_cast<uint16_t> (std::min(static_cast<uint32_t> (cursor_col) + distance,
                                                             static_cast<uint32_t> (cols - 1)));
                break;
            }
            case 'D':
            {
                const uint16_t distance = static_cast<uint16_t> (std::min(std::max(param_1, 1u),
                                                                          static_cast<uint32_t> (cursor_col)));
                cursor_col -= distance;
                break;
            }
            case 'K':
            {
                // Erase in line
                const size_t row_start = static_cast<size_t> (cursor_row) * static_cast<size_t> (cols);
                const size_t row_end = row_start + cols;
                if (param_1 == 1)
                {
                    erase_cells(row_start, row_start + cursor_col + 1);
                }
                else
                if (param_1 == 2)
                {
                    erase_cells(row_start, row_end);
                }
                else
                {
                    erase_cells(row_start + cursor_col, row_end);
                }
                break;
            }
            case 'J':
            {
                // Erase in display
                const size_t cursor_index = static_cast<size_t> (cursor_row) * static_cast<size_t> (cols) +
                                            cursor_col;
                if (param_1 == 1)
                {
                    erase_cells(0, cursor_index + 1);
                }
                else
                if (param_1 == 2 || param_1 == 3)
                {
                    erase_cells(0, back_buffer.size());
                }
                else
                {
                    erase_cells(cursor_index, back_buffer.size());
                }
                break;
            }
            case 'm':
            {
                if (!private_seq)
                {
                    apply_sgr(params);
                }
                break;
            }
            case 'h':
                // fall-through
            case 'l':
            {
                // Cursor visibility, all other modes are display independent and are not tracked
                if (private_seq && param_1 == 25)
                {
                    cursor_visible = final_byte == 'h';
                }
                break;
            }
            default:
            {
                // Sequences that do not affect the contents of the screen are ignored
                break;
            }
        }
    }
    catch (std::bad_alloc&)
    {
        // Out of memory while parsing the parameters, ignore the sequence and
        // update the entire terminal on the next render() call
        full_update = true;
    }
}

void ScreenBuffer::apply_sgr(const std::vector<uint32_t>& params) noexcept
{
    if (params.empty())
    {
        cur_attr = Attributes();
        return;
    }

    for (size_t idx = 0; idx < params.size(); ++idx)
    {
        const uint32_t param = params[idx];
        if (param == 0)
        {
            cur_attr = Attributes();
        }
        else
        if (param == 1)
        {
            cur_attr.flags |= FLAG_BOLD;
        }
        else
        if (param == 2)
        {
            cur_attr.flags |= FLAG_DIM;
        }
        else
        if (param == 4)
        {
            cur_attr.flags |= FLAG_UNDERLINE;
        }
        else
        if (param == 5)
        {
            cur_attr.flags |= FLAG_BLINK;
        }
        else
        if (param == 7)
        {
            cur_attr.flags |= FLAG_REVERSE;
        }
        else
        if (param == 22)
        {
            cur_attr.flags &= static_cast<uint8_t> (~(FLAG_BOLD | FLAG_DIM));
        }
        else
        if (param == 24)
        {
            cur_attr.flags &= static_cast<uint8_t> (~FLAG_UNDERLINE);
        }
        else
        if (param == 25)
        {
            cur_attr.flags &= static_cast<uint8_t> (~FLAG_BLINK);
        }
        else
        if (param == 27)
        {
            cur_attr.flags &= static_cast<uint8_t> (~FLAG_REVERSE);
        }
        else
        if (param >= 30 && param <= 37)
        {
            cur_attr.fg = COLOR_INDEXED | (param - 30);
        }
        else
        if (param == 39)
        {
            cur_attr.fg = COLOR_DEFAULT;
        }
        else
        if (param >= 40 && param <= 47)
        {
            cur_attr.bg = COLOR_INDEXED | (param - 40);
        }
        else
        if (param == 49)
        {
            cur_attr.bg = COLOR_DEFAULT;
        }
        else
        if (param >= 90 && param <= 97)
        {
            cur_attr.fg = COLOR_INDEXED | (param - 90 + 8);
        }
        else
        if (param >= 100 && param <= 107)
        {
            cur_attr.bg = COLOR_INDEXED | (param - 100 + 8);
        }
        else
        if (param == 38 || param == 48)
        {
            uint32_t& color = param == 38 ? cur_attr.fg : cur_attr.bg;
            if (idx + 2 < params.size() && params[idx + 1] == 5)
            {
                color = COLOR_INDEXED | (params[idx + 2] & 0xFF);
                idx += 2;
            }
            else
            if (idx + 4 < params.size() && params[idx + 1] == 2)
            {
                color = COLOR_RGB |
                        ((params[idx + 2] & 0xFF) << 16) |
                        ((params[idx + 3] & 0xFF) << 8) |
                        (params[idx + 4] & 0xFF);
                idx += 4;
            }
        }
        // Any other parameter does not affect the contents of the screen and is ignored
    }
}

void ScreenBuffer::erase_cells(const size_t start_index, const size_t end_index) noexcept
{
    // Erasing uses the current background color, matching the terminal's background color erase
    Cell erase_cell;
    erase_cell.bg = cur_attr.bg;
    const size_t safe_end = std::min(end_index, back_buffer.size());
    for (size_t index = start_index; index < safe_end; ++index)
    {
        back_buffer[index] = erase_cell;
    }
}

bool ScreenBuffer::equal_cells(const Cell& cell, const Cell& other) noexcept
{
    return cell.text_length == other.text_length &&
           cell.fg == other.fg &&
           cell.bg == other.bg &&
           cell.flags == other.flags &&
           std::memcmp(cell.text, other.text, cell.text_length) == 0;
}

void ScreenBuffer::append_text(std::vector<char>& out_data, const char* const text)
{
    out_data.insert(out_data.end(), text, text + std::strlen(text));
}

void ScreenBuffer::append_number(std::vector<char>& out_data, const uint32_t number)
{
    char digits[10];
    size_t digit_count = 0;
    uint32_t value = number;
    do
    {
        digits[digit_count] = static_cast<char> ('0' + (value % 10));
        ++digit_count;
        value /= 10;
    }
    while (value != 0);
    while (digit_count >= 1)
    {
        --digit_count;
        out_data.push_back(digits[digit_count]);
    }
}

void ScreenBuffer::append_cursor_pos(std::vector<char>& out_data, const uint16_t col, const uint16_t row)
{
    append_text(out_data, "\x1B[");
    append_number(out_data, static_cast<uint32_t> (row) + 1);
    out_data.push_back(';');
    append_number(out_data, static_cast<uint32_t> (col) + 1);
    out_data.push_back('f');
}

void ScreenBuffer::append_color(std::vector<char>& out_data, const uint32_t color, const bool foreground)
{
    const uint32_t color_type = color & ~COLOR_VALUE_MASK;
    const uint32_t color_value = color & COLOR_VALUE_MASK;
    if (color_type == COLOR_INDEXED)
    {
        append_number(out_data, foreground ? 38 : 48);
        append_text(out_data, ";5;");
        append_number(out_data, color_value);
    }
    else
    if (color_type == COLOR_RGB)
    {
        append_number(out_data, foreground ? 38 : 48);
        append_text(out_data, ";2;");
        append_number(out_data, (color_value >> 16) & 0xFF);
        out_data.push_back(';');
        append_number(out_data, (color_value >> 8) & 0xFF);
        out_data.push_back(';');
        append_number(out_data, color_value & 0xFF);
    }
    else
    {
        append_number(out_data, foreground ? 39 : 49);
    }
}

void ScreenBuffer::append_attributes(std::vector<char>& out_data, const Cell& cell)
{
    if (cell.fg == out_attr.fg && cell.bg == out_attr.bg && cell.flags == out_attr.flags)
    {
        return;
    }

    // Attributes that are set in out_attr but not in the cell can only be cleared by a reset,
    // therefore the attributes of the cell are always applied to a known, reset state
    append_text(out_data, "\x1B[0");
    if ((cell.flags & FLAG_BOLD) != 0)
    {
        append_text(out_data, ";1");
    }
    if ((cell.flags & FLAG_DIM) != 0)
    {
        append_text(out_data, ";2");
    }
    if ((cell.flags & FLAG_UNDERLINE) != 0)
    {
        append_text(out_data, ";4");
    }
    if ((cell.flags & FLAG_BLINK) != 0)
    {
        append_text(out_data, ";5");
    }
    if ((cell.flags & FLAG_REVERSE) != 0)
    {
        append_text(out_data, ";7");
    }
    if (cell.fg != COLOR_DEFAULT)
    {
        out_data.push_back(';');
        append_color(out_data, cell.fg, true);
    }
    if (cell.bg != COLOR_DEFAULT)
    {
        out_data.push_back(';');
        append_color(out_data, cell.bg, false);
    }
    out_data.push_back('m');

    out_attr.fg = cell.fg;
    out_attr.bg = cell.bg;
    out_attr.flags = cell.flags;
}

void ScreenBuffer::render(std::vector<char>& out_data)
{
    const size_t start_size = out_data.size();
    bool have_update = false;

    // The cursor is hidden while the terminal is updated, so that it does not flicker
    // across the screen while the update is applied
    append_text(out_data, "\x1B[?25l");

    if (full_update)
    {
        // The contents of the terminal are unknown, reset the attributes and clear the screen,
        // so that everything that is outside of the screen buffer is cleared as well.
        // The terminal displays blank cells afterwards, therefore the update is generated by
        // comparing to a blank front buffer, which avoids writing out the blank cells again.
        append_text(out_data, "\x1B[0m\x1B[H\x1B[2J");
        const Cell blank_cell;
        std::fill(front_buffer.begin(), front_buffer.end(), blank_cell);
        have_update = true;
    }
    out_attr = Attributes();

    for (uint16_t row_idx = 0; row_idx < rows; ++row_idx)
    {
        const size_t row_start = static_cast<size_t> (row_idx) * static_cast<size_t> (cols);
        uint16_t col_idx = 0;
        while (col_idx < cols)
        {
            if (equal_cells(back_buffer[row_start + col_idx], front_buffer[row_start + col_idx]))
            {
                ++col_idx;
                continue;
            }

            // Collect a run of changed cells. Runs that are separated by no more than MAX_SKIP_LENGTH
            // unchanged cells are combined, because rewriting a few unchanged cells costs less than
            // the cursor positioning sequence that would be required to skip them.
            uint16_t run_end = col_idx;
            uint16_t skip_count = 0;
            for (uint16_t scan_idx = col_idx; scan_idx < cols; ++scan_idx)
            {
                if (!equal_cells(back_buffer[row_start + scan_idx], front_buffer[row_start + scan_idx]))
                {
                    run_end = static_cast<uint16_t> (scan_idx + 1);
                    skip_count = 0;
                }
                else
                {
                    ++skip_count;
                    if (skip_count > MAX_SKIP_LENGTH)
                    {
                        break;
                    }
                }
            }

            // A run must start at the left half of a double width character, otherwise the terminal
            // would display the character's left half only partially overwritten
            if (back_buffer[row_start + col_idx].text_length == 0 && col_idx >= 1)
            {
                --col_idx;
            }

            append_cursor_pos(out_data, col_idx, row_idx);
            have_update = true;
            for (uint16_t write_idx = col_idx; write_idx < run_end; ++write_idx)
            {
                const Cell& cell = back_buffer[row_start + write_idx];
                if (cell.text_length == 0)
                {
                    // The preceding double width character already advanced the terminal's cursor
                    // across this cell
                    continue;
                }
                append_attributes(out_data, cell);
                out_data.insert(out_data.end(), cell.text, cell.text + cell.text_length);
            }

            col_idx = run_end;
        }
    }

    if (!have_update &&
        cursor_col == out_cursor_col && cursor_row == out_cursor_row &&
        cursor_visible == out_cursor_visible)
    {
        // Nothing changed, do not send anything to the terminal at all
        out_data.resize(start_size);
        return;
    }

    append_text(out_data, "\x1B[0m");
    append_cursor_pos(out_data, cursor_col, cursor_row);
    if (cursor_visible)
    {
        append_text(out_data, "\x1B[?25h");
    }

    out_cursor_col = cursor_col;
    out_cursor_row = cursor_row;
    out_cursor_visible = cursor_visible;

    front_buffer = back_buffer;
    full_update = false;
}
