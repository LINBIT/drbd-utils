#include <terminal/MDspConnectionDetail.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/DisplayConsts.h>
#include <terminal/KeyCodes.h>
#include <terminal/HelpText.h>
#include <terminal/HelpText.h>
#include <objects/DrbdResource.h>
#include <string>

MDspConnectionDetail::MDspConnectionDetail(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub)
{
    cmd_fn_actions =
        [this]() -> void
        {
            opt_actions();
        };
    cmd_fn_peer_vlm =
        [this]() -> void
        {
            opt_peer_vlm();
        };
    cmd_actions = std::unique_ptr<ClickableCommand>(
        new ClickableCommand("A", 1, 15, 1, 20, cmd_fn_actions)
    );
    cmd_peer_vlm = std::unique_ptr<ClickableCommand>(
        new ClickableCommand("V", 1, 15, 21, 40, cmd_fn_peer_vlm)
    );
}

MDspConnectionDetail::~MDspConnectionDetail() noexcept
{
}

void MDspConnectionDetail::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_CON_DETAIL);

    dsp_comp_hub.dsp_io->cursor_xy(1, DisplayConsts::PAGE_NAV_Y + 1);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Connection details");

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
        dsp_comp_hub.dsp_io->write_text("Connection:");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
        dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
        if (!dsp_comp_hub.dsp_shared->monitor_con.empty())
        {
            DrbdConnection* const con = dsp_comp_hub.get_monitor_connection();
            const std::string& con_name = (con != nullptr ? con->get_name() : dsp_comp_hub.dsp_shared->monitor_con);
            dsp_comp_hub.dsp_io->write_string_field(con_name, dsp_comp_hub.term_cols - 20, false);
            ++current_line;

            dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
            dsp_comp_hub.dsp_io->write_text("Status:");
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
            dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
            if (con != nullptr)
            {
                if (con->has_alert_state() || con->has_warn_state())
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
                dsp_comp_hub.dsp_io->write_text("Connection state:");
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
                dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
                if (con->has_connection_alert())
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                }
                else
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
                }
                // TODO: Update experimental feature. This does not exist in any official DRBD version.
                // Begin experimental feature
                const DrbdConnection::sync_state_type sync_state = con->get_sync_state();
                if (con->get_connection_state() != DrbdConnection::state::STANDALONE ||
                    sync_state == DrbdConnection::sync_state_type::RESYNCABLE)
                {
                    const char* const con_state_label = con->get_connection_state_label();
                    dsp_comp_hub.dsp_io->write_text(con_state_label);
                }
                else
                if (sync_state == DrbdConnection::sync_state_type::SPLIT)
                {
                    dsp_comp_hub.dsp_io->write_text("SplitBrain");
                }
                else
                if (sync_state == DrbdConnection::sync_state_type::UNRELATED)
                {
                    dsp_comp_hub.dsp_io->write_text("UnrelatedData");
                }
                else
                {
                    dsp_comp_hub.dsp_io->write_text("(INVALID)");
                }
                // End experimental feature
                ++current_line;

                dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
                dsp_comp_hub.dsp_io->write_text("Peer role:");
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
                dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
                const DrbdRole::resource_role peer_role = con->get_role();
                const char* const peer_role_label = con->get_role_label();
                if (peer_role == DrbdRole::resource_role::PRIMARY || peer_role == DrbdRole::resource_role::SECONDARY)
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
                }
                else
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                }
                dsp_comp_hub.dsp_io->write_text(peer_role_label);
            }
            else
            {
                dsp_comp_hub.dsp_io->write_text("Connection not present");
            }

            current_line += 2;

            dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->hotkey_field.c_str());
            dsp_comp_hub.dsp_io->write_text(" A ");
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->hotkey_label.c_str());
            dsp_comp_hub.dsp_io->write_text(" Actions");

            dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->hotkey_field.c_str());
            dsp_comp_hub.dsp_io->write_text(" V ");
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->hotkey_label.c_str());
            dsp_comp_hub.dsp_io->write_text(" Peer volumes");

            if (current_line != saved_options_line)
            {
                clear_options();
                options_configured = false;
            }

            if (!options_configured)
            {
                cmd_actions->clickable_area.row = current_line;
                cmd_peer_vlm->clickable_area.row = current_line;

                add_option_clickable(*cmd_actions);
                add_option_clickable(*cmd_peer_vlm);

                saved_options_line = current_line;
                options_configured = true;
            }
        }
        else
        {
            dsp_comp_hub.dsp_io->write_text("No selected connection");
        }
    }
    else
    {
        dsp_comp_hub.dsp_io->write_text("No selected resource");
    }

}

uint64_t MDspConnectionDetail::get_update_mask() noexcept
{
    return update_event::UPDATE_FLAG_DRBD;
}

bool MDspConnectionDetail::key_pressed(const uint32_t key)
{
    // Skip MDspMenuBase, not using the option field
    bool intercepted = MDspBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::CON_DETAIL, dsp_comp_hub);
            intercepted = true;
        }
        else
        if (key == static_cast<uint32_t> ('A') || key == static_cast<uint32_t> ('a'))
        {
            opt_actions();
            intercepted = true;
        }
        else
        if (key == static_cast<uint32_t> ('V') || key == static_cast<uint32_t> ('v'))
        {
            opt_peer_vlm();
            intercepted = true;
        }
    }
    return intercepted;
}

void MDspConnectionDetail::text_cursor_ops()
{
    // no-op; prevents MDspMenuBase from positioning the cursor for the option field, which is not used
}

void MDspConnectionDetail::display_activated()
{
    MDspBase::display_activated();
    dsp_comp_hub.dsp_shared->ovrd_connection_selection = true;
}

void MDspConnectionDetail::display_deactivated()
{
    MDspBase::display_deactivated();
    clear_options();
    options_configured = false;
}

void MDspConnectionDetail::opt_actions()
{
    DrbdConnection* const con = dsp_comp_hub.get_monitor_connection();
    if (con != nullptr)
    {
        dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::CON_ACTIONS);
    }
}

void MDspConnectionDetail::opt_peer_vlm()
{
    DrbdConnection* const con = dsp_comp_hub.get_monitor_connection();
    if (con != nullptr)
    {
        dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::PEER_VLM_LIST);
    }
}
