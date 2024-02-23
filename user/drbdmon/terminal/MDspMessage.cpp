#include <terminal/MDspMessage.h>
#include <terminal/DisplayConsts.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/HelpText.h>
#include <terminal/KeyCodes.h>

const uint16_t  MDspMessage::MAX_MSG_TEXT_WIDTH = 160;

MDspMessage::MDspMessage(const ComponentsHub& comp_hub, MessageLog& log_ref):
    MDspBase::MDspBase(comp_hub),
    log(log_ref),
    format_text(comp_hub)
{
}

MDspMessage::~MDspMessage() noexcept
{
}

void MDspMessage::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(
        (&log == dsp_comp_hub.debug_log) ? DisplayId::MDSP_DEBUG_MSG_VIEW : DisplayId::MDSP_MSG_VIEW
    );

    uint32_t current_line = DisplayConsts::PAGE_NAV_Y + 1;
    if (have_message)
    {
        format_text.restart();

        const uint32_t lines_per_page = get_lines_per_page();
        if (saved_term_cols != dsp_comp_hub.term_cols || saved_term_rows != dsp_comp_hub.term_rows)
        {
            format_text.set_line_length(
                std::min(static_cast<uint16_t> (dsp_comp_hub.term_cols - 2), MAX_MSG_TEXT_WIDTH)
            );
            saved_term_cols = dsp_comp_hub.term_cols;
            saved_term_rows = dsp_comp_hub.term_rows;

            // Include the log level & date line on the first page
            uint32_t line_count = 1;
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
        // Include the log level & date line on the first page
        uint32_t page_line_ctr = 1;
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
        if (page_nr == 1)
        {
            dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
            if (level == MessageLog::log_level::INFO)
            {
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
                dsp_comp_hub.dsp_io->write_text("INFO");
            }
            else
            if (level == MessageLog::log_level::WARN)
            {
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->warn.c_str());
                dsp_comp_hub.dsp_io->write_text("WARNING");
            }
            else
            {
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                dsp_comp_hub.dsp_io->write_text("ALERT");
            }
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
            dsp_comp_hub.dsp_io->cursor_xy(12, current_line);
            dsp_comp_hub.dsp_io->write_text(date_label.c_str());
            ++current_line;
        }
        while (page_line_ctr < lines_per_page)
        {
            if (format_text.next_line(line, dsp_comp_hub.active_color_table->rst))
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
    else
    {
        dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
        dsp_comp_hub.dsp_io->write_text("No message selected");
    }
}

bool MDspMessage::key_pressed(const uint32_t key)
{
    bool intercepted = MDspBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::MESSAGE_DETAIL, dsp_comp_hub);
            intercepted = true;
        }
    }
    return intercepted;
}

bool MDspMessage::mouse_action(MouseEvent& mouse)
{
    bool intercepted = MDspBase::mouse_action(mouse);
    return intercepted;
}

void MDspMessage::enter_command_line_mode()
{
}

void MDspMessage::leave_command_line_mode()
{
}

void MDspMessage::enter_page_nav_mode()
{
}

void MDspMessage::leave_page_nav_mode(const page_change_type change)
{
    if (change == page_change_type::PG_CHG_CHANGED)
    {
        dsp_comp_hub.dsp_selector->refresh_display();
    }
}

void MDspMessage::display_activated()
{
    MDspBase::display_activated();
    // Force recalculating the text alignment and page count
    saved_term_cols = 0;
    saved_term_rows = 0;
    have_message = false;

    const uint64_t message_id = (&log == dsp_comp_hub.debug_log) ?
        dsp_comp_hub.dsp_shared->debug_message_id : dsp_comp_hub.dsp_shared->message_id;
    if (message_id != MessageLog::ID_NONE)
    {
        std::unique_lock<std::recursive_mutex> lock(log.queue_lock);

        MessageLog::Entry* log_entry = log.get_entry(message_id);
        if (log_entry != nullptr)
        {
            level = log_entry->get_log_level();
            date_label = log_entry->get_date_label();
            message_text = log_entry->get_message();

            format_text.set_text(&message_text);

            have_message = true;
        }
    }
}

void MDspMessage::display_deactivated()
{
    MDspBase::display_deactivated();
    have_message = false;
    format_text.reset();
    date_label.clear();
    message_text.clear();
}

void MDspMessage::reset_display()
{
    MDspBase::reset_display();
    set_page_nr(1);
    format_text.restart();
}

void MDspMessage::synchronize_data()
{
}

void MDspMessage::cursor_to_next_item()
{
}

void MDspMessage::cursor_to_previous_item()
{
}

uint64_t MDspMessage::get_update_mask() noexcept
{
    return update_event::UPDATE_FLAG_MESSAGE_LOG;
}

uint32_t MDspMessage::get_lines_per_page() noexcept
{
    return dsp_comp_hub.term_rows - DisplayConsts::PAGE_NAV_Y - 2;
}
