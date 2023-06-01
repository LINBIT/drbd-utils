#include <terminal/MDspLogViewer.h>
#include <terminal/KeyCodes.h>
#include <terminal/navigation.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/HelpText.h>
#include <MessageLog.h>
#include <string>
#include <mutex>

const uint8_t MDspLogViewer::LOG_HEADER_Y   = 3;
const uint8_t MDspLogViewer::LOG_LIST_Y     = 4;

MDspLogViewer::MDspLogViewer(const ComponentsHub& comp_hub):
    MDspStdListBase::MDspStdListBase(comp_hub)
{
    log_key_func =
        [](MessageLog::Entry* msg_entry) -> const uint64_t&
        {
            return msg_entry->get_id();
        };
}

MDspLogViewer::~MDspLogViewer() noexcept
{
}

void MDspLogViewer::display_list()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_LOG_VIEW);
    display_log_header();

    std::unique_lock<std::recursive_mutex> lock(dsp_comp_hub.log->queue_lock);

    const uint32_t lines_per_page = get_lines_per_page();
    MessageLog::Queue::ValuesIterator msg_iter = dsp_comp_hub.log->iterator();
    if (msg_iter.get_size() >= 1)
    {
        const bool selecting = is_selecting();
        if (is_cursor_nav())
        {
            uint32_t msg_idx = 0;
            MessageLog::Entry* const first_entry = navigation::find_first_item_on_cursor_page(
                dsp_comp_hub.dsp_shared->message_id, log_key_func, msg_iter, lines_per_page, msg_idx
            );

            if (first_entry != nullptr)
            {
                const uint64_t first_entry_id = first_entry->get_id();
                MessageLog::Queue::ValuesIterator dsp_msg_iter = dsp_comp_hub.log->iterator(first_entry_id);
                uint32_t current_line = LOG_LIST_Y;
                uint32_t line_ctr = 0;
                while (dsp_msg_iter.has_next() && line_ctr < lines_per_page)
                {
                    MessageLog::Entry* const msg_entry = dsp_msg_iter.next();
                    write_log_line(msg_entry, selecting, current_line);
                    ++line_ctr;
                }
            }

            set_page_nr(msg_idx / lines_per_page + 1);
        }
        else
        {
            navigation::find_first_item_on_page(get_page_nr(), get_line_offset(), lines_per_page, msg_iter);

            uint32_t current_line = LOG_LIST_Y;
            uint32_t line_ctr = 0;
            while (msg_iter.has_next() && line_ctr < lines_per_page)
            {
                MessageLog::Entry* const msg_entry = msg_iter.next();
                write_log_line(msg_entry, selecting, current_line);
                ++line_ctr;
            }
        }
    }
    else
    {
        dsp_comp_hub.dsp_io->cursor_xy(1, LOG_LIST_Y);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
        dsp_comp_hub.dsp_io->write_text("The message log contains no entries");
    }
    set_page_count(
        dsp_comp_hub.dsp_common->calculate_page_count(
            static_cast<uint32_t> (msg_iter.get_size()),
            lines_per_page
        )
    );
}

bool MDspLogViewer::key_pressed(const uint32_t key)
{
    bool intercepted = MDspStdListBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == KeyCodes::ENTER)
        {
            if (dsp_comp_hub.dsp_shared->message_id != MessageLog::ID_NONE)
            {
                dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::MSG_VIEWER);
            }
            intercepted = true;
        }
        else
        if (key == KeyCodes::DELETE)
        {
            if (is_selecting())
            {
                {
                    std::unique_lock<std::recursive_mutex> lock(dsp_comp_hub.log->queue_lock);

                    MessageMap::KeysIterator iter(*(dsp_comp_hub.dsp_shared->selected_log_entries));
                    while (iter.has_next())
                    {
                        const uint64_t* const entry_id_ptr = iter.next();
                        const uint64_t entry_id = *entry_id_ptr;
                        dsp_comp_hub.log->delete_entry(entry_id);
                    }
                }
                dsp_comp_hub.dsp_shared->clear_log_entry_selection(*(dsp_comp_hub.dsp_shared->selected_log_entries));
                cursor_to_nearest_item();
                dsp_comp_hub.dsp_selector->refresh_display();
            }
            else
            if (is_cursor_nav())
            {
                std::unique_lock<std::recursive_mutex> lock(dsp_comp_hub.log->queue_lock);

                dsp_comp_hub.log->delete_entry(dsp_comp_hub.dsp_shared->message_id);
                cursor_to_nearest_item();
                dsp_comp_hub.dsp_selector->refresh_display();
            }
            intercepted = true;
        }
        else
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::MESSAGE_LOG, dsp_comp_hub);
            intercepted = true;
        }
    }
    return intercepted;
}

bool MDspLogViewer::mouse_action(MouseEvent& mouse)
{
    bool intercepted = MDspStdListBase::mouse_action(mouse);
    if (!intercepted)
    {
        if (mouse.coord_row >= LOG_LIST_Y && mouse.event == MouseEvent::event_id::MOUSE_RELEASE)
        {
            const uint32_t lines_per_page = get_lines_per_page();
            if (mouse.coord_row < LOG_LIST_Y + lines_per_page)
            {
                list_item_clicked(mouse);
                intercepted = true;
            }
        }
    }
    return intercepted;
}

void MDspLogViewer::list_item_clicked(MouseEvent& mouse)
{
    const uint32_t lines_per_page = get_lines_per_page();
    const uint32_t selected_line = mouse.coord_row - LOG_LIST_Y;

    std::unique_lock<std::recursive_mutex> lock(dsp_comp_hub.log->queue_lock);

    MessageLog::Queue::ValuesIterator msg_iter = dsp_comp_hub.log->iterator();
    navigation::find_first_item_on_page(get_page_nr(), get_line_offset(), lines_per_page, msg_iter);
    uint32_t line_ctr = 0;
    while (msg_iter.has_next() && line_ctr <= selected_line)
    {
        MessageLog::Entry* const msg_entry = msg_iter.next();
        if (line_ctr == selected_line)
        {
            dsp_comp_hub.dsp_shared->message_id = msg_entry->get_id();
            if (mouse.button == MouseEvent::button_id::BUTTON_01 &&
                mouse.coord_column <= DisplayConsts::MAX_SELECT_X)
            {
                toggle_select_cursor_item();
                dsp_comp_hub.dsp_selector->refresh_display();
            }
            else
            if (mouse.button == MouseEvent::button_id::BUTTON_03 && mouse.event == MouseEvent::event_id::MOUSE_RELEASE)
            {
                dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::MSG_VIEWER);
            }
            else
            {
                dsp_comp_hub.dsp_selector->refresh_display();
            }
        }
        ++line_ctr;
    }
}

void MDspLogViewer::display_activated()
{
    MDspStdListBase::display_activated();
}

void MDspLogViewer::display_deactivated()
{
    MDspStdListBase::display_deactivated();
}

void MDspLogViewer::reset_display()
{
    MDspStdListBase::reset_display();
    dsp_comp_hub.dsp_shared->message_id = MessageLog::ID_NONE;
}

void MDspLogViewer::synchronize_data()
{
}

void MDspLogViewer::cursor_to_next_item()
{
    std::unique_lock<std::recursive_mutex> lock(dsp_comp_hub.log->queue_lock);

    if (dsp_comp_hub.dsp_shared->message_id != MessageLog::ID_NONE)
    {
        MessageLog::Entry* msg_entry = dsp_comp_hub.log->get_next_entry(dsp_comp_hub.dsp_shared->message_id);
        if (msg_entry != nullptr)
        {
            dsp_comp_hub.dsp_shared->message_id = msg_entry->get_id();
            dsp_comp_hub.dsp_selector->refresh_display();
        }
    }
    else
    {
        MessageLog::Queue::ValuesIterator msg_iter = dsp_comp_hub.log->iterator();
        if (msg_iter.has_next())
        {
            MessageLog::Entry* const msg_entry = msg_iter.next();
            dsp_comp_hub.dsp_shared->message_id = msg_entry->get_id();
            dsp_comp_hub.dsp_selector->refresh_display();
        }
    }
}

void MDspLogViewer::cursor_to_previous_item()
{
    std::unique_lock<std::recursive_mutex> lock(dsp_comp_hub.log->queue_lock);

    if (dsp_comp_hub.dsp_shared->message_id != MessageLog::ID_NONE)
    {
        MessageLog::Entry* msg_entry = dsp_comp_hub.log->get_previous_entry(dsp_comp_hub.dsp_shared->message_id);
        if (msg_entry != nullptr)
        {
            dsp_comp_hub.dsp_shared->message_id = msg_entry->get_id();
            dsp_comp_hub.dsp_selector->refresh_display();
        }
    }
    else
    {
        MessageLog::Queue::ValuesIterator msg_iter = dsp_comp_hub.log->iterator();
        if (msg_iter.has_next())
        {
            MessageLog::Entry* const msg_entry = msg_iter.next();
            dsp_comp_hub.dsp_shared->message_id = msg_entry->get_id();
            dsp_comp_hub.dsp_selector->refresh_display();
        }
    }
}

uint32_t MDspLogViewer::get_lines_per_page() noexcept
{
    return dsp_comp_hub.term_rows - LOG_HEADER_Y - 2;
}

void MDspLogViewer::display_log_header()
{
    dsp_comp_hub.dsp_io->cursor_xy(1, LOG_HEADER_Y);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->list_header.c_str());
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CLEAR_LINE.c_str());

    dsp_comp_hub.dsp_io->cursor_xy(3, LOG_HEADER_Y);
    dsp_comp_hub.dsp_io->write_text("Severity");

    dsp_comp_hub.dsp_io->cursor_xy(13, LOG_HEADER_Y);
    dsp_comp_hub.dsp_io->write_text("Time");

    dsp_comp_hub.dsp_io->cursor_xy(37, LOG_HEADER_Y);
    dsp_comp_hub.dsp_io->write_text("Message");

    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
}

bool MDspLogViewer::is_cursor_nav()
{
    return dsp_comp_hub.dsp_shared->message_id != MessageLog::ID_NONE;
}

void MDspLogViewer::reset_cursor_position()
{
    std::unique_lock<std::recursive_mutex> lock(dsp_comp_hub.log->queue_lock);

    MessageLog::Queue::ValuesIterator msg_iter = dsp_comp_hub.log->iterator();

    navigation::find_first_item_on_page(get_page_nr(), get_line_offset(), get_lines_per_page(), msg_iter);
    if (msg_iter.has_next())
    {
        MessageLog::Entry* const msg_entry = msg_iter.next();
        dsp_comp_hub.dsp_shared->message_id = msg_entry->get_id();
        dsp_comp_hub.dsp_selector->refresh_display();
    }
}

void MDspLogViewer::clear_cursor()
{
    if (dsp_comp_hub.dsp_shared->message_id != MessageLog::ID_NONE)
    {
        dsp_comp_hub.dsp_selector->refresh_display();
    }
    dsp_comp_hub.dsp_shared->message_id = MessageLog::ID_NONE;
}

bool MDspLogViewer::is_selecting()
{
    return dsp_comp_hub.dsp_shared->have_log_entry_selection(*(dsp_comp_hub.dsp_shared->selected_log_entries));
}

void MDspLogViewer::toggle_select_cursor_item()
{
    if (dsp_comp_hub.dsp_shared->message_id != MessageLog::ID_NONE)
    {
        dsp_comp_hub.dsp_shared->toggle_log_entry_selection(
            *(dsp_comp_hub.dsp_shared->selected_log_entries), dsp_comp_hub.dsp_shared->message_id
        );
    }
}

void MDspLogViewer::clear_selection()
{
    dsp_comp_hub.dsp_shared->clear_log_entry_selection(*(dsp_comp_hub.dsp_shared->selected_log_entries));
}

void MDspLogViewer::write_log_line(MessageLog::Entry* const msg_entry, const bool selecting, uint32_t& current_line)
{
    DisplayIo* const dsp_io = dsp_comp_hub.dsp_io;

    dsp_io->cursor_xy(1, current_line);
    const uint64_t message_id = msg_entry->get_id();
    const bool is_under_cursor = message_id == dsp_comp_hub.dsp_shared->message_id;
    const bool is_selected = selecting && dsp_comp_hub.dsp_shared->is_log_entry_selected(
        *(dsp_comp_hub.dsp_shared->selected_log_entries), msg_entry->get_id()
    );

    const std::string& rst_bg = is_under_cursor ?
        dsp_comp_hub.active_color_table->bg_cursor :
        (is_selected ? dsp_comp_hub.active_color_table->bg_marked : dsp_comp_hub.active_color_table->rst_bg);

    dsp_io->write_text(rst_bg.c_str());
    if (is_under_cursor || is_selected)
    {
        dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CLEAR_LINE.c_str());
    }

    if (is_selected)
    {
        dsp_io->write_text(dsp_comp_hub.active_character_table->sym_marked.c_str());
    }

    dsp_io->cursor_xy(3, current_line);
    MessageLog::log_level level = msg_entry->get_log_level();
    if (level == MessageLog::log_level::INFO)
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
        dsp_io->write_text("INFO");
    }
    else
    if (level == MessageLog::log_level::WARN)
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->warn.c_str());
        dsp_io->write_text("WARNING");
    }
    else
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
        dsp_io->write_text("ALERT");
    }
    dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());

    dsp_io->cursor_xy(13, current_line);
    dsp_io->write_text(msg_entry->get_date_label().c_str());

    dsp_io->cursor_xy(37, current_line);
    const std::string& message = msg_entry->get_message();
    const size_t newline_idx = message.find("\n");
    if (newline_idx == std::string::npos)
    {
        dsp_io->write_string_field(message, dsp_comp_hub.term_cols - 37 + 1, false);
    }
    else
    {
        const std::string first_line = message.substr(0, newline_idx);
        dsp_io->write_string_field(first_line, dsp_comp_hub.term_cols - 37 + 1, false);
    }

    dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

    ++current_line;
}

void MDspLogViewer::cursor_to_nearest_item()
{
    if (is_cursor_nav())
    {
        const uint64_t cursor_message_id = dsp_comp_hub.dsp_shared->message_id;
        if (cursor_message_id != MessageLog::ID_NONE)
        {
            MessageLog::Entry* const near_entry = dsp_comp_hub.log->get_entry_near(cursor_message_id);
            if (near_entry != nullptr)
            {
                dsp_comp_hub.dsp_shared->message_id = near_entry->get_id();
            }
            else
            {
                // No items, cancel cursor navigation
                dsp_comp_hub.dsp_shared->message_id = MessageLog::ID_NONE;
                set_page_nr(1);
            }
            dsp_comp_hub.dsp_selector->refresh_display();
        }
    }
}

uint64_t MDspLogViewer::get_update_mask() noexcept
{
    return update_event::UPDATE_FLAG_MESSAGE_LOG;
}
