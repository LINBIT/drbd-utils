#include <terminal/InputField.h>
#include <terminal/DisplayIo.h>
#include <terminal/AnsiControl.h>
#include <terminal/KeyCodes.h>
#include <bounds.h>
#include <cstring>

const std::string   InputField::ALLOWED_SPECIAL_CHARS   = " ~`!@#$%^&*()-_=+[];',./{}:\"<>?\\|";

InputField::InputField(const ComponentsHub& dsp_comp_hub_ref, const uint16_t length):
    dsp_comp_hub(dsp_comp_hub_ref)
{
    max_length = std::max(length, static_cast<uint16_t> (1));
    dsp_length = std::min(max_length, dsp_length);
}

InputField::InputField(
    const ComponentsHub&    dsp_comp_hub_ref,
    const uint32_t          coord_column,
    const uint32_t          coord_row,
    const uint16_t          length,
    const uint16_t          field_length
):
    dsp_comp_hub(dsp_comp_hub_ref)
{
    column      = coord_column;
    row         = coord_row;
    max_length  = std::max(length, static_cast<uint16_t> (1));
    dsp_length  = bounds(static_cast<uint16_t> (1), field_length, max_length);
}

InputField::~InputField() noexcept
{
}

void InputField::key_pressed(const uint32_t key)
{
    const size_t text_length = text.length();
    if (key == KeyCodes::DELETE)
    {
        if (cursor_pos < text_length)
        {
            text.erase(cursor_pos, 1);
            update_positions();
        }
    }
    else
    if (key == KeyCodes::BACKSPACE)
    {
        if (cursor_pos >= text_length)
        {
            if (text_length >= 1)
            {
                text.erase(text_length - 1, 1);
                --cursor_pos;
                update_positions();
            }
        }
        else
        if (cursor_pos >= 1)
        {
            text.erase(cursor_pos - 1, 1);
            --cursor_pos;
            update_positions();
        }
    }
    else
    if (key == KeyCodes::ARROW_LEFT)
    {
        if (cursor_pos >= 1)
        {
            --cursor_pos;
            update_positions();
        }
    }
    else
    if (key == KeyCodes::ARROW_RIGHT)
    {
        if (cursor_pos < text_length)
        {
            ++cursor_pos;
            update_positions();
        }
    }
    else
    if (key == KeyCodes::HOME)
    {
        cursor_pos = 0;
        update_positions();
    }
    else
    if (key == KeyCodes::END)
    {
        cursor_pos = text_length;
        update_positions();
    }
    else
    if (key != KeyCodes::NONE && text_length < max_length)
    {
        if ((key >= static_cast<uint32_t> ('a') && key <= static_cast<uint32_t> ('z')) ||
            (key >= static_cast<uint32_t> ('A') && key <= static_cast<uint32_t> ('Z')) ||
            (key >= static_cast<uint32_t> ('0') && key <= static_cast<uint32_t> ('9')))
        {
            if (cursor_pos >= text_length)
            {
                const char key_char = static_cast<char> (key);
                text.append(&key_char, 1);
            }
            else
            {
                text.insert(cursor_pos, 1, static_cast<char> (key));
            }
            ++cursor_pos;
            update_positions();
        }
        else
        {
            const size_t idx_limit = ALLOWED_SPECIAL_CHARS.length();
            for (size_t idx = 0; idx < idx_limit; ++idx)
            {
                if (key == static_cast<uint32_t> (ALLOWED_SPECIAL_CHARS[idx]))
                {
                    if (cursor_pos >= text_length)
                    {
                        const char key_char = static_cast<char> (key);
                        text.append(&key_char, 1);
                    }
                    else
                    {
                        text.insert(cursor_pos, 1, static_cast<char> (key));
                    }
                    ++cursor_pos;
                    update_positions();
                    break;
                }
            }
        }
    }

    display();
    cursor();
}

const std::string& InputField::get_text()
{
    return text;
}

bool InputField::is_empty() const
{
    return text.empty();
}

void InputField::set_text(const std::string& new_text)
{
    if (new_text.length() <= max_length)
    {
        text = new_text;
    }
    else
    {
        text = new_text.substr(0, max_length);
    }
    cursor_pos = text.length();
    update_positions();
}

void InputField::set_text(const char* const new_text)
{
    text = new_text;
    if (text.length() > max_length)
    {
        text = text.substr(0, max_length);
    }
    cursor_pos = text.length();
    update_positions();
}

void InputField::clear_text()
{
    text.clear();
    cursor_pos = 0;
    update_positions();
}

void InputField::display()
{
    dsp_comp_hub.dsp_io->cursor_xy(column, row);
    const size_t text_length = text.length();
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->cmd_input.c_str());
    if (text_length >= 1)
    {
        update_positions();
        std::string dsp_text(
            text.substr(offset, std::min(text_length - offset, static_cast<size_t> (dsp_length)))
        );
        dsp_comp_hub.dsp_io->write_string_field(dsp_text, dsp_length, true);
    }
    else
    {
        cursor_pos = 0;
        update_positions();
        dsp_comp_hub.dsp_io->write_fill_char(' ', dsp_length);
    }
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
}

void InputField::cursor()
{
    update_positions();
    dsp_comp_hub.dsp_io->cursor_xy(column + cursor_pos - offset, row);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CURSOR_ON.c_str());
}

void InputField::set_position(const uint16_t coord_column, const uint16_t coord_row)
{
    column = std::max(static_cast<uint16_t> (1), coord_column);
    row = std::max(static_cast<uint16_t> (1), coord_row);
}

InputField::Position InputField::get_position() const
{
    return {column, row};
}

uint16_t InputField::get_cursor_position()
{
    return cursor_pos;
}

void InputField::set_cursor_position(const uint16_t new_cursor_pos)
{
    cursor_pos = new_cursor_pos;
    update_positions();
}

uint16_t InputField::get_offset()
{
    return offset;
}

void InputField::set_offset(const uint16_t new_offset)
{
    offset = new_offset;
    update_positions();
}

void InputField::set_field_length(const uint16_t field_length)
{
    dsp_length = bounds(static_cast<uint16_t> (1), field_length, max_length);
}

void InputField::update_positions()
{
    const size_t text_length = text.length();
    if (cursor_pos > text_length)
    {
        cursor_pos = text_length;
    }
    if (cursor_pos >= static_cast<uint16_t> (0xFFFE))
    {
        cursor_pos = static_cast<uint16_t> (0xFFFE);
    }
    if (cursor_pos < offset)
    {
        offset = cursor_pos;
    }
    else
    if (cursor_pos >= dsp_length)
    {
        const uint16_t min_offset = (cursor_pos - dsp_length) + 1;
        if (offset < min_offset)
        {
            offset = min_offset;
        }
    }
}

bool InputField::mouse_action(MouseEvent& mouse)
{
    bool intercepted = false;
    if (mouse.coord_row == row && mouse.coord_column >= column && mouse.coord_column < column + dsp_length &&
        mouse.button == MouseEvent::button_id::BUTTON_01 && mouse.event == MouseEvent::event_id::MOUSE_DOWN)
    {
        cursor_pos = mouse.coord_column - column + offset;
        update_positions();
        intercepted = true;
    }
    return intercepted;
}
