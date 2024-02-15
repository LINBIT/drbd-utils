#include <terminal/MDspVolumeDetail.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/DisplayConsts.h>
#include <terminal/KeyCodes.h>
#include <terminal/HelpText.h>
#include <objects/DrbdResource.h>
#include <string>

MDspVolumeDetail::MDspVolumeDetail(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub)
{
    cmd_fn_actions =
        [this]() -> void
        {
            opt_actions();
        };
    cmd_actions = std::unique_ptr<ClickableCommand>(
        new ClickableCommand("A", 1, 15, 1, 20, cmd_fn_actions)
    );
}

MDspVolumeDetail::~MDspVolumeDetail() noexcept
{
}

void MDspVolumeDetail::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_VLM_DETAIL);

    dsp_comp_hub.dsp_io->cursor_xy(1, DisplayConsts::PAGE_NAV_Y + 1);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Volume details");

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

        const uint16_t vlm_nr = dsp_comp_hub.dsp_shared->monitor_vlm;

        dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
        dsp_comp_hub.dsp_io->write_text("Volume:");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
        dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
        if (vlm_nr != DisplayConsts::VLM_NONE)
        {
            dsp_comp_hub.dsp_io->write_fmt("%u", static_cast<unsigned int> (vlm_nr));
        }
        else
        {
            dsp_comp_hub.dsp_io->write_text("No selected volume");
        }
        ++current_line;

        dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
        dsp_comp_hub.dsp_io->write_text("Status:");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
        dsp_comp_hub.dsp_io->cursor_xy(21, current_line);

        if (rsc != nullptr)
        {
            DrbdVolume* const vlm = rsc->get_volume(vlm_nr);
            if (vlm != nullptr)
            {
                if (vlm->has_alert_state() || vlm->has_warn_state())
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
                dsp_comp_hub.dsp_io->write_text("Minor number:");
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
                dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
                const uint32_t minor_nr = vlm->get_minor_nr();
                dsp_comp_hub.dsp_io->write_fmt("%lu", static_cast<unsigned long> (minor_nr));
                ++current_line;

                dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
                dsp_comp_hub.dsp_io->write_text("Disk state:");
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
                dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
                if (vlm->has_disk_alert())
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                }
                else
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
                }
                const char* const disk_state_label = vlm->get_disk_state_label();
                DrbdVolume::disk_state vlm_disk_state = vlm->get_disk_state();
                dsp_comp_hub.dsp_io->write_text(disk_state_label);
                if (vlm_disk_state == DrbdVolume::disk_state::DISKLESS)
                {
                    if (vlm->has_disk_alert())
                    {
                        dsp_comp_hub.dsp_io->write_text(" (Detached/Failed)");
                    }
                    else
                    {
                        dsp_comp_hub.dsp_io->write_text(" (DRBD client)");
                    }
                }
                ++current_line;

                dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
                dsp_comp_hub.dsp_io->write_text("Quorum:");
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
                dsp_comp_hub.dsp_io->cursor_xy(21, current_line);
                if (vlm->has_quorum_alert())
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
                dsp_comp_hub.dsp_io->write_text("Volume not present");
            }

            current_line += 2;

            dsp_comp_hub.dsp_io->cursor_xy(1, current_line);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->hotkey_field.c_str());
            dsp_comp_hub.dsp_io->write_text(" A ");
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->hotkey_label.c_str());
            dsp_comp_hub.dsp_io->write_text(" Actions");

            if (current_line != saved_options_line)
            {
                clear_options();
                options_configured = false;
            }

            if (!options_configured)
            {
                cmd_actions->clickable_area.row = current_line;

                add_option_clickable(*cmd_actions);

                saved_options_line = current_line;
                options_configured = true;
            }
        }
        else
        {
            dsp_comp_hub.dsp_io->write_text("Resource not active");
        }
    }
    else
    {
        dsp_comp_hub.dsp_io->write_text("No selected resource");
    }
}

uint64_t MDspVolumeDetail::get_update_mask() noexcept
{
    return update_event::UPDATE_FLAG_DRBD;
}

bool MDspVolumeDetail::key_pressed(const uint32_t key)
{
    // Skip MDspMenuBase, not using the option field
    bool intercepted = MDspBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::VLM_DETAIL, dsp_comp_hub);
            intercepted = true;
        }
        else
        if (key == static_cast<uint32_t> ('A') || key == static_cast<uint32_t> ('a'))
        {
            opt_actions();
            intercepted = true;
        }
    }
    return intercepted;
}

void MDspVolumeDetail::text_cursor_ops()
{
    // no-op; prevents MDspMenuBase from positioning the cursor for the option field, which is not used
}

void MDspVolumeDetail::display_activated()
{
    MDspBase::display_activated();
    dsp_comp_hub.dsp_shared->ovrd_volume_selection = true;
}

void MDspVolumeDetail::display_deactivated()
{
    MDspBase::display_deactivated();
    clear_options();
    options_configured = false;
}

void MDspVolumeDetail::opt_actions()
{
    DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
    if (rsc != nullptr)
    {
        const uint16_t vlm_nr = dsp_comp_hub.dsp_shared->monitor_vlm;
        if (vlm_nr != DisplayConsts::VLM_NONE)
        {
            DrbdVolume* const vlm = rsc->get_volume(vlm_nr);
            if (vlm != nullptr)
            {
                dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::VLM_ACTIONS);
            }
        }
    }
}
