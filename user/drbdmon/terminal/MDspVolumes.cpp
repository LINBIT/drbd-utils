#include <terminal/MDspVolumes.h>
#include <terminal/KeyCodes.h>
#include <terminal/DisplayConsts.h>
#include <terminal/DisplaySelector.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/navigation.h>
#include <terminal/HelpText.h>
#include <terminal/GlobalCommandConsts.h>
#include <objects/DrbdConnection.h>
#include <comparators.h>
#include <bounds.h>
#include <integerparse.h>
#include <dsaext.h>
#include <functional>

const uint8_t   MDspVolumes::RSC_HEADER_Y   = 3;
const uint8_t   MDspVolumes::VLM_HEADER_Y   = 5;
const uint8_t   MDspVolumes::VLM_LIST_Y     = 6;

MDspVolumes::MDspVolumes(const ComponentsHub& comp_hub):
    MDspStdListBase::MDspStdListBase(comp_hub)
{
    problem_filter =
        [](DrbdVolume* vlm) -> bool
        {
            return vlm->has_mark_state() || vlm->has_warn_state() || vlm->has_alert_state();
        };
    vlm_key_func =
        [](DrbdVolume* vlm) -> const uint16_t&
        {
            return vlm->get_volume_nr_ref();
        };
}

MDspVolumes::~MDspVolumes() noexcept
{
}

void MDspVolumes::display_activated()
{
    MDspStdListBase::display_activated();
    if (displayed_rsc != dsp_comp_hub.dsp_shared->monitor_rsc)
    {
        reset_display();
    }
    displayed_rsc = dsp_comp_hub.dsp_shared->monitor_rsc;
    dsp_comp_hub.dsp_shared->ovrd_volume_selection = false;
}

void MDspVolumes::display_deactivated()
{
    MDspStdListBase::display_deactivated();
}

void MDspVolumes::reset_display()
{
    MDspStdListBase::reset_display();
    cursor_vlm = DisplayConsts::VLM_NONE;
    clear_selection();
    set_page_nr(1);
}

void MDspVolumes::display_list()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_VLM_LIST);

    uint32_t current_line = RSC_HEADER_Y;
    dsp_comp_hub.dsp_common->display_resource_header(current_line);
    dsp_comp_hub.dsp_common->display_resource_line(current_line);

    display_volume_header();

    if (is_cursor_nav())
    {
        display_at_cursor();
    }
    else
    {
        display_at_page();
    }
}

void MDspVolumes::display_at_cursor()
{
    DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
    if (rsc != nullptr)
    {
        const uint16_t vlm_count = rsc->get_volume_count();
        const bool problem_mode_flag = is_problem_mode(rsc);
        dsp_comp_hub.dsp_common->display_problem_mode_label(problem_mode_flag);

        const uint32_t lines_per_page = get_lines_per_page();
        uint32_t line_nr = 0;
        DrbdResource::VolumesIterator vlm_iter = rsc->volumes_iterator();
        if (problem_mode_flag)
        {
            // Show volumes with some problem state

            uint32_t dsp_vlm_idx = 0;
            uint32_t dsp_vlm_count = 0;

            DrbdVolume* const page_first_vlm = navigation::find_first_filtered_item_on_cursor_page(
                cursor_vlm,
                vlm_key_func,
                vlm_iter,
                problem_filter,
                lines_per_page,
                dsp_vlm_idx,
                dsp_vlm_count
            );

            set_page_nr((dsp_vlm_idx / lines_per_page) + 1);
            set_page_count(dsp_comp_hub.dsp_common->calculate_page_count(dsp_vlm_count, lines_per_page));

            if (page_first_vlm != nullptr)
            {
                const bool selecting = dsp_comp_hub.dsp_shared->have_volumes_selection();
                DrbdResource::VolumesIterator dsp_vlm_iter =
                    rsc->volumes_iterator(page_first_vlm->get_volume_nr());
                uint32_t current_line = VLM_LIST_Y;
                while (dsp_vlm_iter.has_next() && line_nr < lines_per_page)
                {
                    DrbdVolume* const vlm = dsp_vlm_iter.next();
                    if (problem_filter(vlm))
                    {
                        dsp_comp_hub.dsp_io->cursor_xy(1, VLM_LIST_Y + line_nr);
                        write_volume_line(rsc, vlm, current_line, selecting);
                        ++line_nr;
                    }
                }
            }
        }
        else
        {
            // Show all volumes

            uint32_t dsp_vlm_idx = 0;

            DrbdVolume* const page_first_vlm = navigation::find_first_item_on_cursor_page(
                cursor_vlm,
                vlm_key_func,
                vlm_iter,
                lines_per_page,
                dsp_vlm_idx
            );

            set_page_nr((dsp_vlm_idx / lines_per_page) + 1);
            set_page_count(dsp_comp_hub.dsp_common->calculate_page_count(vlm_count, lines_per_page));

            if (page_first_vlm != nullptr)
            {
                const bool selecting = dsp_comp_hub.dsp_shared->have_volumes_selection();
                DrbdResource::VolumesIterator dsp_vlm_iter =
                    rsc->volumes_iterator(page_first_vlm->get_volume_nr());
                uint32_t current_line = VLM_LIST_Y;
                while (dsp_vlm_iter.has_next() && line_nr < lines_per_page)
                {
                    DrbdVolume* const vlm = dsp_vlm_iter.next();
                    dsp_comp_hub.dsp_io->cursor_xy(1, VLM_LIST_Y + line_nr);
                    write_volume_line(rsc, vlm, current_line, selecting);
                    ++line_nr;
                }
            }
        }

        if (line_nr == 0)
        {
            write_no_volumes_line(problem_mode_flag);
        }
    }
    else
    {
        // Resource not present
        dsp_comp_hub.dsp_io->cursor_xy(1, VLM_LIST_Y);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
        dsp_comp_hub.dsp_io->write_text("N/A");
        dsp_comp_hub.dsp_common->display_problem_mode_label(false);
    }
}

void MDspVolumes::display_at_page()
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
            // Display all problem state volumes on the selected page, or the last page that shows any volumes
            uint32_t dsp_vlm_count = 0;
            DrbdResource::VolumesIterator vlm_iter = dsp_rsc->volumes_iterator();
            DrbdVolume* const page_first_vlm = navigation::find_first_filtered_item_on_page(
                get_page_nr(), get_line_offset(), lines_per_page, vlm_iter, problem_filter, dsp_vlm_count
            );
            if (page_first_vlm != nullptr)
            {
                const bool selecting = dsp_comp_hub.dsp_shared->have_volumes_selection();
                DrbdResource::VolumesIterator dsp_vlm_iter =
                    dsp_rsc->volumes_iterator(page_first_vlm->get_volume_nr());
                uint32_t current_line = VLM_LIST_Y;
                while (dsp_vlm_iter.has_next() && line_nr < lines_per_page)
                {
                    DrbdVolume* const dsp_vlm = dsp_vlm_iter.next();
                    if (problem_filter(dsp_vlm))
                    {
                        dsp_io->cursor_xy(1, VLM_LIST_Y + line_nr);
                        write_volume_line(dsp_rsc, dsp_vlm, current_line, selecting);
                        ++line_nr;
                    }
                }
            }
        }
        else
        {
            const bool selecting = dsp_comp_hub.dsp_shared->have_volumes_selection();
            // Display all volumes on the selected page, or the last page that shows any volumes
            DrbdResource::VolumesIterator vlm_iter = dsp_rsc->volumes_iterator();
            set_page_count(
                dsp_comp_hub.dsp_common->calculate_page_count(
                    static_cast<uint32_t> (vlm_iter.get_size()),
                    lines_per_page
                )
            );

            navigation::find_first_item_on_page(get_page_nr(), get_line_offset(), lines_per_page, vlm_iter);
            uint32_t current_line = VLM_LIST_Y;
            while (vlm_iter.has_next() && line_nr < lines_per_page)
            {
                DrbdVolume* const vlm = vlm_iter.next();
                dsp_io->cursor_xy(1, VLM_LIST_Y + line_nr);
                write_volume_line(dsp_rsc, vlm, current_line, selecting);
                ++line_nr;
            }
        }

        if (line_nr == 0)
        {
            write_no_volumes_line(problem_mode_flag);
        }
    }
    else
    {
        // Resource not present
        dsp_comp_hub.dsp_io->cursor_xy(1, VLM_LIST_Y);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
        dsp_comp_hub.dsp_io->write_text("N/A");
        dsp_comp_hub.dsp_common->display_problem_mode_label(false);
    }
}

void MDspVolumes::display_volume_header()
{
    DisplayIo* const dsp_io = dsp_comp_hub.dsp_io;

    dsp_io->cursor_xy(1, VLM_HEADER_Y);
    dsp_io->write_text(dsp_comp_hub.active_color_table->list_header.c_str());
    dsp_io->write_text(dsp_comp_hub.ansi_ctl->ANSI_CLEAR_LINE.c_str());

    dsp_io->cursor_xy(1, VLM_HEADER_Y);
    dsp_io->write_text("   VlmNr    MinorNr");

    dsp_io->cursor_xy(24, VLM_HEADER_Y);
    dsp_io->write_text("  DiskState");

    dsp_io->cursor_xy(47, VLM_HEADER_Y);
    dsp_io->write_text("Qrm");

    dsp_io->cursor_xy(51, VLM_HEADER_Y);
    dsp_io->write_text("Sync%");

    dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
}

bool MDspVolumes::key_pressed(const uint32_t key)
{
    bool intercepted = MDspStdListBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::VLM_LIST, dsp_comp_hub);
            intercepted = true;
        }
        else
        if (key == static_cast<uint32_t> ('r') || key == static_cast<uint32_t> ('R'))
        {
            dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::RSC_LIST);
            intercepted = true;
        }
        else
        if (key == static_cast<uint32_t> ('c') || key == static_cast<uint32_t> ('C'))
        {
            dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::CON_LIST);
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
                dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::VLM_DETAIL);
                intercepted = true;
            }
        }

        if (!intercepted && (is_cursor_nav() || dsp_comp_hub.dsp_shared->have_volumes_selection()))
        {
            if (key == static_cast<uint32_t> ('A') || key == static_cast<uint32_t> ('a'))
            {
                dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::VLM_ACTIONS);
                intercepted = true;
            }
        }
    }
    return intercepted;
}

bool MDspVolumes::mouse_action(MouseEvent& mouse)
{
    bool intercepted = MDspStdListBase::mouse_action(mouse);
    if (!intercepted)
    {
        if ((mouse.button == MouseEvent::button_id::BUTTON_01 || mouse.button == MouseEvent::button_id::BUTTON_03) &&
            mouse.event == MouseEvent::event_id::MOUSE_RELEASE &&
            mouse.coord_row >= VLM_LIST_Y)
        {
            const uint32_t lines_per_page = get_lines_per_page();
            if (mouse.coord_row < VLM_LIST_Y + lines_per_page)
            {
                list_item_clicked(mouse);
                if (mouse.button == MouseEvent::button_id::BUTTON_03 && is_cursor_nav())
                {
                    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::VLM_DETAIL);
                }
                intercepted = true;
            }
        }
    }
    return intercepted;
}

bool MDspVolumes::execute_command(const std::string& command, StringTokenizer& tokenizer)
{
    bool accepted = false;
    if (command == cmd_names::KEY_CMD_CURSOR)
    {
        if (tokenizer.has_next())
        {
            const std::string obj_id = tokenizer.next();
            try
            {
                const uint16_t vlm_id = dsaext::parse_unsigned_int16(obj_id);
                cursor_vlm = vlm_id;
                accepted = true;
            }
            catch (dsaext::NumberFormatException&)
            {
                // ignored, command rejected
            }
        }
    }
    else
    if (command == cmd_names::KEY_CMD_SELECT || command == cmd_names::KEY_CMD_DESELECT)
    {
        if (tokenizer.has_next())
        {
            const std::string obj_id = tokenizer.next();
            try
            {
                const uint16_t vlm_id = dsaext::parse_unsigned_int16(obj_id);
                if (command == cmd_names::KEY_CMD_SELECT)
                {
                    dsp_comp_hub.dsp_shared->select_volume(vlm_id);
                }
                else
                {
                    dsp_comp_hub.dsp_shared->deselect_volume(vlm_id);
                }
                accepted = true;
            }
            catch (dsaext::NumberFormatException&)
            {
                // ignored, command rejected
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
            DrbdResource::VolumesIterator vlm_iter = rsc->volumes_iterator();
            while (vlm_iter.has_next())
            {
                DrbdVolume* const vlm = vlm_iter.next();
                if (!prb_mode || problem_filter(vlm))
                {
                    const uint16_t vlm_nr = vlm->get_volume_nr();
                    dsp_comp_hub.dsp_shared->select_volume(vlm_nr);
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

void MDspVolumes::list_item_clicked(MouseEvent& mouse)
{
    DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
    if (rsc != nullptr)
    {
        const uint32_t lines_per_page = get_lines_per_page();
        const uint32_t selected_line = mouse.coord_row - VLM_LIST_Y;
        if (is_problem_mode(rsc))
        {
            uint32_t filtered_count = 0;
            DrbdResource::VolumesIterator vlm_iter = rsc->volumes_iterator();
            DrbdVolume* const vlm = navigation::find_first_filtered_item_on_page(
                get_page_nr(), get_line_offset(), lines_per_page, vlm_iter, problem_filter, filtered_count
            );
            if (vlm != nullptr)
            {
                DrbdResource::VolumesIterator cursor_vlm_iter = rsc->volumes_iterator(vlm->get_volume_nr());
                uint32_t line_ctr = 0;
                while (cursor_vlm_iter.has_next() && line_ctr <= selected_line)
                {
                    DrbdVolume* const cur_vlm = cursor_vlm_iter.next();
                    if (problem_filter(cur_vlm))
                    {
                        if (line_ctr == selected_line)
                        {
                            cursor_vlm = cur_vlm->get_volume_nr();
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
            DrbdResource::VolumesIterator cursor_vlm_iter = rsc->volumes_iterator();
            navigation::find_first_item_on_page(get_page_nr(), get_line_offset(), lines_per_page, cursor_vlm_iter);
            uint32_t line_ctr = 0;
            while (cursor_vlm_iter.has_next() && line_ctr <= selected_line)
            {
                DrbdVolume* const cur_vlm = cursor_vlm_iter.next();
                if (line_ctr == selected_line)
                {
                    cursor_vlm = cur_vlm->get_volume_nr();
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

bool MDspVolumes::is_cursor_nav()
{
    return cursor_vlm != DisplayConsts::VLM_NONE;
}

void MDspVolumes::clear_cursor()
{
    cursor_vlm = DisplayConsts::VLM_NONE;
}

bool MDspVolumes::is_selecting()
{
    return dsp_comp_hub.dsp_shared->have_volumes_selection();
}

void MDspVolumes::clear_selection()
{
    dsp_comp_hub.dsp_shared->clear_volumes_selection();
}

// Toggle selection of the volume currently under the cursor,
// if the volume is currently active
void MDspVolumes::toggle_select_cursor_item()
{
    if (dsp_comp_hub.dsp_shared->monitor_rsc.length() >= 1 && cursor_vlm != DisplayConsts::VLM_NONE)
    {
        DrbdResource* const rsc = dsp_comp_hub.rsc_map->get(&(dsp_comp_hub.dsp_shared->monitor_rsc));
        if (rsc != nullptr)
        {
            DrbdVolume* const vlm = rsc->get_volume(cursor_vlm);
            if (vlm != nullptr)
            {
                dsp_comp_hub.dsp_shared->toggle_volume_selection(cursor_vlm);
                dsp_comp_hub.dsp_selector->refresh_display();
            }
        }
    }
}

void MDspVolumes::reset_cursor_position()
{
    cursor_vlm = DisplayConsts::VLM_NONE;
    const uint32_t lines_per_page = get_lines_per_page();
    if (dsp_comp_hub.dsp_shared->monitor_rsc.length() >= 1)
    {
        DrbdResource* const rsc = dsp_comp_hub.rsc_map->get(&(dsp_comp_hub.dsp_shared->monitor_rsc));
        if (rsc != nullptr)
        {
            DrbdResource::VolumesIterator vlm_iter = rsc->volumes_iterator();
            if (is_problem_mode(rsc))
            {
                uint32_t filtered_count = 0;
                DrbdVolume* const vlm = navigation::find_first_filtered_item_on_page(
                    get_page_nr(), get_line_offset(), lines_per_page, vlm_iter, problem_filter, filtered_count
                );
                if (vlm != nullptr)
                {
                    cursor_vlm = vlm->get_volume_nr();
                }
            }
            else
            {
                navigation::find_first_item_on_page(get_page_nr(), get_line_offset(), lines_per_page, vlm_iter);
                if (vlm_iter.has_next())
                {
                    const DrbdVolume* const vlm = vlm_iter.next();
                    cursor_vlm = vlm->get_volume_nr();
                }
            }
        }
    }
}

void MDspVolumes::cursor_to_next_item()
{
    if (cursor_vlm != DisplayConsts::VLM_NONE)
    {
        DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
        if (rsc != nullptr)
        {
            DrbdResource::VolumesIterator vlm_iter = rsc->volumes_iterator();
            const bool prb_flag = is_problem_mode(rsc);
            while (vlm_iter.has_next())
            {
                DrbdVolume* const vlm = vlm_iter.next();
                if (!prb_flag || problem_filter(vlm))
                {
                    const uint16_t vlm_nr = vlm->get_volume_nr();
                    if (vlm_nr > cursor_vlm)
                    {
                        cursor_vlm = vlm_nr;
                        dsp_comp_hub.dsp_selector->refresh_display();
                        break;
                    }
                }
            }
        }
    }
}

void MDspVolumes::cursor_to_previous_item()
{
    if (cursor_vlm != DisplayConsts::VLM_NONE)
    {
        DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
        if (rsc != nullptr)
        {
            uint16_t prev_vlm_nr = DisplayConsts::VLM_NONE;
            DrbdResource::VolumesIterator vlm_iter = rsc->volumes_iterator();
            const bool prb_flag = is_problem_mode(rsc);
            while (vlm_iter.has_next())
            {
                DrbdVolume* const vlm = vlm_iter.next();
                if (!prb_flag || problem_filter(vlm))
                {
                    const uint16_t vlm_nr = vlm->get_volume_nr();
                    if (vlm_nr >= cursor_vlm)
                    {
                        break;
                    }
                    prev_vlm_nr = vlm_nr;
                }
            }
            if (prev_vlm_nr != DisplayConsts::VLM_NONE)
            {
                cursor_vlm = prev_vlm_nr;
                dsp_comp_hub.dsp_selector->refresh_display();
            }
        }
    }
}

void MDspVolumes::write_volume_line(
    DrbdResource* const rsc,
    DrbdVolume* const   vlm,
    uint32_t&           current_line,
    const bool          selecting
)
{
    DisplayIo* const dsp_io = dsp_comp_hub.dsp_io;
    dsp_io->cursor_xy(1, current_line);

    const uint16_t vlm_nr = vlm->get_volume_nr();

    bool is_under_cursor = false;
    if (is_cursor_nav())
    {
        is_under_cursor = cursor_vlm == vlm_nr;
    }

    bool is_selected = false;
    if (selecting)
    {
        is_selected = dsp_comp_hub.dsp_shared->is_volume_selected(vlm_nr);
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

    if (vlm->has_mark_state() || vlm->has_alert_state())
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
        dsp_io->write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
        dsp_io->write_text(rst_bg.c_str());
    }
    else
    if (vlm->has_warn_state())
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

    // Volume number
    if (!(vlm->has_warn_state() || vlm->has_alert_state() || vlm->has_warn_state()))
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->vlm_nr.c_str());
    }
    dsp_io->write_fmt("%5u", static_cast<unsigned int> (vlm_nr));
    dsp_io->write_text(rst_bg.c_str());

    dsp_io->write_text("    ");

    dsp_io->write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
    dsp_io->write_fmt("%7lu", static_cast<unsigned long> (vlm->get_minor_nr()));
    dsp_io->write_text("    ");

    // Disk state
    if (vlm->has_disk_alert())
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
        dsp_io->write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
    }
    else
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
        dsp_io->write_char(' ');
    }
    dsp_io->write_char(' ');
    dsp_io->write_fmt("%-20s", vlm->get_disk_state_label());

    dsp_io->write_char(' ');

    if (vlm->has_quorum_alert())
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
        dsp_io->write_text("N  ");
    }
    else
    {
        dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
        dsp_io->write_text("Y  ");
    }

    dsp_io->write_char(' ');

    if (vlm->get_disk_state() == DrbdVolume::disk_state::INCONSISTENT)
    {
        // Find the SyncTarget peer volume

        uint16_t sync_perc = 10000;
        DrbdResource::ConnectionsIterator con_iter = rsc->connections_iterator();
        while (con_iter.has_next())
        {
            DrbdConnection* const con = con_iter.next();
            DrbdVolume* const peer_vlm = con->get_volume(vlm_nr);
            if (peer_vlm != nullptr)
            {
                const DrbdVolume::repl_state state = peer_vlm->get_replication_state();
                if (state == DrbdVolume::repl_state::SYNC_TARGET)
                {
                    sync_perc = peer_vlm->get_sync_perc();
                    break;
                }
            }
        }

        if (sync_perc < 10000)
        {
            dsp_io->write_text(dsp_comp_hub.active_color_table->warn.c_str());
            dsp_io->write_fmt(
                "%2u.%02u ",
                static_cast<unsigned int> (sync_perc / 100),
                static_cast<unsigned int> (sync_perc % 100)
            );

            const uint16_t sync_bar_length = dsp_comp_hub.term_cols - 57;
            const uint16_t finished_length = static_cast<uint16_t> (
                (static_cast<uint32_t> (sync_bar_length) * sync_perc) / 10000
            );
            const uint16_t remaining_length = sync_bar_length - finished_length;

            dsp_io->write_fill_seq(dsp_comp_hub.active_character_table->sync_blk_fin, finished_length);
            dsp_io->write_fill_seq(dsp_comp_hub.active_character_table->sync_blk_rmn, remaining_length);
        }
    }

    dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
    ++current_line;
}

void MDspVolumes::write_no_volumes_line(const bool problem_mode_flag)
{
    dsp_comp_hub.dsp_io->cursor_xy(1, VLM_LIST_Y);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
    if (problem_mode_flag)
    {
        dsp_comp_hub.dsp_io->write_text("No volumes with problem status");
    }
    else
    {
        dsp_comp_hub.dsp_io->write_text("No active volumes");
    }
}

uint32_t MDspVolumes::get_lines_per_page()
{
    return dsp_comp_hub.term_rows - VLM_LIST_Y - 2;
}

bool MDspVolumes::is_problem_mode(DrbdResource* const rsc)
{
    const DisplayCommon::problem_mode_type problem_mode = dsp_comp_hub.dsp_common->get_problem_mode();
    bool problem_mode_flag = problem_mode == DisplayCommon::problem_mode_type::LOCK;
    if (problem_mode == DisplayCommon::problem_mode_type::AUTO)
    {
        if (rsc->has_mark_state())
        {
            DrbdResource::VolumesIterator vlm_iter = rsc->volumes_iterator();
            while (vlm_iter.has_next())
            {
                DrbdVolume* const vlm = vlm_iter.next();
                if (vlm->has_mark_state() || vlm->has_warn_state() || vlm->has_alert_state())
                {
                    problem_mode_flag = true;
                    break;
                }
            }
        }
    }
    return problem_mode_flag;
}

void MDspVolumes::synchronize_data()
{
    dsp_comp_hub.dsp_shared->update_monitor_vlm(cursor_vlm);
}

void MDspVolumes::notify_data_updated()
{
    if (displayed_rsc != dsp_comp_hub.dsp_shared->monitor_rsc)
    {
        reset_display();
    }
    displayed_rsc = dsp_comp_hub.dsp_shared->monitor_rsc;
    cursor_vlm = dsp_comp_hub.dsp_shared->monitor_vlm;
    if (!is_cursor_nav())
    {
        set_page_nr(1);
    }
    dsp_comp_hub.dsp_selector->refresh_display();
}

uint64_t MDspVolumes::get_update_mask() noexcept
{
    return update_event::UPDATE_FLAG_DRBD;
}
