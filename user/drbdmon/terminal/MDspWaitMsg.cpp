#include <terminal/MDspWaitMsg.h>
#include <terminal/DisplayConsts.h>

MDspWaitMsg::MDspWaitMsg(const ComponentsHub& comp_hub):
    MDspBase::MDspBase(comp_hub)
{
}

MDspWaitMsg::~MDspWaitMsg() noexcept
{
}

void MDspWaitMsg::display_content()
{
    dsp_comp_hub.dsp_io->cursor_xy((dsp_comp_hub.term_cols - wait_msg.length()) >> 1, dsp_comp_hub.term_rows >> 1);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->caution_text.c_str());
    dsp_comp_hub.dsp_io->write_text(wait_msg.c_str());
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
    dsp_comp_hub.dsp_common->application_working();
}

void MDspWaitMsg::set_wait_msg(const std::string& message)
{
    wait_msg = message;
}

void MDspWaitMsg::enter_command_line_mode()
{
}

void MDspWaitMsg::leave_command_line_mode()
{
}

void MDspWaitMsg::enter_page_nav_mode()
{
}

void MDspWaitMsg::leave_page_nav_mode(const page_change_type change)
{
}

void MDspWaitMsg::cursor_to_next_item()
{
}

void MDspWaitMsg::cursor_to_previous_item()
{
}

void MDspWaitMsg::display_deactivated()
{
    MDspBase::display_deactivated();
    wait_msg.clear();
    wait_msg.shrink_to_fit();
}

void MDspWaitMsg::reset_display()
{
    MDspBase::reset_display();
}

void MDspWaitMsg::synchronize_data()
{
}

uint64_t MDspWaitMsg::get_update_mask() noexcept
{
    return 0;
}
