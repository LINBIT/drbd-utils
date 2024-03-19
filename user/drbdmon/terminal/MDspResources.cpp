#include <terminal/MDspResources.h>
#include <terminal/KeyCodes.h>
#include <terminal/DisplayConsts.h>
#include <terminal/DisplaySelector.h>
#include <terminal/InputField.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/navigation.h>
#include <terminal/HelpText.h>
#include <terminal/GlobalCommandConsts.h>
#include <objects/DrbdVolume.h>
#include <objects/DrbdConnection.h>
#include <comparators.h>
#include <string_matching.h>
#include <bounds.h>

const uint8_t  MDspResources::RSC_HEADER_Y      = 3;
const uint8_t  MDspResources::RSC_LIST_Y        = 4;

MDspResources::MDspResources(const ComponentsHub& comp_hub):
    MDspStdListBase::MDspStdListBase(comp_hub)
{
    problem_filter =
        [](DrbdResource* rsc) -> bool
        {
            return rsc->has_mark_state() || rsc->has_warn_state() || rsc->has_alert_state();
        };
}

MDspResources::~MDspResources() noexcept
{
}

void MDspResources::display_activated()
{
    MDspStdListBase::display_activated();
    cursor_rsc = dsp_comp_hub.dsp_shared->monitor_rsc;
    dsp_comp_hub.dsp_shared->ovrd_resource_selection = false;
}

void MDspResources::display_deactivated()
{
    MDspStdListBase::display_deactivated();
}

void MDspResources::reset_display()
{
    MDspStdListBase::reset_display();
    cursor_rsc.clear();
    set_page_nr(1);
}

void MDspResources::display_list()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_RSC_LIST);
    display_rsc_header();

    if (is_cursor_nav())
    {
        display_at_cursor();
    }
    else
    {
        display_at_page();
    }
}

void MDspResources::display_rsc_header()
{
    uint32_t current_line = RSC_HEADER_Y;
    dsp_comp_hub.dsp_common->display_resource_header(current_line);
}

void MDspResources::display_at_cursor()
{
    const uint32_t lines_per_page = get_lines_per_page();

    ResourcesMap& selected_map = select_resources_map();
    dsp_comp_hub.dsp_common->display_problem_mode_label(&selected_map == dsp_comp_hub.prb_rsc_map);
    DrbdResource* const search_rsc = find_resource_near_cursor(selected_map);
    if (search_rsc != nullptr)
    {
        uint32_t rsc_idx = 0;
        uint32_t page_line_ctr  = 0;
        ResourcesMap::Node* page_first_node = nullptr;
        {
            ResourcesMap::NodesIterator nodes_iter(selected_map);
            while (nodes_iter.has_next())
            {
                ResourcesMap::Node* const cur_node = nodes_iter.next();
                if (page_line_ctr == 0)
                {
                    page_first_node = cur_node;
                }
                if (search_rsc == cur_node->get_value())
                {
                    break;
                }
                ++rsc_idx;
                ++page_line_ctr;
                if (page_line_ctr >= lines_per_page)
                {
                    page_line_ctr = 0;
                }
            }
        }

        set_page_nr((rsc_idx / lines_per_page) + 1);
        set_page_count(
            dsp_comp_hub.dsp_common->calculate_page_count(
                static_cast<uint32_t> (selected_map.get_size()),
                lines_per_page
            )
        );

        uint32_t line_nr = 0;
        if (page_first_node != nullptr)
        {
            ResourcesMap::ValuesIterator rsc_iter =
                ResourcesMap::ValuesIterator(selected_map, *page_first_node);

            const bool selecting = dsp_comp_hub.dsp_shared->have_resources_selection();
            uint32_t current_line = RSC_LIST_Y;
            while (rsc_iter.has_next() && line_nr < lines_per_page)
            {
                DrbdResource* const rsc = rsc_iter.next();
                write_resource_line(rsc, current_line, selecting);
                ++line_nr;
            }
        }

        if (line_nr == 0)
        {
            write_no_resources_line(&selected_map == dsp_comp_hub.prb_rsc_map);
        }
    }
    else
    {
        // Zero resources
        set_page_nr(1);
        set_line_offset(0);
        write_no_resources_line(&selected_map == dsp_comp_hub.prb_rsc_map);
    }

    display_common_unfiltered_stats();
}

void MDspResources::display_at_page()
{
    ResourcesMap& dsp_rsc_map = select_resources_map();
    dsp_comp_hub.dsp_common->display_problem_mode_label(&dsp_rsc_map == dsp_comp_hub.prb_rsc_map);

    const uint32_t lines_per_page = get_lines_per_page();
    set_page_count(
        dsp_comp_hub.dsp_common->calculate_page_count(
            static_cast<uint32_t> (dsp_rsc_map.get_size()),
            lines_per_page
        )
    );

    ResourcesMap::ValuesIterator rsc_iter(dsp_rsc_map);
    navigation::find_first_item_on_page(get_page_nr(), get_line_offset(), lines_per_page, rsc_iter);

    // Display all resources on the selected page, or the last page that shows any resources
    const bool selecting = dsp_comp_hub.dsp_shared->have_resources_selection();
    uint32_t line_nr = 0;
    uint32_t current_line = RSC_LIST_Y;
    while (rsc_iter.has_next() && line_nr < lines_per_page)
    {
        DrbdResource* const rsc = rsc_iter.next();
        write_resource_line(rsc, current_line, selecting);
        ++line_nr;
    }

    if (line_nr == 0)
    {
        write_no_resources_line(&dsp_rsc_map == dsp_comp_hub.prb_rsc_map);
    }

    display_common_unfiltered_stats();
}

void MDspResources::display_common_unfiltered_stats()
{
    dsp_comp_hub.dsp_io->cursor_xy(1, dsp_comp_hub.term_rows - 2);
    dsp_comp_hub.dsp_io->write_fmt("%lu resources", static_cast<unsigned int> (dsp_comp_hub.rsc_map->get_size()));

    const uint64_t problem_count = dsp_comp_hub.core_instance->get_problem_count();
    if (problem_count >= 1)
    {
        dsp_comp_hub.dsp_io->write_fmt(
            " %s(%lu degraded)%s",
            dsp_comp_hub.active_color_table->alert.c_str(),
            static_cast<unsigned int> (problem_count),
            dsp_comp_hub.active_color_table->rst.c_str()
        );
    }
}

bool MDspResources::is_cursor_nav()
{
    return !cursor_rsc.empty();
}

void MDspResources::reset_cursor_position()
{
    cursor_rsc.clear();
    const uint32_t lines_per_page = get_lines_per_page();
    ResourcesMap& rsc_map = select_resources_map();
    ResourcesMap::ValuesIterator rsc_iter(rsc_map);
    navigation::find_first_item_on_page(get_page_nr(), get_line_offset(), lines_per_page, rsc_iter);
    if (rsc_iter.has_next())
    {
        const DrbdResource* const rsc = rsc_iter.next();
        cursor_rsc = rsc->get_name();
    }
}

void MDspResources::clear_cursor()
{
    cursor_rsc.clear();
}

bool MDspResources::is_selecting()
{
    return dsp_comp_hub.dsp_shared->have_resources_selection();
}

void MDspResources::write_resource_line(DrbdResource* const rsc, uint32_t& current_line, const bool selecting)
{
    DisplayIo* const dsp_io = dsp_comp_hub.dsp_io;
    dsp_io->cursor_xy(1, current_line);
    const std::string& rsc_name = rsc->get_name();

    const bool has_mark_state   = rsc->has_mark_state();
    const bool has_alert_state  = rsc->has_alert_state();
    const bool has_warn_state   = rsc->has_warn_state();

    bool is_under_cursor = false;
    if (is_cursor_nav())
    {
        if (cursor_rsc == rsc_name)
        {
            is_under_cursor = true;
        }
    }

    bool is_selected = false;
    if (selecting)
    {
        is_selected = dsp_comp_hub.dsp_shared->is_resource_selected(rsc_name);
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

    // Alert symbol
    if (has_mark_state || has_alert_state)
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
        dsp_io->write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
        dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
        dsp_io->write_text(rst_bg.c_str());
    }
    else
    if (has_warn_state)
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->warn.c_str());
        dsp_io->write_text(dsp_comp_hub.active_character_table->sym_warn.c_str());
        dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
        dsp_io->write_text(rst_bg.c_str());
    }
    else
    {
        dsp_io->write_char(' ');
    }

    dsp_io->write_char(' ');

    // Resource role symbol
    {
        const DrbdResource::resource_role rsc_role = rsc->get_role();
        if (rsc_role == DrbdResource::resource_role::PRIMARY)
        {
            dsp_io->write_text(dsp_comp_hub.active_character_table->sym_pri.c_str());
        }
        else
        if (rsc_role == DrbdResource::resource_role::SECONDARY)
        {
            dsp_io->write_text(dsp_comp_hub.active_character_table->sym_sec.c_str());
        }
        else
        {
            dsp_io->write_text(dsp_comp_hub.active_color_table->warn.c_str());
            dsp_io->write_text(dsp_comp_hub.active_character_table->sym_unknown.c_str());
            dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
            dsp_io->write_text(rst_bg.c_str());
        }
    }

    dsp_io->write_char(' ');

    if (has_mark_state || has_alert_state)
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
    }
    else
    if (has_warn_state)
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->warn.c_str());
    }
    else
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->rsc_name.c_str());
    }

    // Resource name field (starts at column 6)
    dsp_io->write_string_field(rsc_name, dsp_comp_hub.term_cols - 33, true);

    dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
    dsp_io->write_text(rst_bg.c_str());
    dsp_io->write_char(' ');

    // Volumes status fields
    {
        const uint16_t vlm_count = rsc->get_volume_count();
        bool have_vlm_alerts = false;
        uint16_t norm_count = 0;
        if (has_mark_state)
        {
            DrbdResource::VolumesIterator vlm_iter = rsc->volumes_iterator();
            while (vlm_iter.has_next())
            {
                const DrbdVolume* const vlm = vlm_iter.next();
                if (!(vlm->has_alert_state() || vlm->has_warn_state() || vlm->has_mark_state()))
                {
                    ++norm_count;
                }
                else
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

        dsp_io->write_text(dsp_comp_hub.active_color_table->vlm_count.c_str());
        dsp_io->write_char(' ');

        dsp_io->write_fmt("%5u/%5u", static_cast<unsigned int> (norm_count), static_cast<unsigned int> (vlm_count));
        dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
        dsp_io->write_text(rst_bg.c_str());
    }

    dsp_io->write_char(' ');

    // Connections status fields
    {
        const uint8_t con_count = rsc->get_connection_count();
        bool have_con_alerts = false;
        bool have_con_warnings = false;
        uint8_t norm_count = 0;
        if (has_mark_state)
        {
            DrbdResource::ConnectionsIterator con_iter = rsc->connections_iterator();
            while (con_iter.has_next())
            {
                const DrbdConnection* const con = con_iter.next();
                if (con->has_alert_state())
                {
                    have_con_alerts = true;
                }
                else
                {
                    if (con->has_warn_state() || con->has_mark_state())
                    {
                        have_con_warnings = true;
                    }
                    ++norm_count;
                }
            }
        }
        else
        {
            norm_count = con_count;
        }

        if (have_con_alerts)
        {
            dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
            dsp_io->write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
            dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
            dsp_io->write_text(rst_bg.c_str());
        }
        else
        if (have_con_warnings)
        {
            dsp_io->write_text(dsp_comp_hub.active_color_table->warn.c_str());
            dsp_io->write_text(dsp_comp_hub.active_character_table->sym_warn.c_str());
            dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
            dsp_io->write_text(rst_bg.c_str());
        }
        else
        {
            dsp_io->write_char(' ');
        }

        dsp_io->write_text(dsp_comp_hub.active_color_table->con_count.c_str());
        dsp_io->write_char(' ');

        dsp_io->write_fmt("%2u/%2u", static_cast<unsigned int> (norm_count), static_cast<unsigned int> (con_count));
        dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
        dsp_io->write_text(rst_bg.c_str());
    }

    dsp_io->write_char(' ');

    // Quorum status field
    if (rsc->has_quorum_alert())
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
        dsp_io->write_char('N');
        dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
        dsp_io->write_text(rst_bg.c_str());
    }
    else
    {
        dsp_io->write_char('Y');
    }
    dsp_io->write_text("  ");

    dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
    ++current_line;
}

void MDspResources::write_no_resources_line(const bool problem_mode_flag)
{
    dsp_comp_hub.dsp_io->cursor_xy(1, RSC_LIST_Y);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
    if (problem_mode_flag)
    {
        dsp_comp_hub.dsp_io->write_text("No resources with problem status");
    }
    else
    {
        dsp_comp_hub.dsp_io->write_text("No active resources");
    }
}

void MDspResources::cursor_to_previous_item()
{
    if (cursor_rsc.length() >= 1)
    {
        ResourcesMap& selected_map = select_resources_map();
        const DrbdResource* const prev_rsc = selected_map.get_less_value(&cursor_rsc);
        if (prev_rsc != nullptr)
        {
            cursor_rsc = prev_rsc->get_name();
            dsp_comp_hub.dsp_selector->refresh_display();
        }
    }
}

void MDspResources::cursor_to_next_item()
{
    if (cursor_rsc.length() >= 1)
    {
        ResourcesMap& selected_map = select_resources_map();
        const DrbdResource* const next_rsc = selected_map.get_greater_value(&cursor_rsc);
        if (next_rsc != nullptr)
        {
            cursor_rsc = next_rsc->get_name();
            dsp_comp_hub.dsp_selector->refresh_display();
        }
    }
}

bool MDspResources::key_pressed(const uint32_t key)
{
    bool intercepted = MDspStdListBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == static_cast<uint32_t> ('P') || key == static_cast<uint32_t> ('p'))
        {
            dsp_comp_hub.dsp_common->toggle_problem_mode();
            dsp_comp_hub.dsp_selector->refresh_display();
            intercepted = true;
        }
        else
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::RSC_LIST, dsp_comp_hub);
        }
        else
        if (is_cursor_nav())
        {
            if (key == static_cast<uint32_t> ('v') || key == static_cast<uint32_t> ('V'))
            {
                dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::VLM_LIST);
                intercepted = true;
            }
            else
            if (key == static_cast<uint32_t> ('c') || key == static_cast<uint32_t> ('C'))
            {
                dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::CON_LIST);
                intercepted = true;
            }
            else
            if (key == KeyCodes::ENTER)
            {
                dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::RSC_DETAIL);
                intercepted = true;
            }
        }

        if (!intercepted && (dsp_comp_hub.dsp_shared->have_resources_selection() || is_cursor_nav()))
        {
            if (key == static_cast<uint32_t> ('a') || key == static_cast<uint32_t> ('A'))
            {
                dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::RSC_ACTIONS);
                intercepted = true;
            }
        }
    }
    return intercepted;
}

bool MDspResources::mouse_action(MouseEvent& mouse)
{
    bool intercepted = MDspStdListBase::mouse_action(mouse);
    if (!intercepted)
    {
        if ((mouse.button == MouseEvent::button_id::BUTTON_01 || mouse.button == MouseEvent::button_id::BUTTON_03) &&
            mouse.event == MouseEvent::event_id::MOUSE_RELEASE &&
            mouse.coord_row >= RSC_LIST_Y)
        {
            const uint32_t lines_per_page = get_lines_per_page();
            if (mouse.coord_row < RSC_LIST_Y + lines_per_page)
            {
                list_item_clicked(mouse);
                if (mouse.button == MouseEvent::button_id::BUTTON_03 && is_cursor_nav())
                {
                    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::RSC_DETAIL);
                }
                intercepted = true;
            }
        }
    }
    return intercepted;
}

bool MDspResources::execute_command(const std::string& command, StringTokenizer& tokenizer)
{
    bool accepted = false;
    if (command == cmd_names::KEY_CMD_CURSOR)
    {
        if (tokenizer.has_next())
        {
            const std::string cmd_arg = tokenizer.next();
            if (!cmd_arg.empty())
            {
                if (string_matching::is_pattern(cmd_arg))
                {
                    dsp_comp_hub.dsp_common->application_working();
                    try
                    {
                        std::unique_ptr<string_matching::PatternItem> pattern;
                        string_matching::process_pattern(cmd_arg, pattern);

                        ResourcesMap& selected_map = select_resources_map();
                        ResourcesMap::KeysIterator rsc_iter(selected_map);
                        while (!accepted && rsc_iter.has_next())
                        {
                            const std::string* const rsc_name_ptr = rsc_iter.next();
                            const std::string& rsc_name = *rsc_name_ptr;
                            accepted = string_matching::match_text(rsc_name, pattern.get());
                            if (accepted)
                            {
                                cursor_rsc = rsc_name;
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
                    accepted = dsp_comp_hub.global_cmd_exec->execute_command(cmd_names::KEY_CMD_RESOURCE, tokenizer);
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
                dsp_comp_hub.dsp_common->application_working();
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
                    dsp_comp_hub.dsp_shared->select_resource(cmd_arg);
                }
                else
                {
                    dsp_comp_hub.dsp_shared->deselect_resource(cmd_arg);
                }
                accepted = true;
            }
        }
        return accepted;
    }
    else
    if (command == cmd_names::KEY_CMD_SELECT_ALL)
    {
        dsp_comp_hub.dsp_common->application_working();
        ResourcesMap& selected_map = select_resources_map();
        ResourcesMap::KeysIterator rsc_iter(selected_map);
        while (rsc_iter.has_next())
        {
            const std::string* const rsc_name = rsc_iter.next();
            dsp_comp_hub.dsp_shared->select_resource(*rsc_name);
        }
        accepted = true;
    }
    else
    if (command == cmd_names::KEY_CMD_DESELECT_ALL || command == cmd_names::KEY_CMD_CLEAR_SELECTION)
    {
        dsp_comp_hub.dsp_common->application_working();
        clear_selection();
        accepted = true;
    }
    if (accepted)
    {
        dsp_comp_hub.dsp_selector->refresh_display();
    }
    else
    {
        dsp_comp_hub.dsp_common->application_idle();
        reposition_text_cursor();
    }
    return accepted;
}

void MDspResources::list_item_clicked(MouseEvent& mouse)
{
    const uint32_t lines_per_page = get_lines_per_page();
    ResourcesMap& selected_map = select_resources_map();
    ResourcesMap::ValuesIterator rsc_iter(selected_map);
    navigation::find_first_item_on_page(get_page_nr(), get_line_offset(), lines_per_page, rsc_iter);

    const uint32_t selected_line = mouse.coord_row - RSC_LIST_Y;
    uint32_t line_ctr = 0;
    while (rsc_iter.has_next() && line_ctr < selected_line)
    {
        rsc_iter.next();
        ++line_ctr;
    }
    if (rsc_iter.has_next())
    {
        DrbdResource* const selected_rsc = rsc_iter.next();
        cursor_rsc = selected_rsc->get_name();
        if (mouse.coord_column <= DisplayConsts::MAX_SELECT_X)
        {
            toggle_select_cursor_item();
        }
        dsp_comp_hub.dsp_selector->refresh_display();
    }
}

uint32_t MDspResources::get_lines_per_page()
{
    return dsp_comp_hub.term_rows - RSC_LIST_Y - 2;
}

DrbdResource* MDspResources::find_resource_near_cursor(ResourcesMap& selected_map)
{
    return (cursor_rsc.length() >= 1 ? navigation::find_item_near_cursor(selected_map, &cursor_rsc) : nullptr);
}

ResourcesMap& MDspResources::select_resources_map() const
{
    ResourcesMap* selected_map = dsp_comp_hub.rsc_map;
    DisplayCommon::problem_mode_type problem_mode = dsp_comp_hub.dsp_common->get_problem_mode();
    if (
        problem_mode == DisplayCommon::problem_mode_type::LOCK ||
        (problem_mode == DisplayCommon::problem_mode_type::AUTO &&
        dsp_comp_hub.prb_rsc_map->get_size() >= 1)
    )
    {
        selected_map = dsp_comp_hub.prb_rsc_map;
    }
    return *selected_map;
}

// Toggle selection of the resource currently under the cursor,
// if the resource is currently active
void MDspResources::toggle_select_cursor_item()
{
    if (cursor_rsc.length() >= 1)
    {
        DrbdResource* const rsc = dsp_comp_hub.rsc_map->get(&cursor_rsc);
        if (rsc != nullptr)
        {
            dsp_comp_hub.dsp_shared->toggle_resource_selection(cursor_rsc);
            dsp_comp_hub.dsp_selector->refresh_display();
        }
    }
}

// @throws std::bad_alloc, string_matching::PatternLimitException
bool MDspResources::change_selection(const std::string& pattern_text, const bool select_flag)
{
    ResourcesMap& selected_map = select_resources_map();
    std::unique_ptr<string_matching::PatternItem> pattern;
    string_matching::process_pattern(pattern_text, pattern);

    bool matched = false;
    ResourcesMap::KeysIterator rsc_iter(selected_map);
    while (rsc_iter.has_next())
    {
        const std::string* const rsc_name_ptr = rsc_iter.next();
        const std::string& rsc_name = *rsc_name_ptr;
        if (string_matching::match_text(rsc_name, pattern.get()))
        {
            matched = true;
            if (select_flag)
            {
                dsp_comp_hub.dsp_shared->select_resource(rsc_name);
            }
            else
            {
                dsp_comp_hub.dsp_shared->deselect_resource(rsc_name);
            }
        }
    }
    return matched;
}

// Public entry point to private/non-virtual clear_selection_impl, which is also used by the destructor
void MDspResources::clear_selection()
{
    dsp_comp_hub.dsp_shared->clear_resources_selection();
}

void MDspResources::synchronize_data()
{
    if (cursor_rsc.empty())
    {
        dsp_comp_hub.dsp_shared->clear_monitor_rsc();
    }
    else
    {
        dsp_comp_hub.dsp_shared->update_monitor_rsc(cursor_rsc);
    }
}

void MDspResources::notify_data_updated()
{
    cursor_rsc = dsp_comp_hub.dsp_shared->monitor_rsc;
    if (!is_cursor_nav())
    {
        set_page_nr(1);
    }
    dsp_comp_hub.dsp_selector->refresh_display();
}

uint64_t MDspResources::get_update_mask() noexcept
{
    return update_event::UPDATE_FLAG_DRBD;
}

