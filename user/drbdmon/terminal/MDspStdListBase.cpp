#include <terminal/MDspStdListBase.h>
#include <terminal/KeyCodes.h>
#include <terminal/DisplayConsts.h>

MDspStdListBase::MDspStdListBase(const ComponentsHub& comp_hub):
    MDspBase::MDspBase(comp_hub)
{
    autoscroll = true;
}

MDspStdListBase::~MDspStdListBase() noexcept
{
}

void MDspStdListBase::display_content()
{
    display_list();

    dsp_comp_hub.dsp_common->display_selection_mode_label(is_selecting());
    dsp_comp_hub.dsp_common->display_page_or_cursor_nav(is_cursor_nav());
}

bool MDspStdListBase::is_list_focused()
{
    return list_focused;
}

void MDspStdListBase::set_list_focus(const bool flag)
{
    list_focused = flag;
}

bool MDspStdListBase::key_pressed(const uint32_t key)
{
    bool intercepted = MDspBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == KeyCodes::TAB)
        {
            if (is_cursor_nav())
            {
                clear_cursor();
            }
            else
            {
                reset_cursor_position();
            }
            dsp_comp_hub.dsp_selector->refresh_display();
            intercepted = true;
        }
        else
        if (key == static_cast<uint32_t> (' '))
        {
            if (is_cursor_nav())
            {
                toggle_select_cursor_item();
            }
            dsp_comp_hub.dsp_selector->refresh_display();
            intercepted = true;
        }
        else
        if (key == KeyCodes::FUNC_04)
        {
            dsp_comp_hub.dsp_common->application_working();
            clear_selection();
            dsp_comp_hub.dsp_selector->refresh_display();
            intercepted = true;
        }
    }
    return intercepted;
}

bool MDspStdListBase::mouse_action(MouseEvent& mouse)
{
    bool intercepted = MDspBase::mouse_action(mouse);
    if (!intercepted)
    {
        if (mouse.button == MouseEvent::button_id::BUTTON_01 && mouse.event == MouseEvent::event_id::MOUSE_RELEASE)
        {
            if (mouse.coord_row == DisplayConsts::PAGE_NAV_Y &&
                mouse.coord_column >= dsp_comp_hub.term_cols - DisplayConsts::PAGE_NAV_X &&
                mouse.coord_column < dsp_comp_hub.term_cols - DisplayConsts::PAGE_NAV_X + 4)
            {
                clear_cursor();
                dsp_comp_hub.dsp_selector->refresh_display();
                intercepted = true;
            }
        }
    }
    return intercepted;
}

void MDspStdListBase::enter_command_line_mode()
{
    set_list_focus(false);
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspStdListBase::leave_command_line_mode()
{
    set_list_focus(true);
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspStdListBase::enter_page_nav_mode()
{
    set_list_focus(false);
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspStdListBase::leave_page_nav_mode(const page_change_type change)
{
    set_list_focus(true);
    if (change != page_change_type::PG_CHG_CANCELED)
    {
        if (change != page_change_type::PG_CHG_SAME || is_cursor_nav())
        {
            clear_cursor();
            dsp_comp_hub.dsp_selector->refresh_display();
        }
    }
}

void MDspStdListBase::first_page()
{
    set_page_nr(1);
    if (is_cursor_nav())
    {
        reset_cursor_position();
    }
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspStdListBase::next_page()
{
    MDspBase::next_page();
    if (is_cursor_nav())
    {
        reset_cursor_position();
    }
}

void MDspStdListBase::previous_page()
{
    MDspBase::previous_page();
    if (is_cursor_nav())
    {
        reset_cursor_position();
    }
}

void MDspStdListBase::last_page()
{
    set_page_nr(DisplayConsts::MAX_PAGE_NR);
    if (is_cursor_nav())
    {
        reset_cursor_position();
    }
    dsp_comp_hub.dsp_selector->refresh_display();
}
