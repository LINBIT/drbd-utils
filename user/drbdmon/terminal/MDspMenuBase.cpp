#include <terminal/MDspMenuBase.h>
#include <terminal/KeyCodes.h>
#include <terminal/DisplayUpdateEvent.h>
#include <comparators.h>
#include <dsaext.h>

MDspMenuBase::MDspMenuBase(const ComponentsHub& comp_hub):
    MDspBase::MDspBase(comp_hub)
{
    mb_option_field = std::unique_ptr<InputField>(new InputField(comp_hub, 4));

    cmd_map = std::unique_ptr<ClickableCommand::CommandMap>(
        new ClickableCommand::CommandMap(&comparators::compare_string)
    );

    click_map = std::unique_ptr<ClickableCommand::ClickableMap>(
        new ClickableCommand::ClickableMap(&ClickableCommand::compare_position)
    );
}

MDspMenuBase::~MDspMenuBase() noexcept
{
    cmd_map->clear();
    click_map->clear();
}

void MDspMenuBase::display_option(
    const char* const       key_text,
    const char* const       text,
    const ClickableCommand& cmd,
    const std::string&      text_color
)
{
    dsp_comp_hub.dsp_io->cursor_xy(cmd.clickable_area.start_col, cmd.clickable_area.row);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->option_key.c_str());
    dsp_comp_hub.dsp_io->write_text(key_text);
    dsp_comp_hub.dsp_io->write_text(text_color.c_str());
    dsp_comp_hub.dsp_io->write_text(text);
}

void MDspMenuBase::display_option_query(const uint16_t coord_x, const uint16_t coord_y)
{
    dsp_comp_hub.dsp_io->cursor_xy(coord_x, coord_y);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->option_key.c_str());
    dsp_comp_hub.dsp_io->write_text("Option ");
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_character_table->sym_cmd_arrow.c_str());
    mb_option_field->set_position(coord_x + 9, coord_y);
    mb_option_field->display();
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
}

void MDspMenuBase::display_activated()
{
    MDspBase::display_activated();
    content_focus = true;
}

void MDspMenuBase::display_deactivated()
{
    MDspBase::display_deactivated();
    mb_option_field->clear_text();
}

void MDspMenuBase::display_closed()
{
    MDspBase::display_closed();
    content_focus = true;
    focus_delegated = false;
}

void MDspMenuBase::reset_display()
{
    MDspBase::reset_display();
    content_focus = true;
    focus_delegated = false;
    mb_option_field->clear_text();
}

void MDspMenuBase::synchronize_data()
{
    // no-op
}

void MDspMenuBase::text_cursor_ops()
{
    if (content_focus && !focus_delegated)
    {
        mb_option_field->cursor();
    }
}

void MDspMenuBase::enter_command_line_mode()
{
    content_focus = false;
}

void MDspMenuBase::leave_command_line_mode()
{
    content_focus = true;
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspMenuBase::enter_page_nav_mode()
{
    content_focus = false;
}

void MDspMenuBase::leave_page_nav_mode(const page_change_type change)
{
    content_focus = true;
    if (change == page_change_type::PG_CHG_CHANGED)
    {
        dsp_comp_hub.dsp_selector->refresh_display();
    }
    else
    {
        text_cursor_ops();
    }
}

void MDspMenuBase::cursor_to_next_item()
{
    // no-op
}

void MDspMenuBase::cursor_to_previous_item()
{
    // no-op
}

InputField& MDspMenuBase::get_option_field()
{
    return *mb_option_field;
}

void MDspMenuBase::add_option_command(ClickableCommand& cmd)
{
    try
    {
        cmd_map->insert(&(cmd.command), &cmd);
    }
    catch (dsaext::DuplicateInsertException&)
    {
        // Ignore duplicate inserts
    }
}

void MDspMenuBase::add_option_clickable(ClickableCommand& cmd)
{
    try
    {
        click_map->insert(&(cmd.clickable_area), &cmd);
    }
    catch (dsaext::DuplicateInsertException&)
    {
        // Ignore duplicate inserts
    }
}

void MDspMenuBase::add_option(ClickableCommand& cmd)
{
    add_option_command(cmd);
    add_option_clickable(cmd);
}

void MDspMenuBase::clear_option_commands() noexcept
{
    cmd_map->clear();
}

void MDspMenuBase::clear_option_clickables() noexcept
{
    click_map->clear();
}

void MDspMenuBase::clear_options() noexcept
{
    cmd_map->clear();
    click_map->clear();
}

void MDspMenuBase::delegate_focus(const bool flag)
{
    focus_delegated = flag;
}

bool MDspMenuBase::is_focus_delegated()
{
    return focus_delegated;
}

bool MDspMenuBase::key_pressed(const uint32_t key)
{
    bool intercepted = MDspBase::key_pressed(key);
    if (!intercepted)
    {
        if (content_focus && !focus_delegated)
        {
            if (key == KeyCodes::ENTER)
            {
                const std::string& option_text = mb_option_field->get_text();
                ClickableCommand* const cmd = get_command_by_name(*cmd_map, option_text);
                if (cmd != nullptr)
                {
                    if (cmd->handler_func != nullptr)
                    {
                        (*(cmd->handler_func))();
                    }
                }
                mb_option_field->display();
                mb_option_field->cursor();
                intercepted = true;
            }
            else
            if (key != KeyCodes::FUNC_01)
            {
                mb_option_field->key_pressed(key);
                mb_option_field->display();
                mb_option_field->cursor();
                intercepted = true;
            }
        }
    }
    return intercepted;
}

bool MDspMenuBase::mouse_action(MouseEvent& mouse)
{
    bool intercepted = MDspBase::mouse_action(mouse);
    if (!intercepted)
    {
        if (mouse.button == MouseEvent::button_id::BUTTON_01 && mouse.event == MouseEvent::event_id::MOUSE_RELEASE)
        {
            const uint32_t current_page_nr = get_page_nr();
            ClickableCommand* const cmd = get_command_by_position(*click_map, current_page_nr, mouse);
            if (cmd != nullptr)
            {
                if (cmd->handler_func != nullptr)
                {
                    (*(cmd->handler_func))();
                }
                intercepted = true;
            }
        }
    }
    return intercepted;
}
