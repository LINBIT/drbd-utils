#include <terminal/MDspBase.h>
#include <terminal/KeyCodes.h>
#include <terminal/DisplayCommon.h>
#include <terminal/DisplayConsts.h>
#include <terminal/MouseEvent.h>
#include <terminal/InputField.h>
#include <cppdsaext/src/integerparse.h>
#include <cppdsaext/src/dsaext.h>
#include <string_transformations.h>
#include <bounds.h>
#include <algorithm>

MDspBase::MDspBase(const ComponentsHub& comp_hub):
    dsp_comp_hub(comp_hub)
{
}

MDspBase::~MDspBase() noexcept
{
}

void MDspBase::display_activated()
{
    base_input_mode = base_input_mode_type::GLOBAL_KEYS;
}

void MDspBase::display_deactivated()
{
    // Default no-op action; to be overridden by subclasses
}

void MDspBase::display_closed()
{
    // Default no-op action; to be overridden by subclasses
}

void MDspBase::reset_display()
{
    base_input_mode = base_input_mode_type::GLOBAL_KEYS;
}

void MDspBase::notify_data_updated()
{
    // Default no-op action; to be overridden by subclasses
}

void MDspBase::display()
{
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CURSOR_OFF.c_str());
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CLEAR_SCREEN.c_str());
    dsp_comp_hub.dsp_common->application_working();

    dsp_comp_hub.dsp_io->cursor_xy(1, 1);
    dsp_comp_hub.dsp_common->display_header();

    display_content();

    dsp_comp_hub.dsp_common->display_page_numbers(
        base_input_mode == base_input_mode_type::PAGE_NAV,
        base_page_nr,
        base_page_count,
        base_line_offset,
        base_input_page_nr
    );

    if (dsp_comp_hub.dsp_shared->debug_msg.empty())
    {
        dsp_comp_hub.dsp_common->display_std_hotkeys();
    }
    else
    {
        dsp_comp_hub.dsp_io->cursor_xy(1, dsp_comp_hub.term_rows);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
        dsp_comp_hub.dsp_io->write_string_field(
            dsp_comp_hub.dsp_shared->debug_msg, dsp_comp_hub.term_cols - 10, false
        );
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
    }

    dsp_comp_hub.dsp_common->display_command_line();

    // Idle/working must be displayed before the cursor is positioned
    dsp_comp_hub.dsp_common->application_idle();

    reposition_text_cursor();
}

void MDspBase::reposition_text_cursor()
{
    if (base_input_mode == base_input_mode_type::COMMAND)
    {
        dsp_comp_hub.command_line->cursor();
    }
    else
    if (base_input_mode == base_input_mode_type::PAGE_NAV)
    {
        dsp_comp_hub.dsp_common->page_navigation_cursor();
    }
    else
    {
        text_cursor_ops();
    }
}

// To be overridden by subclasses. Do not call this method directly, call reposition_text_cursor instead.
void MDspBase::text_cursor_ops()
{
    // no-op
}

bool MDspBase::key_pressed(const uint32_t key)
{
    bool intercepted = false;
    if (key == KeyCodes::FUNC_06)
    {
        base_enter_page_nav_mode();
        intercepted = true;
    }
    else
    if (key == KeyCodes::FUNC_02)
    {
        dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::MAIN_MENU);
        intercepted = true;
    }
    else
    if (key == KeyCodes::FUNC_10)
    {
        dsp_comp_hub.dsp_selector->leave_display();
        intercepted = true;
    }
    else
    if (base_input_mode == base_input_mode_type::GLOBAL_KEYS)
    {
        if (key == KeyCodes::PG_UP || key == static_cast<uint32_t> ('<'))
        {
            previous_page();
            intercepted = true;
        }
        else
        if (key == KeyCodes::PG_DOWN || key == static_cast<uint32_t> ('>'))
        {
            next_page();
            intercepted = true;
        }
        else
        if (key == KeyCodes::ARROW_UP)
        {
            cursor_to_previous_item();
            intercepted = true;
        }
        else
        if (key == KeyCodes::ARROW_DOWN)
        {
            cursor_to_next_item();
            intercepted = true;
        }
        else
        if (key == KeyCodes::HOME || key == static_cast<uint32_t> ('!'))
        {
            first_page();
            intercepted = true;
        }
        else
        if (key == KeyCodes::END || key == static_cast<uint32_t> ('+'))
        {
            last_page();
            intercepted = true;
        }
        else
        if (key == static_cast<uint32_t> ('/'))
        {
            base_enter_command_line_mode();
            intercepted = true;
        }
    }
    else
    if (base_input_mode == base_input_mode_type::COMMAND)
    {
        const DisplayCommon::command_state_type state = dsp_comp_hub.dsp_common->command_line_key_pressed(key);
        bool exit_cmd_mode = false;
        if (state == DisplayCommon::command_state_type::CMD_LOCAL)
        {
            StringTokenizer tokenizer(dsp_comp_hub.command_line->get_text(), DisplayConsts::CMD_TOKEN_DELIMITER);
            if (tokenizer.has_next())
            {
                std::string keyword(tokenizer.next());
                if (keyword.length() >= 2)
                {
                    keyword.erase(0, 1);
                    std::string upper_keyword = string_transformations::uppercase_copy_of(keyword);
                    exit_cmd_mode = execute_command(upper_keyword, tokenizer);
                }
            }
        }
        else
        if (state != DisplayCommon::command_state_type::INPUT)
        {
            exit_cmd_mode = true;
        }

        if (exit_cmd_mode)
        {
            base_input_mode = base_input_mode_type::GLOBAL_KEYS;
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CURSOR_OFF.c_str());
            dsp_comp_hub.command_line->clear_text();
            dsp_comp_hub.dsp_common->display_command_line();
            leave_command_line_mode();
        }
        else
        {
            dsp_comp_hub.dsp_common->application_idle();
            reposition_text_cursor();
        }

        intercepted = true;
    }
    else
    if (base_input_mode == base_input_mode_type::PAGE_NAV)
    {
        if (key == KeyCodes::ENTER || key == KeyCodes::TAB)
        {
            base_input_mode = base_input_mode_type::GLOBAL_KEYS;
            const uint32_t prev_page_nr = get_page_nr();
            if (base_input_page_nr >= 1)
            {
                set_page_nr(std::min(base_input_page_nr, DisplayConsts::MAX_PAGE_NR));
            }
            if (prev_page_nr == base_page_nr)
            {
                // Update the page number display in case a subclass does not refresh the display
                // because the page number did not change
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CURSOR_OFF.c_str());
                dsp_comp_hub.dsp_common->display_page_numbers(
                    false,
                    base_page_nr,
                    base_page_count,
                    base_line_offset,
                    base_input_page_nr
                );
            }
            leave_page_nav_mode(
                prev_page_nr == base_page_nr ?
                page_change_type::PG_CHG_SAME : page_change_type::PG_CHG_CHANGED
            );
        }
        else
        if (key == KeyCodes::FUNC_12)
        {
            base_input_mode = base_input_mode_type::GLOBAL_KEYS;
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CURSOR_OFF.c_str());
            dsp_comp_hub.dsp_common->display_page_numbers(
                false,
                base_page_nr,
                base_page_count,
                base_line_offset,
                base_input_page_nr
            );
            leave_page_nav_mode(page_change_type::PG_CHG_CANCELED);
        }
        else
        if (key == static_cast<uint32_t> ('/'))
        {
            base_enter_command_line_mode();
        }
        else
        {
            uint32_t updated_page_nr = std::min(base_input_page_nr, DisplayConsts::MAX_PAGE_NR);
            if ((key & 0xFF) == key)
            {
                const char key_char = static_cast<char> (key);
                try
                {
                    const uint32_t input_digit = dsaext::parse_unsigned_int32_c_str(&key_char, 1);
                    if (updated_page_nr <= (DisplayConsts::MAX_PAGE_NR / 10))
                    {
                        const uint32_t scaled_page_nr = updated_page_nr * 10;
                        if (input_digit < DisplayConsts::MAX_PAGE_NR &&
                            DisplayConsts::MAX_PAGE_NR - input_digit >= scaled_page_nr)
                        {
                            updated_page_nr = scaled_page_nr + input_digit;
                        }
                    }
                }
                catch (dsaext::NumberFormatException& ignored)
                {
                }
            }
            else
            if (key == KeyCodes::BACKSPACE || key == KeyCodes::DELETE)
            {
                updated_page_nr = base_input_page_nr / 10;
            }
            base_input_page_nr = updated_page_nr;
            dsp_comp_hub.dsp_common->display_page_numbers(
                true,
                base_page_nr,
                base_page_count,
                base_line_offset,
                base_input_page_nr
            );
            dsp_comp_hub.dsp_common->page_navigation_cursor();
        }
        intercepted = true;
    }
    return intercepted;
}

bool MDspBase::mouse_action(MouseEvent& mouse)
{
    bool intercepted = false;
    if (mouse.button == MouseEvent::button_id::BUTTON_01)
    {
        if (mouse.event == MouseEvent::event_id::MOUSE_RELEASE)
        {
            if (mouse.coord_row == 1 && mouse.coord_column >= dsp_comp_hub.term_cols - 2)
            {
                dsp_comp_hub.dsp_selector->leave_display();
                intercepted = true;
            }
            else
            if (mouse.coord_row == dsp_comp_hub.term_rows - DisplayConsts::CMD_LINE_Y)
            {
                if (base_input_mode != base_input_mode_type::COMMAND)
                {
                    base_enter_command_line_mode();
                }
                intercepted = true;
            }
            else
            if (mouse.coord_row == DisplayConsts::PAGE_NAV_Y &&
                mouse.coord_column >= dsp_comp_hub.term_cols - DisplayConsts::PAGE_NAV_X + 4)
            {
                base_enter_page_nav_mode();
                intercepted = true;
            }
            else
            {
                // Mouse clicked elsewhere, leave page navigation or command line mode,
                // but pass on the click for handling by another class
                base_input_mode = base_input_mode_type::GLOBAL_KEYS;
            }
        }
        else
        if (mouse.coord_row == dsp_comp_hub.term_rows - DisplayConsts::CMD_LINE_Y)
        {
            intercepted = dsp_comp_hub.command_line->mouse_action(mouse);
            reposition_text_cursor();
        }
    }
    else
    if (mouse.event == MouseEvent::event_id::SCROLL_UP || mouse.event == MouseEvent::event_id::SCROLL_DOWN)
    {
        if (mouse.coord_row >= DisplayConsts::PAGE_NAV_Y &&
            mouse.coord_row < (dsp_comp_hub.term_rows - DisplayConsts::CMD_LINE_Y))
        {
            if (mouse.event == MouseEvent::event_id::SCROLL_UP)
            {
                previous_page();
            }
            else
            {
                next_page();
            }
            intercepted = true;
        }
    }
    return intercepted;
}

uint32_t MDspBase::get_page_nr() noexcept
{
    return base_page_nr;
}

uint32_t MDspBase::get_line_offset() noexcept
{
    return base_line_offset;
}

uint32_t MDspBase::get_page_count() noexcept
{
    return base_page_count;
}

void MDspBase::set_page_nr(const uint32_t new_page_nr)
{
    base_page_nr = bounds(
        static_cast<uint32_t> (1),
        new_page_nr,
        (autoscroll ? DisplayConsts::MAX_PAGE_NR : base_page_count)
    );
}

void MDspBase::set_line_offset(const uint32_t new_line_offset)
{
    base_line_offset = new_line_offset;
}

void MDspBase::set_page_count(const uint32_t new_page_count)
{
    base_page_count = bounds(static_cast<uint32_t> (1), new_page_count, DisplayConsts::MAX_PAGE_NR);
}

void MDspBase::first_page()
{
    set_page_nr(1);
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspBase::next_page()
{
    set_page_nr(base_page_nr >= base_page_count ? DisplayConsts::MAX_PAGE_NR : base_page_nr + 1);
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspBase::previous_page()
{
    set_page_nr(std::min(base_page_nr - 1, base_page_count));
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspBase::last_page()
{
    set_page_nr(autoscroll ? DisplayConsts::MAX_PAGE_NR : base_page_count);
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspBase::base_enter_command_line_mode()
{
    base_input_mode = base_input_mode_type::COMMAND;
    dsp_comp_hub.dsp_common->activate_command_line();
    enter_command_line_mode();
    dsp_comp_hub.dsp_common->display_command_line();
    reposition_text_cursor();
}

void MDspBase::base_enter_page_nav_mode()
{
    base_input_page_nr = 0;
    base_input_mode = base_input_mode_type::PAGE_NAV;
    dsp_comp_hub.dsp_common->display_page_numbers(
        true,
        base_page_nr,
        base_page_count,
        base_line_offset,
        base_input_page_nr
    );
    enter_page_nav_mode();
    reposition_text_cursor();
}

bool MDspBase::execute_command(const std::string& command, StringTokenizer& tokenizer)
{
    return false;
}

ClickableCommand* MDspBase::get_command_by_name(
    ClickableCommand::CommandMap&   cmd_map,
    const std::string&              name
)
{
    std::string key = string_transformations::uppercase_copy_of(name);
    return cmd_map.get(&key);
}

ClickableCommand* MDspBase::get_command_by_position(
    ClickableCommand::ClickableMap& click_map,
    const uint16_t                  page_nr,
    MouseEvent&                     mouse
)
{
    ClickableCommand* cmd = nullptr;
    ClickableCommand::Position click_pos(page_nr, mouse.coord_row, mouse.coord_column, 0);
    if (mouse.button == MouseEvent::button_id::BUTTON_01 && mouse.event == MouseEvent::event_id::MOUSE_RELEASE)
    {
        ClickableCommand* const cmd_near_pos = click_map.get_floor_value(&click_pos);
        if (cmd_near_pos != nullptr)
        {
            if (cmd_near_pos->clickable_area.is_click_in_area(page_nr, mouse.coord_row, mouse.coord_column))
            {
                cmd = cmd_near_pos;
            }
        }
    }
    return cmd;
}
