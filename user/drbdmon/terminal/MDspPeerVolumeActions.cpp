#include <terminal/MDspPeerVolumeActions.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/DisplayConsts.h>
#include <terminal/KeyCodes.h>
#include <terminal/HelpText.h>
#include <MessageLog.h>
#include <objects/DrbdResource.h>
#include <subprocess/SubProcessQueue.h>
#include <subprocess/CmdLine.h>
#include <subprocess/DrbdCmdConsts.h>
#include <memory>

const uint8_t   MDspPeerVolumeActions::OPT_LIST_Y   = 7;

MDspPeerVolumeActions::MDspPeerVolumeActions(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub)
{
    cmd_fn_pause_sync =
        [this]() -> void
        {
            selection_action(&MDspPeerVolumeActions::action_pause_sync);
        };
    cmd_fn_resume_sync =
        [this]() -> void
        {
            selection_action(&MDspPeerVolumeActions::action_resume_sync);
        };
    cmd_fn_verify =
        [this]() -> void
        {
            selection_action(&MDspPeerVolumeActions::action_verify);
        };
    cmd_fn_invalidate_remote =
        [this]() -> void
        {
            selection_action(&MDspPeerVolumeActions::action_invalidate_remote);
        };

    uint16_t opt_line   = OPT_LIST_Y;
    uint16_t start_col  = 5;
    uint16_t end_col    = 45;

    cmd_pause_sync = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "1", 1, opt_line, start_col, end_col,
            cmd_fn_pause_sync
        )
    );
    ++opt_line;

    cmd_resume_sync = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "2", 1, opt_line, start_col, end_col,
            cmd_fn_resume_sync
        )
    );
    ++opt_line;

    cmd_verify = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "3", 1, opt_line, start_col, end_col,
            cmd_fn_verify
        )
    );
    ++opt_line;

    cmd_invalidate_remote = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "4", 1, opt_line, start_col, end_col,
            cmd_fn_invalidate_remote
        )
    );
    ++opt_line;

    add_option(*cmd_pause_sync);
    add_option(*cmd_resume_sync);
    add_option(*cmd_verify);
    add_option(*cmd_invalidate_remote);
}

MDspPeerVolumeActions::~MDspPeerVolumeActions() noexcept
{
}

void MDspPeerVolumeActions::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_PEER_VLM_ACT);

    dsp_comp_hub.dsp_io->cursor_xy(1, DisplayConsts::PAGE_NAV_Y + 1);
    dsp_comp_hub.dsp_io->write_text("Peer volume actions: ");

    if (!dsp_comp_hub.dsp_shared->monitor_rsc.empty())
    {
        DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
        if (rsc != nullptr)
        {
            dsp_comp_hub.dsp_io->write_text("Resource ");
            dsp_comp_hub.dsp_io->write_string_field(
                dsp_comp_hub.dsp_shared->monitor_rsc,
                dsp_comp_hub.term_cols - 31,
                false
            );

            if (!dsp_comp_hub.dsp_shared->monitor_con.empty())
            {
                DrbdConnection* const con = dsp_comp_hub.get_monitor_connection();
                if (con != nullptr)
                {
                    dsp_comp_hub.dsp_io->cursor_xy(22, DisplayConsts::PAGE_NAV_Y + 2);
                    dsp_comp_hub.dsp_io->write_text("Connection ");
                    dsp_comp_hub.dsp_io->write_string_field(
                        dsp_comp_hub.dsp_shared->monitor_con,
                        dsp_comp_hub.term_cols - 33,
                        false
                    );

                    dsp_comp_hub.dsp_io->cursor_xy(22, DisplayConsts::PAGE_NAV_Y + 3);
                    if (!dsp_comp_hub.dsp_shared->ovrd_peer_volume_selection &&
                        dsp_comp_hub.dsp_shared->have_peer_volumes_selection())
                    {
                        VolumesMap& selection_map = dsp_comp_hub.dsp_shared->get_selected_peer_volumes_map();
                        const size_t count = selection_map.get_size();
                        if (count > 1)
                        {
                            dsp_comp_hub.dsp_io->write_fmt("%lu selected volumes", static_cast<unsigned long> (count));
                        }
                        else
                        {
                            dsp_comp_hub.dsp_io->write_fmt("%lu selected volume", static_cast<unsigned long> (count));
                        }
                    }
                    else
                    if (dsp_comp_hub.dsp_shared->monitor_peer_vlm != DisplayConsts::VLM_NONE)
                    {
                        DrbdVolume* const vlm = con->get_volume(dsp_comp_hub.dsp_shared->monitor_peer_vlm);
                        if (vlm != nullptr)
                        {
                            dsp_comp_hub.dsp_io->write_text("Volume ");
                            dsp_comp_hub.dsp_io->write_fmt(
                                "%u",
                                static_cast<unsigned int> (dsp_comp_hub.dsp_shared->monitor_peer_vlm)
                            );
                        }
                        else
                        {
                            dsp_comp_hub.dsp_io->write_text("Volume ");
                            dsp_comp_hub.dsp_io->write_fmt(
                                "%u",
                                static_cast<unsigned int> (dsp_comp_hub.dsp_shared->monitor_peer_vlm)
                            );
                            dsp_comp_hub.dsp_io->write_text(" not active");
                        }
                    }
                    else
                    {
                        dsp_comp_hub.dsp_io->write_text("No selected volume");
                    }
                }
                else
                {
                    dsp_comp_hub.dsp_io->write_text("Connection ");
                    dsp_comp_hub.dsp_io->write_string_field(
                        dsp_comp_hub.dsp_shared->monitor_rsc,
                        dsp_comp_hub.term_cols - 44,
                        false
                    );
                    dsp_comp_hub.dsp_io->write_text(" not active");
                }
            }
            else
            {
                dsp_comp_hub.dsp_io->write_text("No selected connection");
            }
        }
        else
        {
            dsp_comp_hub.dsp_io->write_text("Resource ");
            dsp_comp_hub.dsp_io->write_string_field(
                dsp_comp_hub.dsp_shared->monitor_rsc,
                dsp_comp_hub.term_cols - 42,
                false
            );
            dsp_comp_hub.dsp_io->write_text(" not active");
        }
    }
    else
    {
        dsp_comp_hub.dsp_io->write_text("No selected resource");
    }

    const std::string& std_color = dsp_comp_hub.active_color_table->option_text;
    const std::string& caution_color = dsp_comp_hub.active_color_table->caution_text;

    display_option(" 1   ", "Pause resynchronization", *cmd_pause_sync, std_color);
    display_option(" 2   ", "Resume resynchronization", *cmd_resume_sync, std_color);
    display_option(" 3   ", "Verify data", *cmd_verify, std_color);
    display_option(" 4   ", "Invalidate peer volume", *cmd_invalidate_remote, caution_color);

    display_option_query(5, 12);
}

void MDspPeerVolumeActions::selection_action(const action_func_type action_func)
{
    try
    {
        dsp_comp_hub.dsp_common->application_working();

        DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
        DrbdConnection* const con = dsp_comp_hub.get_monitor_connection();

        if (rsc != nullptr && con != nullptr)
        {
            const std::string& rsc_name = rsc->get_name();
            const std::string& con_name = con->get_name();

            if (!dsp_comp_hub.dsp_shared->ovrd_peer_volume_selection &&
                dsp_comp_hub.dsp_shared->have_peer_volumes_selection())
            {
                VolumesMap& selection_map = dsp_comp_hub.dsp_shared->get_selected_peer_volumes_map();
                VolumesMap::KeysIterator iter(selection_map);

                while (iter.has_next())
                {
                    const uint16_t vlm_nr = *(iter.next());
                    (this->*action_func)(rsc_name, con_name, vlm_nr);
                }
                dsp_comp_hub.dsp_selector->leave_display();
            }
            else
            {
                const uint16_t vlm_nr = dsp_comp_hub.dsp_shared->monitor_peer_vlm;

                if (vlm_nr != DisplayConsts::VLM_NONE)
                {
                    (this->*action_func)(rsc_name, con_name, vlm_nr);
                    dsp_comp_hub.dsp_selector->leave_display();
                }
            }
        }
    }
    catch (SubProcessQueue::QueueCapacityException&)
    {
        dsp_comp_hub.log->add_entry(
            MessageLog::log_level::ALERT,
            "Volume actions: Cannot execute command, insufficient queue capacity"
        );
    }
    catch (SubProcess::Exception&)
    {
        dsp_comp_hub.log->add_entry(
            MessageLog::log_level::ALERT,
            "Volume actions: Command failed: Sub-process execution error"
        );
    }
    dsp_comp_hub.dsp_common->application_idle();
    reposition_text_cursor();
}

void MDspPeerVolumeActions::action_pause_sync(
    const std::string&  rsc_name,
    const std::string&  con_name,
    const uint16_t      vlm_nr
)
{
    std::string cmd_target(rsc_name);
    cmd_target.append(":");
    cmd_target.append(con_name);
    cmd_target.append("/");
    cmd_target.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_PAUSE_SYNC);
    command->add_argument(cmd_target);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Pause resynchronization, resource ");
    text.append(rsc_name);
    text.append(", connection ");
    text.append(con_name);
    text.append(", volume ");
    text.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspPeerVolumeActions::action_resume_sync(
    const std::string&  rsc_name,
    const std::string&  con_name,
    const uint16_t      vlm_nr
)
{
    std::string cmd_target(rsc_name);
    cmd_target.append(":");
    cmd_target.append(con_name);
    cmd_target.append("/");
    cmd_target.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_RESUME_SYNC);
    command->add_argument(cmd_target);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Resume resynchronization, resource ");
    text.append(rsc_name);
    text.append(", connection ");
    text.append(con_name);
    text.append(", volume ");
    text.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspPeerVolumeActions::action_verify(
    const std::string&  rsc_name,
    const std::string&  con_name,
    const uint16_t      vlm_nr
)
{
    std::string cmd_target(rsc_name);
    cmd_target.append(":");
    cmd_target.append(con_name);
    cmd_target.append("/");
    cmd_target.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_VERIFY);
    command->add_argument(cmd_target);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Verify data, resource ");
    text.append(rsc_name);
    text.append(", connection ");
    text.append(con_name);
    text.append(", volume ");
    text.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspPeerVolumeActions::action_invalidate_remote(
    const std::string&  rsc_name,
    const std::string&  con_name,
    const uint16_t      vlm_nr
)
{
    std::string cmd_target(rsc_name);
    cmd_target.append(":");
    cmd_target.append(con_name);
    cmd_target.append("/");
    cmd_target.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_INVALIDATE_REMOTE);
    command->add_argument(cmd_target);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Invalidate peer data, resource ");
    text.append(rsc_name);
    text.append(", connection ");
    text.append(con_name);
    text.append(", volume ");
    text.append(std::to_string(static_cast<unsigned int> (vlm_nr)));

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

uint64_t MDspPeerVolumeActions::get_update_mask() noexcept
{
    return update_event::UPDATE_FLAG_DRBD;
}

bool MDspPeerVolumeActions::key_pressed(const uint32_t key)
{
    bool intercepted = MDspMenuBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::PVLM_ACTIONS, dsp_comp_hub);
            intercepted = true;
        }
    }
    return intercepted;
}

