#include <terminal/MDspConnections.h>
#include <terminal/KeyCodes.h>
#include <terminal/DisplayConsts.h>
#include <terminal/DisplaySelector.h>
#include <terminal/InputField.h>
#include <terminal/navigation.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/HelpText.h>
#include <terminal/GlobalCommandConsts.h>
#include <objects/DrbdResource.h>
#include <comparators.h>
#include <bounds.h>
#include <string_matching.h>
#include <functional>

const uint8_t   MDspConnections::RSC_HEADER_Y       = 3;
const uint8_t   MDspConnections::CON_HEADER_Y       = 5;
const uint8_t   MDspConnections::CON_LIST_Y         = 6;

MDspConnections::MDspConnections(const ComponentsHub& comp_hub):
    MDspStdListBase::MDspStdListBase(comp_hub)
{
    problem_filter =
        [](DrbdConnection* con) -> bool
        {
            return con->has_mark_state() || con->has_warn_state() || con->has_alert_state();
        };
    con_key_func =
        [](DrbdConnection* con) -> const std::string&
        {
            return con->get_name();
        };
}

MDspConnections::~MDspConnections() noexcept
{
}

void MDspConnections::display_activated()
{
    if (displayed_rsc != dsp_comp_hub.dsp_shared->monitor_rsc)
    {
        reset_display();
    }
    displayed_rsc = dsp_comp_hub.dsp_shared->monitor_rsc;
    cursor_con = dsp_comp_hub.dsp_shared->monitor_con;
    dsp_comp_hub.dsp_shared->ovrd_connection_selection = false;
}

void MDspConnections::display_deactivated()
{
    MDspStdListBase::display_deactivated();
}

void MDspConnections::reset_display()
{
    MDspStdListBase::reset_display();
    cursor_con.clear();
    clear_selection();
    set_page_nr(1);
}

void MDspConnections::display_list()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_CON_LIST);

    uint32_t current_line = RSC_HEADER_Y;
    dsp_comp_hub.dsp_common->display_resource_header(current_line);
    dsp_comp_hub.dsp_common->display_resource_line(current_line);

    dsp_comp_hub.dsp_common->display_connection_header(current_line);

    if (is_cursor_nav())
    {
        display_at_cursor();
    }
    else
    {
        display_at_page();
    }
}

void MDspConnections::write_connection_line(DrbdConnection* const con, uint32_t& current_line, const bool selecting)
{
    DisplayIo* const dsp_io = dsp_comp_hub.dsp_io;

    const bool has_mark_state   = con->has_mark_state();
    const bool has_alert_state  = con->has_alert_state();
    const bool has_warn_state   = con->has_warn_state();

    const std::string& con_name = con->get_name();
    bool is_under_cursor = false;
    if (is_cursor_nav())
    {
        is_under_cursor = cursor_con == con_name;
    }

    bool is_selected = false;
    if (selecting)
    {
        is_selected = dsp_comp_hub.dsp_shared->is_connection_selected(con_name);
    }

    const std::string& rst_bg = is_under_cursor ? dsp_comp_hub.active_color_table->bg_cursor :
        (is_selected ? dsp_comp_hub.active_color_table->bg_marked : dsp_comp_hub.active_color_table->rst_bg);
    dsp_io->write_text(rst_bg.c_str());
    dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CLEAR_LINE.c_str());

    // Selection symbol
    if (is_selected)
    {
        dsp_io->write_text(dsp_comp_hub.active_character_table->sym_marked.c_str());
    }
    else
    {
        dsp_io->write_char(' ');
    }

    if (has_alert_state)
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
        dsp_io->write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
        dsp_io->write_text(rst_bg.c_str());
    }
    else
    if (has_mark_state || has_warn_state)
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->warn.c_str());
        dsp_io->write_text(dsp_comp_hub.active_character_table->sym_warn.c_str());
        dsp_io->write_text(rst_bg.c_str());
    }
    else
    {
        dsp_io->write_char(' ');
    }

    dsp_io->write_char(' ');

    // Connection/peer role
    DrbdConnection::resource_role peer_role = con->get_role();
    if (peer_role == DrbdConnection::resource_role::PRIMARY)
    {
        dsp_io->write_text(dsp_comp_hub.active_character_table->sym_pri.c_str());
    }
    else
    if (peer_role == DrbdConnection::resource_role::SECONDARY)
    {
        dsp_io->write_text(dsp_comp_hub.active_character_table->sym_sec.c_str());
    }
    else
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->warn.c_str());
        dsp_io->write_text(dsp_comp_hub.active_character_table->sym_unknown.c_str());
        dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
    }

    dsp_io->write_char(' ');

    // Connection name
    if (!(has_mark_state || has_alert_state || has_warn_state))
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->con_name.c_str());
    }
    else
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
    }
    dsp_io->write_string_field(con_name, dsp_comp_hub.term_cols - 34, false);

    dsp_io->cursor_xy(dsp_comp_hub.term_cols - 29, current_line);
    if (con->has_connection_alert())
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
    }
    else
    if (has_mark_state || has_warn_state)
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->warn.c_str());
    }
    else
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
    }
    // TODO: Update experimental feature. This does not exist in any official DRBD version.
    // Begin experimental feature
    const DrbdConnection::sync_state_type sync_state = con->get_sync_state();
    if (con->get_connection_state() != DrbdConnection::state::STANDALONE ||
        sync_state == DrbdConnection::sync_state_type::RESYNCABLE)
    {
        dsp_io->write_text(con->get_connection_state_label());
    }
    else
    if (sync_state == DrbdConnection::sync_state_type::SPLIT)
    {
        dsp_io->write_text("SplitBrain");
    }
    else
    if (sync_state == DrbdConnection::sync_state_type::UNRELATED)
    {
        dsp_io->write_text("UnrelatedData");
    }
    else
    {
        dsp_io->write_text("(INVALID)");
    }
    // End experimental feature

    dsp_io->cursor_xy(dsp_comp_hub.term_cols - 13, current_line);
    // Peer volume count
    {
        bool have_vlm_alerts = false;
        const uint16_t vlm_count = con->get_volume_count();
        uint16_t norm_count = 0;
        if (has_mark_state)
        {
            DrbdConnection::VolumesIterator vlm_iter = con->volumes_iterator();
            while (vlm_iter.has_next())
            {
                DrbdVolume* const vlm = vlm_iter.next();
                if (!(vlm->has_alert_state() || vlm->has_mark_state() || vlm->has_warn_state()))
                {
                    ++norm_count;
                }
                if (vlm->has_alert_state())
                {
                    have_vlm_alerts = true;
                }
            }
        }
        else
        {
            norm_count = vlm_count;
        }

        if (vlm_count == norm_count)
        {
            dsp_io->write_char(' ');
        }
        else
        if (have_vlm_alerts)
        {
            dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
            dsp_io->write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
            dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
            dsp_io->write_text(rst_bg.c_str());
        }
        else
        {
            dsp_io->write_text(dsp_comp_hub.active_color_table->warn.c_str());
            dsp_io->write_text(dsp_comp_hub.active_character_table->sym_warn.c_str());
            dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
            dsp_io->write_text(rst_bg.c_str());
        }
        dsp_io->write_fmt(" %5u/%5u", static_cast<unsigned int> (norm_count), static_cast<unsigned int> (vlm_count));
        dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
        dsp_io->write_text(rst_bg.c_str());
    }

    dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
    ++current_line;
}

void MDspConnections::write_no_connections_line(const bool problem_mode_flag)
{
    dsp_comp_hub.dsp_io->cursor_xy(1, CON_LIST_Y);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
    if (problem_mode_flag)
    {
        dsp_comp_hub.dsp_io->write_text("No connections with problem status");
    }
    else
    {
        dsp_comp_hub.dsp_io->write_text("No active connections");
    }
}

void MDspConnections::display_at_cursor()
{
    DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
    if (rsc != nullptr)
    {
        const bool problem_mode_flag = is_problem_mode(rsc);
        dsp_comp_hub.dsp_common->display_problem_mode_label(problem_mode_flag);

        const uint32_t lines_per_page = get_lines_per_page();
        uint32_t line_nr = 0;
        DrbdResource::ConnectionsIterator con_iter = rsc->connections_iterator();
        if (problem_mode_flag)
        {
            // Show connections with some problem state

            uint32_t dsp_con_idx = 0;
            uint32_t dsp_con_count = 0;

            DrbdConnection* const page_first_con = navigation::find_first_filtered_item_on_cursor_page(
                cursor_con,
                con_key_func,
                con_iter,
                problem_filter,
                lines_per_page,
                dsp_con_idx,
                dsp_con_count
            );

            set_page_nr((dsp_con_idx / lines_per_page) + 1);
            set_page_count(dsp_comp_hub.dsp_common->calculate_page_count(dsp_con_count, lines_per_page));

            if (page_first_con != nullptr)
            {
                const bool selecting = dsp_comp_hub.dsp_shared->have_connections_selection();
                DrbdResource::ConnectionsIterator dsp_con_iter =
                    rsc->connections_iterator(page_first_con->get_name());
                uint32_t current_line = CON_LIST_Y;
                while (dsp_con_iter.has_next() && line_nr < lines_per_page)
                {
                    DrbdConnection* const con = dsp_con_iter.next();
                    if (problem_filter(con))
                    {
                        dsp_comp_hub.dsp_io->cursor_xy(1, CON_LIST_Y + line_nr);
                        write_connection_line(con, current_line, selecting);
                        ++line_nr;
                    }
                }
            }
        }
        else
        {
            // Show all connections
            const uint8_t con_count = rsc->get_connection_count();

            uint32_t dsp_con_idx = 0;

            DrbdConnection* const page_first_con = navigation::find_first_item_on_cursor_page(
                cursor_con,
                con_key_func,
                con_iter,
                lines_per_page,
                dsp_con_idx
            );

            set_page_nr((dsp_con_idx / lines_per_page) + 1);
            set_page_count(dsp_comp_hub.dsp_common->calculate_page_count(con_count, lines_per_page));

            if (page_first_con != nullptr)
            {
                const bool selecting = dsp_comp_hub.dsp_shared->have_connections_selection();
                DrbdResource::ConnectionsIterator dsp_con_iter =
                    rsc->connections_iterator(page_first_con->get_name());
                uint32_t current_line = CON_LIST_Y;
                while (dsp_con_iter.has_next() && line_nr < lines_per_page)
                {
                    DrbdConnection* const con = dsp_con_iter.next();
                    dsp_comp_hub.dsp_io->cursor_xy(1, CON_LIST_Y + line_nr);
                    write_connection_line(con, current_line, selecting);
                    ++line_nr;
                }
            }
        }

        if (line_nr == 0)
        {
            write_no_connections_line(problem_mode_flag);
        }
    }
    else
    {
        // Resource not present
        dsp_comp_hub.dsp_io->cursor_xy(1, CON_LIST_Y);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
        dsp_comp_hub.dsp_io->write_text("N/A");
    }
}

void MDspConnections::display_at_page()
{
    DisplayIo* const dsp_io = dsp_comp_hub.dsp_io;

    DrbdResource* const dsp_rsc = dsp_comp_hub.get_monitor_resource();
    if (dsp_rsc != nullptr)
    {
        const bool problem_mode_flag = is_problem_mode(dsp_rsc);
        dsp_comp_hub.dsp_common->display_problem_mode_label(problem_mode_flag);

        const uint32_t lines_per_page = get_lines_per_page();
        uint32_t line_nr = 0;
        if (problem_mode_flag)
        {
            uint32_t dsp_con_count = 0;
            DrbdResource::ConnectionsIterator con_iter = dsp_rsc->connections_iterator();
            DrbdConnection* const page_first_con = navigation::find_first_filtered_item_on_page(
                get_page_nr(), get_line_offset(), lines_per_page, con_iter, problem_filter, dsp_con_count
            );
            if (page_first_con != nullptr)
            {
                const bool selecting = dsp_comp_hub.dsp_shared->have_connections_selection();
                DrbdResource::ConnectionsIterator dsp_con_iter =
                    dsp_rsc->connections_iterator(page_first_con->get_name());
                uint32_t current_line = CON_LIST_Y;
                while (dsp_con_iter.has_next() && line_nr < lines_per_page)
                {
                    DrbdConnection* const dsp_con = dsp_con_iter.next();
                    if (problem_filter(dsp_con))
                    {
                        dsp_io->cursor_xy(1, CON_LIST_Y + line_nr);
                        write_connection_line(dsp_con, current_line, selecting);
                        ++line_nr;
                    }
                }
            }
        }
        else
        {
            const bool selecting = dsp_comp_hub.dsp_shared->have_connections_selection();
            DrbdResource::ConnectionsIterator con_iter = dsp_rsc->connections_iterator();
            set_page_count(
                dsp_comp_hub.dsp_common->calculate_page_count(
                    static_cast<uint32_t> (con_iter.get_size()),
                    lines_per_page
                )
            );

            navigation::find_first_item_on_page(get_page_nr(), get_line_offset(), lines_per_page, con_iter);
            uint32_t current_line = CON_LIST_Y;
            while (con_iter.has_next())
            {
                DrbdConnection* const con = con_iter.next();
                dsp_io->cursor_xy(1, CON_LIST_Y + line_nr);
                write_connection_line(con, current_line, selecting);
                ++line_nr;
            }
        }

        if (line_nr == 0)
        {
            write_no_connections_line(problem_mode_flag);
        }
    }
    else
    {
        // Resource not present
        dsp_comp_hub.dsp_io->cursor_xy(1, CON_LIST_Y);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
        dsp_comp_hub.dsp_io->write_text("N/A");
        dsp_comp_hub.dsp_common->display_problem_mode_label(false);
    }
}

// @throws std::bad_alloc, string_matching::PatternLimitException
bool MDspConnections::change_selection(const std::string& pattern_text, const bool select_flag)
{
    DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
    bool matched = false;
    if (rsc != nullptr)
    {
        std::unique_ptr<string_matching::PatternItem> pattern;
        string_matching::process_pattern(pattern_text, pattern);

        DrbdResource::ConnectionsIterator con_iter = rsc->connections_iterator();
        if (is_problem_mode(rsc))
        {
            while (con_iter.has_next())
            {
                DrbdConnection* const con = con_iter.next();
                if (problem_filter(con))
                {
                    const std::string& con_name = con->get_name();
                    if (string_matching::match_text(con_name, pattern.get()))
                    {
                        matched = true;
                        if (select_flag)
                        {
                            dsp_comp_hub.dsp_shared->select_connection(con_name);
                        }
                        else
                        {
                            dsp_comp_hub.dsp_shared->deselect_connection(con_name);
                        }
                    }
                }
            }
        }
        else
        {
            while (con_iter.has_next())
            {
                DrbdConnection* const con = con_iter.next();
                const std::string& con_name = con->get_name();
                if (string_matching::match_text(con_name, pattern.get()))
                {
                    matched = true;
                    if (select_flag)
                    {
                        dsp_comp_hub.dsp_shared->select_connection(con_name);
                    }
                    else
                    {
                        dsp_comp_hub.dsp_shared->deselect_connection(con_name);
                    }
                }
            }
        }
    }
    return matched;
}

void MDspConnections::clear_selection()
{
    dsp_comp_hub.dsp_shared->clear_connections_selection();
}

bool MDspConnections::is_cursor_nav()
{
    return !cursor_con.empty();
}

void MDspConnections::reset_cursor_position()
{
    cursor_con.clear();
    if (!dsp_comp_hub.dsp_shared->monitor_rsc.empty())
    {
        DrbdResource* const rsc = dsp_comp_hub.rsc_map->get(&(dsp_comp_hub.dsp_shared->monitor_rsc));
        if (rsc != nullptr)
        {
            const uint32_t lines_per_page = get_lines_per_page();
            if (is_problem_mode(rsc))
            {
                DrbdResource::ConnectionsIterator con_iter = rsc->connections_iterator();
                std::function<bool(DrbdConnection*)> filter_func =
                    [](DrbdConnection* con) -> bool
                    {
                        return con->has_mark_state() || con->has_warn_state() || con->has_alert_state();
                    };
                uint32_t filtered_count = 0;
                DrbdConnection* const con = navigation::find_first_filtered_item_on_page(
                    get_page_nr(), get_line_offset(), lines_per_page,
                    con_iter, filter_func, filtered_count
                );
                if (con != nullptr)
                {
                    cursor_con = con->get_name();
                }
            }
            else
            {
                DrbdResource::ConnectionsIterator con_iter = rsc->connections_iterator();
                navigation::find_first_item_on_page(get_page_nr(), get_line_offset(), lines_per_page, con_iter);
                if (con_iter.has_next())
                {
                    DrbdConnection* const con = con_iter.next();
                    cursor_con = con->get_name();
                }
            }
        }
    }
}

void MDspConnections::clear_cursor()
{
    cursor_con.clear();
}

bool MDspConnections::is_selecting()
{
    return dsp_comp_hub.dsp_shared->have_connections_selection();
}

void MDspConnections::toggle_select_cursor_item()
{
    if (!dsp_comp_hub.dsp_shared->monitor_rsc.empty() && !cursor_con.empty())
    {
        DrbdResource* const rsc = dsp_comp_hub.rsc_map->get(&(dsp_comp_hub.dsp_shared->monitor_rsc));
        if (rsc != nullptr)
        {
            DrbdConnection* const con = rsc->get_connection(cursor_con);
            if (con != nullptr)
            {
                dsp_comp_hub.dsp_shared->toggle_connection_selection(cursor_con);
                dsp_comp_hub.dsp_selector->refresh_display();
            }
        }
    }
}

bool MDspConnections::key_pressed(const uint32_t key)
{
    bool intercepted = MDspStdListBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::CON_LIST, dsp_comp_hub);
            intercepted = true;
        }
        else
        if (key == static_cast<uint32_t> ('r') || key == static_cast<uint32_t> ('R'))
        {
            dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::RSC_LIST);
            intercepted = true;
        }
        else
        if (key == static_cast<uint32_t> ('v') || key == static_cast<uint32_t> ('V'))
        {
            if (is_cursor_nav())
            {
                dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::PEER_VLM_LIST);
            }
            else
            {
                dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::VLM_LIST);
            }
            intercepted = true;
        }
        else
        if (key == static_cast<uint32_t> ('P') || key == static_cast<uint32_t> ('p'))
        {
            dsp_comp_hub.dsp_common->toggle_problem_mode();
            dsp_comp_hub.dsp_selector->refresh_display();
            intercepted = true;
        }
        else
        if (is_cursor_nav())
        {
            if (key == KeyCodes::ENTER)
            {
                dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::CON_DETAIL);
                intercepted = true;
            }
        }

        if (!intercepted && (is_cursor_nav() || dsp_comp_hub.dsp_shared->have_connections_selection()))
        {
            if (key == static_cast<uint32_t> ('A') || key == static_cast<uint32_t> ('a'))
            {
                dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::CON_ACTIONS);
                intercepted = true;
            }
        }
    }
    return intercepted;
}

bool MDspConnections::mouse_action(MouseEvent& mouse)
{
    bool intercepted = MDspStdListBase::mouse_action(mouse);
    if (!intercepted)
    {
        if ((mouse.button == MouseEvent::button_id::BUTTON_01 || mouse.button == MouseEvent::button_id::BUTTON_03) &&
            mouse.event == MouseEvent::event_id::MOUSE_RELEASE &&
            mouse.coord_row >= CON_LIST_Y)
        {
            const uint32_t lines_per_page = get_lines_per_page();
            if (mouse.coord_row < CON_LIST_Y + lines_per_page)
            {
                list_item_clicked(mouse);
                if (mouse.button == MouseEvent::button_id::BUTTON_03 && is_cursor_nav())
                {
                    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::CON_DETAIL);
                }
                intercepted = true;
            }
        }
    }
    return intercepted;
}

bool MDspConnections::execute_command(const std::string& command, StringTokenizer& tokenizer)
{
    bool accepted = false;
    if (command == cmd_names::KEY_CMD_CURSOR)
    {
        DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
        if (rsc != nullptr)
        {
            if (tokenizer.has_next())
            {
                const std::string cmd_arg = tokenizer.next();
                if (!cmd_arg.empty())
                {
                    if (string_matching::is_pattern(cmd_arg))
                    {
                        try
                        {
                            std::unique_ptr<string_matching::PatternItem> pattern;
                            string_matching::process_pattern(cmd_arg, pattern);

                            DrbdResource::ConnectionsIterator con_iter = rsc->connections_iterator();
                            if (is_problem_mode(rsc))
                            {
                                while (!accepted && con_iter.has_next())
                                {
                                    DrbdConnection* const con = con_iter.next();
                                    if (problem_filter(con))
                                    {
                                        const std::string& con_name = con->get_name();
                                        accepted = string_matching::match_text(con_name, pattern.get());

                                        if (accepted)
                                        {
                                            cursor_con = con_name;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                while (!accepted && con_iter.has_next())
                                {
                                    const DrbdConnection* const con = con_iter.next();
                                    const std::string& con_name = con->get_name();
                                    accepted = string_matching::match_text(con_name, pattern.get());

                                    if (accepted)
                                    {
                                        cursor_con = con_name;
                                    }
                                }
                            }
                        }
                        catch (string_matching::PatternLimitException&)
                        {
                            std::string error_msg(cmd_names::KEY_CMD_CURSOR);
                            error_msg += " command rejected: Excessive number of wildcard characters";
                            dsp_comp_hub.log->add_entry(MessageLog::log_level::ALERT, error_msg);
                        }
                    }
                    else
                    {
                        tokenizer.restart();
                        tokenizer.advance();
                        accepted = dsp_comp_hub.global_cmd_exec->execute_command(
                            cmd_names::KEY_CMD_CONNECTION,
                            tokenizer
                        );
                    }
                }
            }
        }
    }
    else
    if (command == cmd_names::KEY_CMD_SELECT || command == cmd_names::KEY_CMD_DESELECT)
    {
        if (tokenizer.has_next())
        {
            std::string cmd_arg = tokenizer.next();
            if (string_matching::is_pattern(cmd_arg))
            {
                try
                {
                    accepted = change_selection(cmd_arg, command == cmd_names::KEY_CMD_SELECT);
                }
                catch (string_matching::PatternLimitException&)
                {
                    std::string error_msg(command);
                    error_msg += " command rejected: Excessive number of wildcard characters";
                    dsp_comp_hub.log->add_entry(MessageLog::log_level::ALERT, error_msg);
                }
            }
            else
            {
                if (command == cmd_names::KEY_CMD_SELECT)
                {
                    dsp_comp_hub.dsp_shared->select_connection(cmd_arg);
                }
                else
                {
                    dsp_comp_hub.dsp_shared->deselect_connection(cmd_arg);
                }
                accepted = true;
            }
        }
        return accepted;
    }
    else
    if (command == cmd_names::KEY_CMD_SELECT_ALL)
    {
        DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
        if (rsc != nullptr)
        {
            const bool prb_mode = is_problem_mode(rsc);
            DrbdResource::ConnectionsIterator con_iter = rsc->connections_iterator();
            while (con_iter.has_next())
            {
                DrbdConnection* const con = con_iter.next();
                if (!prb_mode || problem_filter(con))
                {
                    const std::string& con_name = con->get_name();
                    dsp_comp_hub.dsp_shared->select_connection(con_name);
                }
            }
        }
        accepted = true;
    }
    else
    if (command == cmd_names::KEY_CMD_DESELECT_ALL || command == cmd_names::KEY_CMD_CLEAR_SELECTION)
    {
        clear_selection();
        accepted = true;
    }
    if (accepted)
    {
        dsp_comp_hub.dsp_selector->refresh_display();
    }
    return accepted;
}

bool MDspConnections::is_problem_mode(DrbdResource* const rsc)
{
    const DisplayCommon::problem_mode_type problem_mode = dsp_comp_hub.dsp_common->get_problem_mode();
    bool problem_mode_flag = problem_mode == DisplayCommon::problem_mode_type::LOCK;
    if (problem_mode == DisplayCommon::problem_mode_type::AUTO)
    {
        if (rsc->has_mark_state())
        {
            DrbdResource::ConnectionsIterator con_iter = rsc->connections_iterator();
            while (con_iter.has_next())
            {
                DrbdConnection* const con = con_iter.next();
                if (con->has_mark_state() || con->has_warn_state() || con->has_alert_state())
                {
                    problem_mode_flag = true;
                    break;
                }
            }
        }
    }
    return problem_mode_flag;
}

void MDspConnections::cursor_to_next_item()
{
    if (!cursor_con.empty())
    {
        DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
        if (rsc != nullptr)
        {
            DrbdResource::ConnectionsIterator con_iter = rsc->connections_iterator();
            const bool prb_flag = is_problem_mode(rsc);
            while (con_iter.has_next())
            {
                DrbdConnection* const con = con_iter.next();
                if (!prb_flag || problem_filter(con))
                {
                    const std::string& con_name = con->get_name();
                    if (con_name > cursor_con)
                    {
                        cursor_con = con_name;
                        dsp_comp_hub.dsp_selector->refresh_display();
                        break;
                    }
                }
            }
        }
    }
}

void MDspConnections::cursor_to_previous_item()
{
    if (!cursor_con.empty())
    {
        DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
        if (rsc != nullptr)
        {
            DrbdConnection* prev_con = nullptr;
            DrbdResource::ConnectionsIterator con_iter = rsc->connections_iterator();
            const bool prb_flag = is_problem_mode(rsc);
            while (con_iter.has_next())
            {
                DrbdConnection* const con = con_iter.next();
                if (!prb_flag || problem_filter(con))
                {
                    if (con->get_name() >= cursor_con)
                    {
                        break;
                    }
                    prev_con = con;
                }
            }
            if (prev_con != nullptr)
            {
                cursor_con = prev_con->get_name();
                dsp_comp_hub.dsp_selector->refresh_display();
            }
        }
    }
}

void MDspConnections::list_item_clicked(MouseEvent& mouse)
{
    DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
    if (rsc != nullptr)
    {
        const uint32_t lines_per_page = get_lines_per_page();
        const uint32_t selected_line = mouse.coord_row - CON_LIST_Y;
        if (is_problem_mode(rsc))
        {
            uint32_t filtered_count = 0;
            DrbdResource::ConnectionsIterator con_iter = rsc->connections_iterator();
            DrbdConnection* const con = navigation::find_first_filtered_item_on_page(
                get_page_nr(), get_line_offset(), lines_per_page, con_iter, problem_filter, filtered_count
            );
            if (con != nullptr)
            {
                DrbdResource::ConnectionsIterator cursor_con_iter = rsc->connections_iterator(con->get_name());
                uint32_t line_ctr = 0;
                while (cursor_con_iter.has_next() && line_ctr <= selected_line)
                {
                    DrbdConnection* const cur_con = cursor_con_iter.next();
                    if (problem_filter(cur_con))
                    {
                        if (line_ctr == selected_line)
                        {
                            cursor_con = cur_con->get_name();
                            if (mouse.coord_column <= DisplayConsts::MAX_SELECT_X)
                            {
                                toggle_select_cursor_item();
                            }
                            dsp_comp_hub.dsp_selector->refresh_display();
                        }
                        ++line_ctr;
                    }
                }
            }
        }
        else
        {
            DrbdResource::ConnectionsIterator cursor_con_iter = rsc->connections_iterator();
            navigation::find_first_item_on_page(get_page_nr(), get_line_offset(), lines_per_page, cursor_con_iter);
            uint32_t line_ctr = 0;
            while (cursor_con_iter.has_next() && line_ctr <= selected_line)
            {
                DrbdConnection* const cur_con = cursor_con_iter.next();
                if (line_ctr == selected_line)
                {
                    cursor_con = cur_con->get_name();
                    if (mouse.coord_column <= DisplayConsts::MAX_SELECT_X)
                    {
                        toggle_select_cursor_item();
                    }
                    dsp_comp_hub.dsp_selector->refresh_display();
                }
                ++line_ctr;
            }
        }
    }
}

uint32_t MDspConnections::get_lines_per_page()
{
    return dsp_comp_hub.term_rows - CON_LIST_Y - 2;
}

void MDspConnections::synchronize_data()
{
    if (cursor_con.empty())
    {
        dsp_comp_hub.dsp_shared->clear_monitor_con();
    }
    else
    {
        dsp_comp_hub.dsp_shared->update_monitor_con(cursor_con);
    }
}

void MDspConnections::notify_data_updated()
{
    if (displayed_rsc != dsp_comp_hub.dsp_shared->monitor_rsc)
    {
        reset_display();
    }
    displayed_rsc = dsp_comp_hub.dsp_shared->monitor_rsc;
    cursor_con = dsp_comp_hub.dsp_shared->monitor_con;
    if (!is_cursor_nav())
    {
        set_page_nr(1);
    }
    dsp_comp_hub.dsp_selector->refresh_display();
}

uint64_t MDspConnections::get_update_mask() noexcept
{
    return update_event::UPDATE_FLAG_DRBD;
}
