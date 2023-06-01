#include <DrbdMonConsts.h>
#include <terminal/ComponentsHub.h>
#include <terminal/DisplayCommonImpl.h>
#include <terminal/ColorTable.h>
#include <terminal/DisplayIo.h>
#include <terminal/AnsiControl.h>
#include <terminal/DisplayConsts.h>
#include <terminal/KeyCodes.h>
#include <terminal/InputField.h>
#include <objects/StateFlags.h>
#include <objects/DrbdResource.h>
#include <string_transformations.h>
#include <cppdsaext/src/integerparse.h>
#include <StringTokenizer.h>
#include <bounds.h>
#include <iostream>

DisplayCommonImpl::DisplayCommonImpl(const ComponentsHub& comp_hub):
    dsp_comp_hub(comp_hub),
    dsp_io(*(comp_hub.dsp_io))
{
    program_version.reserve(80);
    program_version = DrbdMonConsts::PROGRAM_NAME;
    program_version.append(" ");
    program_version.append(DrbdMonConsts::PROJECT_VERSION);
    program_version.shrink_to_fit();
}

DisplayCommonImpl::~DisplayCommonImpl() noexcept
{
}

void DisplayCommonImpl::display_header() const
{
    const ColorTable& clr = *(dsp_comp_hub.active_color_table);

    dsp_io.write_fmt(
        "%s%s%s", clr.title_bar.c_str(), AnsiControl::ANSI_CLEAR_LINE.c_str(),
        program_version.c_str()
    );
    if (dsp_comp_hub.node_name != nullptr)
    {
        dsp_io.write_text(" | Node ");
        dsp_io.write_string_field(
            *(dsp_comp_hub.node_name),
            dsp_comp_hub.term_cols - program_version.length() - 8, false
        );
    }

    if (dsp_comp_hub.dsp_selector->can_leave_display())
    {
        // Close symbol
        dsp_io.write_text(clr.sym_close.c_str());
        dsp_io.cursor_xy(dsp_comp_hub.term_cols - 2, 1);
        dsp_io.write_text("[");
        dsp_io.write_text(dsp_comp_hub.active_character_table->sym_close.c_str());
        dsp_io.write_text("]");
    }

    dsp_io.write_text(clr.rst.c_str());
}

void DisplayCommonImpl::display_page_id(const std::string& page_id) const
{
    dsp_io.cursor_xy(1, DisplayConsts::PAGE_NAV_Y);
    dsp_io.write_text(page_id.c_str());
    dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
}

void DisplayCommonImpl::display_command_line() const
{
    dsp_io.cursor_xy(1, dsp_comp_hub.term_rows - DisplayConsts::CMD_LINE_Y);
    dsp_io.write_text(dsp_comp_hub.active_color_table->cmd_label.c_str());
    dsp_io.write_text("Command ");
    dsp_io.write_text(dsp_comp_hub.active_character_table->sym_cmd_arrow.c_str());
    dsp_io.write_char(' ');
    dsp_comp_hub.command_line->display();
}

void DisplayCommonImpl::display_std_hotkeys() const
{
    const ColorTable& clr = *(dsp_comp_hub.active_color_table);

    DisplayId::display_page active_page = dsp_comp_hub.dsp_selector->get_active_page();

    dsp_io.cursor_xy(1, dsp_comp_hub.term_rows);

    if (active_page != DisplayId::display_page::PGM_INFO && active_page != DisplayId::display_page::HELP_IDX)
    {
        if (active_page == DisplayId::display_page::HELP_VIEWER)
        {
            write_hotkey(clr, " F1 ", " Help index ");
        }
        else
        {
            write_hotkey(clr, " F1 ", " Help ");
        }
        hotkey_spacer();
    }
    if (active_page == DisplayId::display_page::MAIN_MENU)
    {
        write_hotkey(clr, " F3 ", " Exit ");
    }
    else
    {
        write_hotkey(clr, " F2 ", " Main menu ");
    }
    hotkey_spacer();
    write_hotkey(clr, " / ", " Command ");
    hotkey_spacer();
    write_hotkey(clr, " F6 ", " Go to page ");
    if (dsp_comp_hub.dsp_selector->can_leave_display())
    {
        hotkey_spacer();
        write_hotkey(clr, " F10 ", " Close ");
    }

    dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
}

void DisplayCommonImpl::display_page_or_cursor_nav(const bool is_cursor_tracking) const
{
    if (is_cursor_tracking)
    {
        dsp_io.cursor_xy(dsp_comp_hub.term_cols - DisplayConsts::CURSOR_LABEL_X, DisplayConsts::PAGE_NAV_Y);
        dsp_io.write_text(dsp_comp_hub.active_character_table->sym_info.c_str());
        dsp_io.write_text("Cursor");
        dsp_io.cursor_xy(dsp_comp_hub.term_cols - DisplayConsts::PAGE_NAV_X, DisplayConsts::PAGE_NAV_Y);
    }
    else
    {
        dsp_io.cursor_xy(dsp_comp_hub.term_cols - DisplayConsts::PAGE_NAV_X - 1, DisplayConsts::PAGE_NAV_Y);
        dsp_io.write_text(dsp_comp_hub.active_character_table->sym_info.c_str());
    }
}

void DisplayCommonImpl::display_page_numbers(
    const bool      is_focused,
    const uint32_t  page_nr,
    const uint32_t  page_count,
    const uint32_t  line_offset,
    const uint32_t  input_page_nr
) const
{
    dsp_comp_hub.dsp_io->cursor_xy(dsp_comp_hub.term_cols - DisplayConsts::PAGE_NAV_X, DisplayConsts::PAGE_NAV_Y);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
    dsp_comp_hub.dsp_io->write_text("Page ");
    const uint32_t displayed_page_nr = (page_nr <= page_count ? page_nr : page_count);
    if (is_focused)
    {
        dsp_io.write_text(dsp_comp_hub.active_color_table->bg_text_field.c_str());
    }
    if (!is_focused || input_page_nr >= 1)
    {
        dsp_comp_hub.dsp_io->write_fmt(
            "%5lu",
            (is_focused ? static_cast<unsigned long> (input_page_nr) : static_cast<unsigned long> (displayed_page_nr))
        );
    }
    else
    {
        dsp_comp_hub.dsp_io->write_fill_char(' ', 5);
    }
    if (is_focused)
    {
        dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
    }

    if (displayed_page_nr == page_nr)
    {
        dsp_io.write_char('/');
    }
    else
    {
        dsp_io.write_text(dsp_comp_hub.active_character_table->sym_last_page.c_str());
    }
    dsp_io.write_fmt("%5lu", static_cast<unsigned long> (page_count));
    if (line_offset != 0)
    {
        dsp_io.write_fmt(" +%3lu", static_cast<unsigned long> (line_offset));
    }
}

void DisplayCommonImpl::display_resource_header(uint32_t& current_line) const
{
    dsp_io.cursor_xy(1, current_line);
    dsp_io.write_text(dsp_comp_hub.active_color_table->list_header.c_str());
    dsp_io.write_text(dsp_comp_hub.ansi_ctl->ANSI_CLEAR_LINE.c_str());

    dsp_io.cursor_xy(1, current_line);
    dsp_io.write_text("   R Resource");
    dsp_io.cursor_xy(dsp_comp_hub.term_cols - 26, current_line);
    dsp_io.write_fmt("  %s", "Volumes");

    dsp_io.cursor_xy(dsp_comp_hub.term_cols - 12, current_line);
    dsp_io.write_fmt("  %s", "Conn");

    dsp_io.cursor_xy(dsp_comp_hub.term_cols - 4, current_line);
    dsp_io.write_text("Qrm");

    dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());

    ++current_line;
}

void DisplayCommonImpl::display_connection_header(uint32_t& current_line) const
{
    dsp_io.cursor_xy(1, current_line);
    dsp_io.write_text(dsp_comp_hub.active_color_table->list_header.c_str());
    dsp_io.write_text(dsp_comp_hub.ansi_ctl->ANSI_CLEAR_LINE.c_str());

    dsp_io.cursor_xy(1, current_line);
    dsp_io.write_text("   R Connection");

    dsp_io.cursor_xy(dsp_comp_hub.term_cols - 29, current_line);
    dsp_io.write_text("ConnState");

    dsp_io.cursor_xy(dsp_comp_hub.term_cols - 13, current_line);
    dsp_io.write_text("  Volumes");

    dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());

    ++current_line;
}

void DisplayCommonImpl::display_resource_line(uint32_t& current_line) const
{
    dsp_io.cursor_xy(1, current_line);
    DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
    if (rsc != nullptr)
    {
        dsp_io.cursor_xy(1, current_line);
        const std::string& rsc_name = rsc->get_name();

        const bool has_mark_state   = rsc->has_mark_state();
        const bool has_alert_state  = rsc->has_alert_state();
        const bool has_warn_state   = rsc->has_warn_state();

        dsp_io.write_char(' ');

        // Alert symbol
        if (has_mark_state || has_alert_state)
        {
            dsp_io.write_text(dsp_comp_hub.active_color_table->alert.c_str());
            dsp_io.write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
            dsp_io.write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
            dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
        }
        else
        if (has_warn_state)
        {
            dsp_io.write_text(dsp_comp_hub.active_color_table->warn.c_str());
            dsp_io.write_text(dsp_comp_hub.active_character_table->sym_warn.c_str());
            dsp_io.write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
            dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
        }
        else
        {
            dsp_io.write_char(' ');
        }

        dsp_io.write_char(' ');

        // Resource role symbol
        {
            const DrbdResource::resource_role rsc_role = rsc->get_role();
            if (rsc_role == DrbdResource::resource_role::PRIMARY)
            {
                dsp_io.write_text(dsp_comp_hub.active_character_table->sym_pri.c_str());
            }
            else
            if (rsc_role == DrbdResource::resource_role::SECONDARY)
            {
                dsp_io.write_text(dsp_comp_hub.active_character_table->sym_sec.c_str());
            }
            else
            {
                dsp_io.write_text(dsp_comp_hub.active_color_table->warn.c_str());
                dsp_io.write_text(dsp_comp_hub.active_character_table->sym_unknown.c_str());
                dsp_io.write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
                dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
            }
        }

        dsp_io.write_char(' ');

        if (has_mark_state || has_alert_state)
        {
            dsp_io.write_text(dsp_comp_hub.active_color_table->alert.c_str());
        }
        else
        if (has_warn_state)
        {
            dsp_io.write_text(dsp_comp_hub.active_color_table->warn.c_str());
        }
        else
        {
            dsp_io.write_text(dsp_comp_hub.active_color_table->rsc_name.c_str());
        }

        // Resource name field (starts at column 6)
        dsp_io.write_string_field(rsc_name, dsp_comp_hub.term_cols - 33, true);

        dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
        dsp_io.write_char(' ');

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
                dsp_io.write_char(' ');
            }
            else
            if (have_vlm_alerts)
            {
                dsp_io.write_text(dsp_comp_hub.active_color_table->alert.c_str());
                dsp_io.write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
                dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
            }
            else
            {
                dsp_io.write_text(dsp_comp_hub.active_color_table->warn.c_str());
                dsp_io.write_text(dsp_comp_hub.active_character_table->sym_warn.c_str());
                dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
            }

            dsp_io.write_text(dsp_comp_hub.active_color_table->vlm_count.c_str());
            dsp_io.write_char(' ');

            dsp_io.write_fmt("%5u/%5u", static_cast<unsigned int> (norm_count), static_cast<unsigned int> (vlm_count));
            dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
        }

        dsp_io.write_char(' ');

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
                dsp_io.write_text(dsp_comp_hub.active_color_table->alert.c_str());
                dsp_io.write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
                dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
            }
            else
            if (have_con_warnings)
            {
                dsp_io.write_text(dsp_comp_hub.active_color_table->warn.c_str());
                dsp_io.write_text(dsp_comp_hub.active_character_table->sym_warn.c_str());
                dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
            }
            else
            {
                dsp_io.write_char(' ');
            }

            dsp_io.write_text(dsp_comp_hub.active_color_table->con_count.c_str());
            dsp_io.write_char(' ');

            dsp_io.write_fmt("%2u/%2u", static_cast<unsigned int> (norm_count), static_cast<unsigned int> (con_count));
            dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
        }

        dsp_io.write_char(' ');

        // Quorum status field
        if (rsc->has_quorum_alert())
        {
            dsp_io.write_text(dsp_comp_hub.active_color_table->alert.c_str());
            dsp_io.write_char('N');
            dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
        }
        else
        {
            dsp_io.write_char('Y');
        }
        dsp_io.write_text("  ");

        dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
    }
    else
    {
        if (dsp_comp_hub.dsp_shared->monitor_rsc.empty())
        {
            dsp_io.write_text(dsp_comp_hub.active_color_table->warn.c_str());
            dsp_io.write_text("No selected resource");
            dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
        }
        else
        {
            dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
            dsp_io.write_text("UNKN ");
            dsp_io.write_string_field(dsp_comp_hub.dsp_shared->monitor_rsc, dsp_comp_hub.term_cols - 33, false);
        }
    }

    ++current_line;
}

void DisplayCommonImpl::display_connection_line(uint32_t& current_line) const
{
    dsp_io.cursor_xy(1, current_line);
    DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
    if (rsc != nullptr)
    {
        if (!dsp_comp_hub.dsp_shared->monitor_con.empty())
        {
            DrbdConnection* const con = rsc->get_connection(dsp_comp_hub.dsp_shared->monitor_con);
            if (con != nullptr)
            {
                const bool has_mark_state   = con->has_mark_state();
                const bool has_alert_state  = con->has_alert_state();
                const bool has_warn_state   = con->has_warn_state();

                const std::string& con_name = con->get_name();

                dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
                dsp_io.write_text(dsp_comp_hub.ansi_ctl->ANSI_CLEAR_LINE.c_str());

                dsp_io.write_char(' ');

                if (has_alert_state)
                {
                    dsp_io.write_text(dsp_comp_hub.active_color_table->alert.c_str());
                    dsp_io.write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
                    dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
                }
                else
                if (has_mark_state || has_warn_state)
                {
                    dsp_io.write_text(dsp_comp_hub.active_color_table->warn.c_str());
                    dsp_io.write_text(dsp_comp_hub.active_character_table->sym_warn.c_str());
                    dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
                }
                else
                {
                    dsp_io.write_char(' ');
                }

                dsp_io.write_char(' ');

                // Connection/peer role
                DrbdConnection::resource_role peer_role = con->get_role();
                if (peer_role == DrbdConnection::resource_role::PRIMARY)
                {
                    dsp_io.write_text(dsp_comp_hub.active_character_table->sym_pri.c_str());
                }
                else
                if (peer_role == DrbdConnection::resource_role::SECONDARY)
                {
                    dsp_io.write_text(dsp_comp_hub.active_character_table->sym_sec.c_str());
                }
                else
                {
                    dsp_io.write_text(dsp_comp_hub.active_color_table->warn.c_str());
                    dsp_io.write_text(dsp_comp_hub.active_character_table->sym_unknown.c_str());
                    dsp_io.write_text(dsp_comp_hub.active_color_table->rst_fg.c_str());
                }

                dsp_io.write_char(' ');

                // Connection name
                if (!(has_mark_state || has_alert_state || has_warn_state))
                {
                    dsp_io.write_text(dsp_comp_hub.active_color_table->con_name.c_str());
                }
                else
                {
                    dsp_io.write_text(dsp_comp_hub.active_color_table->alert.c_str());
                }
                dsp_io.write_string_field(con_name, dsp_comp_hub.term_cols - 34, false);

                dsp_io.cursor_xy(dsp_comp_hub.term_cols - 29, current_line);
                if (con->has_connection_alert())
                {
                    dsp_io.write_text(dsp_comp_hub.active_color_table->alert.c_str());
                }
                else
                if (has_mark_state || has_warn_state)
                {
                    dsp_io.write_text(dsp_comp_hub.active_color_table->warn.c_str());
                }
                else
                {
                    dsp_io.write_text(dsp_comp_hub.active_color_table->norm.c_str());
                }
                dsp_io.write_text(con->get_connection_state_label());

                dsp_io.cursor_xy(dsp_comp_hub.term_cols - 13, current_line);
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
                        dsp_io.write_char(' ');
                    }
                    else
                    if (have_vlm_alerts)
                    {
                        dsp_io.write_text(dsp_comp_hub.active_color_table->alert.c_str());
                        dsp_io.write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
                        dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
                    }
                    else
                    {
                        dsp_io.write_text(dsp_comp_hub.active_color_table->warn.c_str());
                        dsp_io.write_text(dsp_comp_hub.active_character_table->sym_warn.c_str());
                        dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
                    }
                    dsp_io.write_fmt(
                        " %5u/%5u", static_cast<unsigned int> (norm_count), static_cast<unsigned int> (vlm_count)
                    );
                    dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
                }

                dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
                ++current_line;
            }
            else
            {
                dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
                dsp_io.write_text("UNKN ");
                dsp_io.write_string_field(dsp_comp_hub.dsp_shared->monitor_con, dsp_comp_hub.term_cols - 34, false);
            }
        }
        else
        {
            dsp_io.write_text(dsp_comp_hub.active_color_table->warn.c_str());
            dsp_io.write_text("No selected connection");
            dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
        }
    }
}

void DisplayCommonImpl::page_navigation_cursor() const
{
    // Page navigation cursor
    dsp_io.cursor_xy(dsp_comp_hub.term_cols - DisplayConsts::PAGE_NAV_CURSOR_X, DisplayConsts::PAGE_NAV_Y);
    dsp_io.write_text(dsp_comp_hub.ansi_ctl->ANSI_CURSOR_ON.c_str());
}

void DisplayCommonImpl::display_problem_mode_label(const bool using_problem_mode) const
{
    dsp_io.cursor_xy(dsp_comp_hub.term_cols - DisplayConsts::PRB_MODE_X, DisplayConsts::PAGE_NAV_Y);
    if (problem_mode == problem_mode_type::AUTO)
    {
        if (using_problem_mode)
        {
            dsp_io.write_text(dsp_comp_hub.active_color_table->alert_label.c_str());
        }
        else
        {
            dsp_io.write_text(dsp_comp_hub.active_color_table->status_label.c_str());
        }
        dsp_io.write_text("Problems/Auto");
        dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
    }
    else
    if (problem_mode == problem_mode_type::LOCK)
    {
        dsp_io.write_text(dsp_comp_hub.active_color_table->alert_label.c_str());
        dsp_io.write_text("Problems/Lock");
        dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
    }
    else
    {
        dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
        dsp_io.write_fill_char(' ', 13);
    }
}

void DisplayCommonImpl::display_selection_mode_label(const bool is_enabled) const
{
    dsp_io.cursor_xy(dsp_comp_hub.term_cols - DisplayConsts::SLCT_MODE_X, DisplayConsts::PAGE_NAV_Y);
    if (is_enabled)
    {
        dsp_io.write_text(dsp_comp_hub.active_color_table->status_label.c_str());
        dsp_io.write_text("Selection");
        dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
    }
    else
    {
        dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
        dsp_io.write_fill_char(' ', 9);
    }
}

uint32_t DisplayCommonImpl::calculate_page_count(const uint32_t lines, const uint32_t lines_per_page) const
{
    const uint32_t result_page_count =
        std::max(
            (lines / lines_per_page) + (lines % lines_per_page != 0 ? 1 : 0),
            static_cast<uint32_t> (1)
        );
    return result_page_count;
}

DisplayCommon::problem_mode_type DisplayCommonImpl::get_problem_mode() const noexcept
{
    return problem_mode;
}

void DisplayCommonImpl::toggle_problem_mode() noexcept
{
    switch (problem_mode)
    {
        case problem_mode_type::OFF:
            problem_mode = problem_mode_type::AUTO;
            break;
        case problem_mode_type::AUTO:
            problem_mode = problem_mode_type::LOCK;
            break;
        case problem_mode_type::LOCK:
            problem_mode = problem_mode_type::OFF;
            break;
        default:
            problem_mode = problem_mode_type::OFF;
            break;
    }
}

DisplayCommon::command_state_type DisplayCommonImpl::command_line_key_pressed(const uint32_t key) const
{
    DisplayCommon::command_state_type state = DisplayCommon::command_state_type::INPUT;
    if (key == KeyCodes::FUNC_12)
    {
        state = DisplayCommon::command_state_type::CANCEL;
        dsp_comp_hub.command_line->clear_text();
    }
    else
    if (key == KeyCodes::ENTER)
    {
        state = DisplayCommon::command_state_type::CMD_LOCAL;
        if (global_command())
        {
            state = DisplayCommon::command_state_type::CMD_GLOBAL;
        }
    }
    else
    {
        dsp_comp_hub.command_line->key_pressed(key);
        if (dsp_comp_hub.command_line->is_empty())
        {
            state = DisplayCommon::command_state_type::CANCEL;
            dsp_comp_hub.command_line->clear_text();
        }
        else
        if (key == static_cast<uint32_t> (' '))
        {
            const std::string& command = dsp_comp_hub.command_line->get_text();
            const size_t command_length = command.length();
            const char* const command_chars = command.c_str();
            // Need at least the slash, one letter and the space
            if (command_length >= 3)
            {
                if (command_chars[0] == '/')
                {
                    const uint16_t cursor_pos = dsp_comp_hub.command_line->get_cursor_position();
                    const size_t space_idx = command.find(" ");
                    if (space_idx != std::string::npos && space_idx >= 2 && cursor_pos >= 1 &&
                        static_cast<size_t> (cursor_pos - 1) == space_idx)
                    {
                        const bool cmd_scope_drbd = command_chars[1] == '/';

                        const std::string prefix = command.substr(
                            (cmd_scope_drbd ? 2 : 1), space_idx - (cmd_scope_drbd ? 2 : 1));
                        // Need at least the first character for command completion
                        if (prefix.length() >= 1)
                        {
                            size_t arguments_idx = space_idx + 1;
                            {

                                while (arguments_idx < command_length && command_chars[arguments_idx] == ' ')
                                {
                                    ++arguments_idx;
                                }
                            }
                            const std::string arguments = arguments_idx < command_length ?
                                    command.substr(arguments_idx, command_length - arguments_idx) : "";

                            command_completion(command, arguments, prefix, cmd_scope_drbd);
                        }
                    }
                }
            }
        }
    }
    return state;
}

void DisplayCommonImpl::command_completion(
    const std::string& command,
    const std::string& arguments,
    const std::string& prefix,
    const bool cmd_scope_drbd
) const
{
    bool unique = true;
    std::string completion;
    if (prefix == "d" || prefix == "D")
    {
        completion = "DISPLAY";
    }
    else
    {
        if (!cmd_scope_drbd)
        {
            unique = dsp_comp_hub.global_cmd_exec->complete_command(prefix, completion);
        }
        if (cmd_scope_drbd || completion.empty())
        {
            unique = dsp_comp_hub.drbd_cmd_exec->complete_command(prefix, completion);
        }
    }

    if (!completion.empty())
    {
        std::string updated_command = cmd_scope_drbd ? "//" : "/";
        updated_command.append(completion);
        if (!arguments.empty())
        {
            updated_command.append(" ");
            updated_command.append(arguments);
        }
        else
        if (unique)
        {
            updated_command.append(" ");
        }
        dsp_comp_hub.command_line->set_text(updated_command);
        dsp_comp_hub.command_line->set_cursor_position(
            completion.length() + (cmd_scope_drbd ? 2 : 1) + (unique ? 1 : 0)
        );
        dsp_comp_hub.command_line->display();
        dsp_comp_hub.command_line->cursor();
    }
}

void DisplayCommonImpl::activate_command_line() const
{
    const std::string& command = dsp_comp_hub.command_line->get_text();
    if (command.length() == 0)
    {
        dsp_comp_hub.command_line->set_text("/");
    }
}

bool DisplayCommonImpl::global_command() const
{
    bool processed = false;
    const std::string& command = dsp_comp_hub.command_line->get_text();
    // All commands start with '/'
    if (command.length() >= 2)
    {
        StringTokenizer tokenizer(command, DisplayConsts::CMD_TOKEN_DELIMITER);
        if (tokenizer.has_next())
        {
            std::string keyword = tokenizer.next();
            if (keyword.length() >= 2)
            {
                keyword.erase(0, 1);
                std::string upper_keyword = string_transformations::uppercase_copy_of(keyword);
                if (upper_keyword.find("/") == 0)
                {
                    upper_keyword.erase(0, 1);
                    processed = dsp_comp_hub.drbd_cmd_exec->execute_command(upper_keyword, tokenizer);
                }
                else
                {
                    processed = dsp_comp_hub.global_cmd_exec->execute_command(upper_keyword, tokenizer);
                    if (!processed)
                    {
                        processed = dsp_comp_hub.drbd_cmd_exec->execute_command(upper_keyword, tokenizer);
                    }
                }
            }
        }
    }
    if (processed)
    {
        dsp_comp_hub.command_line->clear_text();
    }
    return processed;
}

void DisplayCommonImpl::application_idle() const
{
    dsp_io.cursor_xy(dsp_comp_hub.term_cols - DisplayConsts::HOURGLASS_X, dsp_comp_hub.term_rows);
    dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
    dsp_io.write_fill_char(' ', DisplayConsts::HOURGLASS_X);
}

void DisplayCommonImpl::application_working() const
{
    dsp_io.write_text(dsp_comp_hub.ansi_ctl->ANSI_CURSOR_OFF.c_str());
    dsp_io.cursor_xy(dsp_comp_hub.term_cols - DisplayConsts::HOURGLASS_X, dsp_comp_hub.term_rows);
    dsp_io.write_text(dsp_comp_hub.active_color_table->rst.c_str());
    dsp_io.write_text(dsp_comp_hub.active_character_table->sym_working.c_str());
    dsp_io.write_text(" Working");
}

void DisplayCommonImpl::write_hotkey(const ColorTable& clr, const char* const key, const char* const label) const
{
    dsp_io.write_text(clr.hotkey_field.c_str());
    dsp_io.write_text(key);
    dsp_io.write_text(clr.hotkey_label.c_str());
    dsp_io.write_text(label);
}

void DisplayCommonImpl::hotkey_spacer() const
{
    dsp_io.write_text("  ");
}
