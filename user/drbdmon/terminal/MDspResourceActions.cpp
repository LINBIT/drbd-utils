#include <terminal/MDspResourceActions.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/DisplayConsts.h>
#include <terminal/KeyCodes.h>
#include <terminal/HelpText.h>
#include <objects/DrbdResource.h>
#include <subprocess/SubProcessQueue.h>
#include <subprocess/CmdLine.h>
#include <subprocess/DrbdCmdConsts.h>
#include <string>

const uint8_t   MDspResourceActions::OPT_LIST_Y = 5;

MDspResourceActions::MDspResourceActions(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub)
{
    cmd_fn_start =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_start);
        };
    cmd_fn_stop =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_stop);
        };
    cmd_fn_adjust =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_adjust);
        };
    cmd_fn_primary =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_primary);
        };
    cmd_fn_secondary =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_secondary);
        };
    cmd_fn_connect =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_connect);
        };
    cmd_fn_disconnect =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_disconnect);
        };
    cmd_fn_verify =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_verify);
        };
    cmd_fn_pause_sync =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_pause_sync);
        };
    cmd_fn_resume_sync =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_resume_sync);
        };
    cmd_fn_connect_discard =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_connect_discard);
        };
    cmd_fn_force_primary =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_force_primary);
        };
    cmd_fn_force_secondary =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_force_secondary);
        };
    cmd_fn_invalidate =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_invalidate);
        };
    cmd_fn_adjust_skip_disk =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_adjust_skip_disk);
        };
    cmd_fn_adjust_skip_net =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_adjust_skip_net);
        };
    cmd_fn_adjust_skip_disk_net =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_adjust_skip_disk_net);
        };

    // Left column

    ClickableCommand::Builder bld;
    bld.auto_nr = 1;
    bld.coords.page = 1;
    bld.coords.row = OPT_LIST_Y;
    bld.coords.start_col = 5;
    bld.coords.end_col = 45;

    cmd_start = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_start)
    );
    add_option(*cmd_start);
    cmd_stop = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_stop)
    );
    add_option(*cmd_stop);
    cmd_adjust = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_adjust)
    );
    add_option(*cmd_adjust);
    cmd_primary = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_primary)
    );
    add_option(*cmd_primary);
    cmd_secondary = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_secondary)
    );
    add_option(*cmd_secondary);
    cmd_connect = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_connect)
    );
    add_option(*cmd_connect);
    cmd_disconnect = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_disconnect)
    );
    add_option(*cmd_disconnect);
    cmd_verify = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_verify)
    );
    add_option(*cmd_verify);
    cmd_pause_sync = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_pause_sync)
    );
    add_option(*cmd_pause_sync);
    cmd_resume_sync = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_resume_sync)
    );
    add_option(*cmd_resume_sync);

    // Right column

    bld.coords.row = OPT_LIST_Y;
    bld.coords.start_col = 50;
    bld.coords.end_col = 95;

    cmd_force_primary = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_force_primary)
    );
    add_option(*cmd_force_primary);
    cmd_force_secondary = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_force_secondary)
    );
    add_option(*cmd_force_secondary);
    cmd_connect_discard = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_connect_discard)
    );
    add_option(*cmd_connect_discard);
    cmd_invalidate = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_invalidate)
    );
    add_option(*cmd_invalidate);

    ++bld.coords.row;

    cmd_adjust_skip_disk = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_adjust_skip_disk)
    );
    add_option(*cmd_adjust_skip_disk);
    cmd_adjust_skip_net = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_adjust_skip_net)
    );
    add_option(*cmd_adjust_skip_net);
    cmd_adjust_skip_disk_net = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_adjust_skip_disk_net)
    );
    add_option(*cmd_adjust_skip_disk_net);
}

MDspResourceActions::~MDspResourceActions() noexcept
{
}

void MDspResourceActions::display_content()
{
    if (dsp_comp_hub.enable_drbd_actions)
    {
        show_actions();
    }
    else
    {
        dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_CON_ACT);

        dsp_comp_hub.dsp_io->cursor_xy(1, DisplayConsts::PAGE_NAV_Y + 3);
        dsp_comp_hub.dsp_io->write_text("This page is currently disabled");
    }
}

void MDspResourceActions::show_actions()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_RSC_ACT);

    dsp_comp_hub.dsp_io->cursor_xy(1, DisplayConsts::PAGE_NAV_Y + 1);
    dsp_comp_hub.dsp_io->write_text("Resource actions: ");
    if (!dsp_comp_hub.dsp_shared->ovrd_resource_selection && dsp_comp_hub.dsp_shared->have_resources_selection())
    {
        ResourceSelectionMap& selection_map = dsp_comp_hub.dsp_shared->get_selected_resources_map();
        const size_t count = selection_map.get_size();
        if (count > 1)
        {
            dsp_comp_hub.dsp_io->write_fmt("%lu selected resources", static_cast<unsigned long> (count));
        }
        else
        {
            dsp_comp_hub.dsp_io->write_fmt("%lu selected resource", static_cast<unsigned long> (count));
        }
    }
    else
    if (!dsp_comp_hub.dsp_shared->monitor_rsc.empty())
    {
        dsp_comp_hub.dsp_io->write_text("Resource ");
        dsp_comp_hub.dsp_io->write_string_field(
            dsp_comp_hub.dsp_shared->monitor_rsc,
            dsp_comp_hub.term_cols - 30,
            false
        );
    }
    else
    {
        dsp_comp_hub.dsp_io->write_text("No selected resource");
    }

    const std::string& std_color = dsp_comp_hub.active_color_table->option_text;
    const std::string& caution_color = dsp_comp_hub.active_color_table->caution_text;

    display_option(5, "Start resource", *cmd_start, std_color);
    display_option(5, "Stop resource", *cmd_stop, std_color);
    display_option(5, "Adjust resource", *cmd_adjust, std_color);
    display_option(5, "Make primary", *cmd_primary, std_color);
    display_option(5, "Make secondary", *cmd_secondary, std_color);
    display_option(5, "Connect", *cmd_connect, std_color);
    display_option(5, "Disconnect", *cmd_disconnect, std_color);
    display_option(5, "Run verification", *cmd_verify, std_color);
    display_option(5, "Pause resynchronization", *cmd_pause_sync, std_color);
    display_option(5, "Resume resynchronization", *cmd_resume_sync, std_color);
    display_option(5, "Force make primary", *cmd_force_primary, caution_color);
    display_option(5, "Force make secondary", *cmd_force_secondary, caution_color);
    display_option(5, "Discard & resolve split-brain", *cmd_connect_discard, caution_color);
    display_option(5, "Invalidate & resynchronize", *cmd_invalidate, caution_color);
    display_option(5, "Adjust resource, skip disk actions", *cmd_adjust_skip_disk, std_color);
    display_option(5, "Adjust resource, skip network actions", *cmd_adjust_skip_net, std_color);
    display_option(5, "Adjust resource, skip disk & network actions", *cmd_adjust_skip_disk_net, std_color);

    display_option_query(5, 16);
}

void MDspResourceActions::selection_action(const action_func_type action_func)
{
    try
    {
        dsp_comp_hub.dsp_common->application_working();
        if (!dsp_comp_hub.dsp_shared->ovrd_resource_selection && dsp_comp_hub.dsp_shared->have_resources_selection())
        {
            ResourceSelectionMap& selection_map = dsp_comp_hub.dsp_shared->get_selected_resources_map();
            ResourceSelectionMap::KeysIterator iter(selection_map);
            while (iter.has_next())
            {
                const std::string* const rsc_name_ptr = iter.next();
                (this->*action_func)(*rsc_name_ptr);
            }
            dsp_comp_hub.dsp_selector->leave_display();
        }
        else
        if (!dsp_comp_hub.dsp_shared->monitor_rsc.empty())
        {
            (this->*action_func)(dsp_comp_hub.dsp_shared->monitor_rsc);
            dsp_comp_hub.dsp_selector->leave_display();
        }
        else
        {
            // no-op
            // TODO: Maybe switch to an error page, because there is no selection
        }
    }
    catch (SubProcessQueue::QueueCapacityException&)
    {
        dsp_comp_hub.log->add_entry(
            MessageLog::log_level::ALERT,
            "Resource actions: Cannot execute command, insufficient queue capacity"
        );
    }
    catch (SubProcess::Exception&)
    {
        dsp_comp_hub.log->add_entry(
            MessageLog::log_level::ALERT,
            "Resource actions: Command failed: Sub-process execution error"
        );
    }
    dsp_comp_hub.dsp_common->application_idle();
    reposition_text_cursor();
}

void MDspResourceActions::action_start(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_START);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Start resource ");
    text.append(rsc_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_stop(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDSETUP_CMD);
    command->add_argument(drbdcmd::ARG_STOP);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Stop resource ");
    text.append(rsc_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_primary(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDSETUP_CMD);
    command->add_argument(drbdcmd::ARG_PRIMARY);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Switch to primary, resource ");
    text.append(rsc_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_force_primary(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDSETUP_CMD);
    command->add_argument(drbdcmd::ARG_PRIMARY);
    command->add_argument(drbdcmd::ARG_FORCE);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Force switch to primary, resource ");
    text.append(rsc_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_secondary(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDSETUP_CMD);
    command->add_argument(drbdcmd::ARG_SECONDARY);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Switch to secondary, resource ");
    text.append(rsc_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_force_secondary(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDSETUP_CMD);
    command->add_argument(drbdcmd::ARG_SECONDARY);
    command->add_argument(drbdcmd::ARG_FORCE);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Force switch to secondary, resource ");
    text.append(rsc_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_adjust(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_ADJUST);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Adjust resource ");
    text.append(rsc_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_adjust_skip_disk(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_ADJUST);
    command->add_argument(drbdcmd::ARG_SKIP_DISK);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Adjust resource ");
    text.append(rsc_name);
    text.append(", skip disk actions");

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_adjust_skip_net(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_ADJUST);
    command->add_argument(drbdcmd::ARG_SKIP_NET);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Adjust resource ");
    text.append(rsc_name);
    text.append(", skip network actions");

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_adjust_skip_disk_net(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_ADJUST);
    command->add_argument(drbdcmd::ARG_SKIP_DISK);
    command->add_argument(drbdcmd::ARG_SKIP_NET);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Adjust resource ");
    text.append(rsc_name);
    text.append(", skip disk & network actions");

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_verify(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_VERIFY);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Start online verification, resource ");
    text.append(rsc_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_pause_sync(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_PAUSE_SYNC);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Pause resync, resource ");
    text.append(rsc_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_resume_sync(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_RESUME_SYNC);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Resume resync, resource ");
    text.append(rsc_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_connect(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_CONNECT);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Connect resource ");
    text.append(rsc_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_disconnect(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_DISCONNECT);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Disconnect resource ");
    text.append(rsc_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_connect_discard(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_CONNECT);
    command->add_argument(drbdcmd::ARG_DISCARD);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Discard data, connect resource ");
    text.append(rsc_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspResourceActions::action_invalidate(const std::string& rsc_name)
{
    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_INVALIDATE);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Invalidate local data, resource ");
    text.append(rsc_name);
    text.append(", all volumes");

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

uint64_t MDspResourceActions::get_update_mask() noexcept
{
    return update_event::UPDATE_FLAG_DRBD;
}

bool MDspResourceActions::key_pressed(const uint32_t key)
{
    bool intercepted = MDspMenuBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::RSC_ACTIONS, dsp_comp_hub);
            intercepted = true;
        }
    }
    return intercepted;
}
