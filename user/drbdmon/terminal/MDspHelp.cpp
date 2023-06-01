#include <terminal/MDspHelp.h>
#include <terminal/DisplayConsts.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/DisplayId.h>
#include <terminal/KeyCodes.h>

const uint16_t  MDspHelp::MAX_HELP_TEXT_WIDTH   = 160;

MDspHelp::MDspHelp(const ComponentsHub& comp_hub):
    MDspBase::MDspBase(comp_hub),
    format_text(comp_hub)
{
}

MDspHelp::~MDspHelp() noexcept
{
}

void MDspHelp::display_content()
{
    format_text.restart();

    const uint32_t lines_per_page = get_lines_per_page();
    if (saved_term_cols != dsp_comp_hub.term_cols || saved_term_rows != dsp_comp_hub.term_rows)
    {
        format_text.set_line_length(
            std::min(static_cast<uint16_t> (dsp_comp_hub.term_cols - 2), MAX_HELP_TEXT_WIDTH)
        );
        saved_term_cols = dsp_comp_hub.term_cols;
        saved_term_rows = dsp_comp_hub.term_rows;

        uint32_t line_count = 0;
        while (format_text.skip_line())
        {
            ++line_count;
        }

        const uint32_t page_count = dsp_comp_hub.dsp_common->calculate_page_count(line_count, lines_per_page);
        set_page_count(page_count);
    }

    uint32_t page_nr = get_page_nr();
    const uint32_t page_count = get_page_count();
    // Override auto-scroll to the last page
    if (page_nr > page_count)
    {
        set_page_nr(page_count);
        page_nr = page_count;
    }

    format_text.restart();

    uint32_t page_ctr = 1;
    uint32_t page_line_ctr = 0;
    while (page_ctr < page_nr && format_text.skip_line())
    {
        ++page_line_ctr;
        if (page_line_ctr >= lines_per_page)
        {
            page_line_ctr = 0;
            ++page_ctr;
        }
    }

    std::string line;
    page_line_ctr = 0;
    uint32_t current_line = DisplayConsts::PAGE_NAV_Y + 1;
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->help_text.c_str());
    while (page_line_ctr < lines_per_page)
    {
        if (format_text.next_line(line, dsp_comp_hub.active_color_table->help_text))
        {
            dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
            dsp_comp_hub.dsp_io->write_text(line.c_str());
        }
        else
        {
            break;
        }
        ++page_line_ctr;
        ++current_line;
    }
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
}

void MDspHelp::display_activated()
{
    MDspBase::display_activated();

    // Force recalculating the text alignment and page count
    if (dsp_comp_hub.dsp_shared->help_text_updated)
    {
        saved_term_cols = 0;
        saved_term_rows = 0;

        set_page_nr(1);

        dsp_comp_hub.dsp_shared->help_text_updated = false;
    }

    format_text.set_text(&(dsp_comp_hub.dsp_shared->help_text));
}

void MDspHelp::display_deactivated()
{
    MDspBase::display_deactivated();
}

void MDspHelp::display_closed()
{
    MDspBase::display_closed();
    format_text.reset();
    dsp_comp_hub.dsp_shared->help_text.clear();
}

void MDspHelp::reset_display()
{
    MDspBase::reset_display();
    set_page_nr(1);
    format_text.restart();
}

void MDspHelp::synchronize_data()
{
}

uint32_t MDspHelp::get_lines_per_page() noexcept
{
    return dsp_comp_hub.term_rows - DisplayConsts::PAGE_NAV_Y - 2;
}

void MDspHelp::cursor_to_next_item()
{
    // no-op
}

void MDspHelp::cursor_to_previous_item()
{
    // no-op
}

bool MDspHelp::key_pressed(const uint32_t key)
{
    bool intercepted = MDspBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == KeyCodes::FUNC_01)
        {
            dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::HELP_IDX);
            intercepted = true;
        }
    }
    return intercepted;
}

bool MDspHelp::mouse_action(MouseEvent& mouse)
{
    bool intercepted = MDspBase::mouse_action(mouse);
    return intercepted;
}

void MDspHelp::enter_command_line_mode()
{
}

void MDspHelp::leave_command_line_mode()
{
}

void MDspHelp::enter_page_nav_mode()
{
}

void MDspHelp::leave_page_nav_mode(const page_change_type change)
{
    if (change == page_change_type::PG_CHG_CHANGED)
    {
        dsp_comp_hub.dsp_selector->refresh_display();
    }
}

uint64_t MDspHelp::get_update_mask() noexcept
{
    return 0;
}
