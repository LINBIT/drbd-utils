#include <terminal/MDspResourceDetail.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/DisplayConsts.h>
#include <terminal/KeyCodes.h>
#include <terminal/HelpText.h>
#include <objects/DrbdResource.h>
#include <string>

MDspResourceDetail::MDspResourceDetail(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub)
{
    cmd_fn_actions =
        [this]() -> void
        {
            opt_actions();
        };
    cmd_fn_connections =
        [this]() -> void
        {
            opt_connections_list();
        };
    cmd_fn_volumes =
        [this]() -> void
        {
            opt_volumes_list();
        };
    cmd_actions = std::unique_ptr<ClickableCommand>(
        new ClickableCommand("A", 1, 15, 1, 20, cmd_fn_actions)
    );
    cmd_connections = std::unique_ptr<ClickableCommand>(
        new ClickableCommand("C", 1, 15, 21, 40, cmd_fn_connections)
    );
    cmd_volumes = std::unique_ptr<ClickableCommand>(
        new ClickableCommand("V", 1, 15, 41, 60, cmd_fn_volumes)
    );
}

MDspResourceDetail::~MDspResourceDetail() noexcept
{
}

void MDspResourceDetail::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_RSC_DETAIL);

    dsp_comp_hub.dsp_io->cursor_xy(1, DisplayConsts::PAGE_NAV_Y + 1);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Resource details");

    uint32_t current_line = DisplayConsts::PAGE_NAV_Y + 3;
    dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Resource:");
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
    dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
    if (!dsp_comp_hub.dsp_shared->monitor_rsc.empty())
    {
        DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
        const std::string& rsc_name = (rsc != nullptr ? rsc->get_name() : dsp_comp_hub.dsp_shared->monitor_rsc);
        dsp_comp_hub.dsp_io->write_string_field(rsc_name, dsp_comp_hub.term_cols - 20, false);
        ++current_line;

        dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
        dsp_comp_hub.dsp_io->write_text("Status:");

        dsp_comp_hub.dsp_io->cursor_xy(21, current_line);

        if (rsc != nullptr)
        {
            if (rsc->has_alert_state() || rsc->has_warn_state() || rsc->has_mark_state())
            {
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                dsp_comp_hub.dsp_io->write_text("Degraded");
            }
            else
            {
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
                dsp_comp_hub.dsp_io->write_text("Normal");
            }
            ++current_line;

            dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
            dsp_comp_hub.dsp_io->write_text("Role:");
            dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
            DrbdRole::resource_role rsc_role = rsc->get_role();
            if (rsc_role == DrbdRole::resource_role::PRIMARY || rsc_role == DrbdRole::resource_role::SECONDARY)
            {
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
            }
            else
            {
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
            }
            dsp_comp_hub.dsp_io->write_text(rsc->get_role_label());
            ++current_line;

            uint8_t con_count = 0;
            uint8_t con_degraded_count = 0;
            {
                DrbdResource::ConnectionsIterator con_iter = rsc->connections_iterator();
                con_count = static_cast<uint8_t> (con_iter.get_size());
                while (con_iter.has_next())
                {
                    DrbdConnection* const con = con_iter.next();
                    if (con->has_alert_state() || con->has_warn_state())
                    {
                        ++con_degraded_count;
                    }
                }
            }
            uint8_t con_normal_count = con_count - con_degraded_count;

            uint16_t vlm_count = 0;
            uint16_t vlm_degraded_count = 0;
            {
                DrbdResource::VolumesIterator vlm_iter = rsc->volumes_iterator();
                vlm_count = static_cast<uint16_t> (vlm_iter.get_size());
                while (vlm_iter.has_next())
                {
                    DrbdVolume* const vlm = vlm_iter.next();
                    if (vlm->has_alert_state() || vlm->has_warn_state())
                    {
                        ++vlm_degraded_count;
                    }
                }
            }
            uint16_t vlm_normal_count = vlm_count - vlm_degraded_count;

            // Connections summary
            dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
            dsp_comp_hub.dsp_io->write_text("Connections:");

            dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
            const char *con_label = con_count != 1 ? "connections" : "connection";
            if (con_degraded_count == 0)
            {
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->con_count.c_str());
                dsp_comp_hub.dsp_io->write_fmt(
                    "%u %s, no alerts", static_cast<unsigned int> (con_count), con_label
                );
            }
            else
            if (con_degraded_count == con_count)
            {
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                dsp_comp_hub.dsp_io->write_fmt(
                    "%u %s, ", static_cast<unsigned int> (con_count), con_label
                );
                if (con_count == 1)
                {
                    dsp_comp_hub.dsp_io->write_text("degraded");
                }
                else
                {
                    dsp_comp_hub.dsp_io->write_text("all degraded");
                }
            }
            else
            {
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->con_count.c_str());
                dsp_comp_hub.dsp_io->write_fmt(
                    "%u %s, ", static_cast<unsigned int> (con_count), con_label
                );
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                dsp_comp_hub.dsp_io->write_fmt("%u degraded", static_cast<unsigned int> (con_degraded_count));
                if (con_normal_count > 0)
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
                    dsp_comp_hub.dsp_io->write_fmt(" (%u normal)", static_cast<unsigned int> (con_normal_count));
                }
            }
            ++current_line;

            // Volumes summary
            dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
            dsp_comp_hub.dsp_io->write_text("Volumes:");

            dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
            const char *vlm_label = vlm_count != 1 ? "volumes" : "volume";
            if (vlm_degraded_count == 0)
            {
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->con_count.c_str());
                dsp_comp_hub.dsp_io->write_fmt(
                    "%u %s, no alerts", static_cast<unsigned int> (vlm_count), vlm_label
                );
            }
            else
            if (vlm_degraded_count == vlm_count)
            {
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                dsp_comp_hub.dsp_io->write_fmt(
                    "%u %s, ", static_cast<unsigned int> (vlm_count), vlm_label
                );
                if (vlm_count == 1)
                {
                    dsp_comp_hub.dsp_io->write_text("degraded");
                }
                else
                {
                    dsp_comp_hub.dsp_io->write_text("all degraded");
                }
            }
            else
            {
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->vlm_count.c_str());
                dsp_comp_hub.dsp_io->write_fmt(
                    "%u %s, ", static_cast<unsigned int> (vlm_count), vlm_label
                );
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                dsp_comp_hub.dsp_io->write_fmt("%u degraded", static_cast<unsigned int> (vlm_degraded_count));
                if (con_normal_count > 0)
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
                    dsp_comp_hub.dsp_io->write_fmt(" (%u normal)", static_cast<unsigned int> (vlm_normal_count));
                }
            }
            ++current_line;

            dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
            dsp_comp_hub.dsp_io->write_text("Quorum:");
            dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
            if (rsc->has_quorum_alert())
            {
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                dsp_comp_hub.dsp_io->write_text("No");
            }
            else
            {
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
                dsp_comp_hub.dsp_io->write_text("Yes");
            }
        }
        else
        {
            dsp_comp_hub.dsp_io->write_text("Not active");
        }

        current_line += 2;

        dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->hotkey_field.c_str());
        dsp_comp_hub.dsp_io->write_text(" A ");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->hotkey_label.c_str());
        dsp_comp_hub.dsp_io->write_text(" Actions");

        dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->hotkey_field.c_str());
        dsp_comp_hub.dsp_io->write_text(" C ");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->hotkey_label.c_str());
        dsp_comp_hub.dsp_io->write_text(" Connections");

        dsp_comp_hub.dsp_io->cursor_xy(41, current_line);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->hotkey_field.c_str());
        dsp_comp_hub.dsp_io->write_text(" V ");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->hotkey_label.c_str());
        dsp_comp_hub.dsp_io->write_text(" Volumes");

        if (current_line != saved_options_line)
        {
            clear_options();
            options_configured = false;
        }

        if (!options_configured)
        {
            cmd_actions->clickable_area.row = current_line;
            cmd_connections->clickable_area.row = current_line;
            cmd_volumes->clickable_area.row = current_line;

            add_option_clickable(*cmd_actions);
            add_option_clickable(*cmd_connections);
            add_option_clickable(*cmd_volumes);

            saved_options_line = current_line;
            options_configured = true;
        }
    }
    else
    {
        dsp_comp_hub.dsp_io->write_text("No selected resource");
    }
}

uint64_t MDspResourceDetail::get_update_mask() noexcept
{
    return update_event::UPDATE_FLAG_DRBD;
}

bool MDspResourceDetail::key_pressed(const uint32_t key)
{
    // Skip MDspMenuBase, not using the option field
    bool intercepted = MDspBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::RSC_DETAIL, dsp_comp_hub);
            intercepted = true;
        }
        else
        if (key == static_cast<uint32_t> ('A') || key == static_cast<uint32_t> ('a'))
        {
            opt_actions();
            intercepted = true;
        }
        else
        if (key == static_cast<uint32_t> ('C') || key == static_cast<uint32_t> ('c'))
        {
            opt_connections_list();
            intercepted = true;
        }
        else
        if (key == static_cast<uint32_t> ('V') || key == static_cast<uint32_t> ('v'))
        {
            opt_volumes_list();
            intercepted = true;
        }
    }
    return intercepted;
}

void MDspResourceDetail::text_cursor_ops()
{
    // no-op; prevents MDspMenuBase from positioning the cursor for the option field, which is not used
}

void MDspResourceDetail::display_activated()
{
    MDspBase::display_activated();
    dsp_comp_hub.dsp_shared->ovrd_resource_selection = true;
}

void MDspResourceDetail::display_deactivated()
{
    MDspBase::display_deactivated();
    clear_options();
    options_configured = false;
}

void MDspResourceDetail::opt_actions()
{
    if (!dsp_comp_hub.dsp_shared->monitor_rsc.empty())
    {
        dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::RSC_ACTIONS);
    }
}

void MDspResourceDetail::opt_connections_list()
{
    if (!dsp_comp_hub.dsp_shared->monitor_rsc.empty())
    {
        dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::CON_LIST);
    }
}

void MDspResourceDetail::opt_volumes_list()
{
    if (!dsp_comp_hub.dsp_shared->monitor_rsc.empty())
    {
        dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::VLM_LIST);
    }
}
