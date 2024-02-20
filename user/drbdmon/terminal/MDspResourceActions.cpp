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
    cmd_fn_invalidate =
        [this]() -> void
        {
            selection_action(&MDspResourceActions::action_invalidate);
        };

    // Left column

    uint16_t opt_line   = OPT_LIST_Y;
    uint16_t start_col  = 5;
    uint16_t end_col    = 45;

    cmd_start = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "1", 1, opt_line, start_col, end_col,
            cmd_fn_start
        )
    );
    ++opt_line;

    cmd_stop = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "2", 1, opt_line, start_col, end_col,
            cmd_fn_stop
        )
    );
    ++opt_line;

    cmd_adjust = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "3", 1, opt_line, start_col, end_col,
            cmd_fn_adjust
        )
    );
    ++opt_line;

    cmd_primary = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "4", 1, opt_line, start_col, end_col,
            cmd_fn_primary
        )
    );
    ++opt_line;

    cmd_secondary = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "5", 1, opt_line, start_col, end_col,
            cmd_fn_secondary
        )
    );
    ++opt_line;

    cmd_connect = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "6", 1, opt_line, start_col, end_col,
            cmd_fn_connect
        )
    );
    ++opt_line;

    cmd_disconnect = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "7", 1, opt_line, start_col, end_col,
            cmd_fn_disconnect
        )
    );
    ++opt_line;

    cmd_verify = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "8", 1, opt_line, start_col, end_col,
            cmd_fn_verify
        )
    );
    ++opt_line;

    cmd_pause_sync = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "9", 1, opt_line, start_col, end_col,
            cmd_fn_pause_sync
        )
    );
    ++opt_line;

    cmd_resume_sync = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "10", 1, opt_line, start_col, end_col,
            cmd_fn_resume_sync
        )
    );
    ++opt_line;

    // Right column

    opt_line    = OPT_LIST_Y;
    start_col   = 45;
    end_col     = 90;

    cmd_force_primary = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "11", 1, opt_line, start_col, end_col,
            cmd_fn_force_primary
        )
    );
    ++opt_line;

    cmd_connect_discard = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "12", 1, opt_line, start_col, end_col,
            cmd_fn_connect_discard
        )
    );
    ++opt_line;

    cmd_invalidate = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "13", 1, opt_line, start_col, end_col,
            cmd_fn_invalidate
        )
    );
    ++opt_line;

    add_option(*cmd_start);
    add_option(*cmd_stop);
    add_option(*cmd_adjust);
    add_option(*cmd_primary);
    add_option(*cmd_force_primary);
    add_option(*cmd_secondary);
    add_option(*cmd_connect);
    add_option(*cmd_disconnect);
    add_option(*cmd_verify);
    add_option(*cmd_pause_sync);
    add_option(*cmd_resume_sync);
    add_option(*cmd_connect_discard);
    add_option(*cmd_invalidate);
}

MDspResourceActions::~MDspResourceActions() noexcept
{
}

void MDspResourceActions::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_RSC_ACT);

    dsp_comp_hub.dsp_io->cursor_xy(1, DisplayConsts::PAGE_NAV_Y + 1);
    dsp_comp_hub.dsp_io->write_text("Resource actions: ");
    if (!dsp_comp_hub.dsp_shared->ovrd_resource_selection && dsp_comp_hub.dsp_shared->have_resources_selection())
    {
        ResourcesMap& selection_map = dsp_comp_hub.dsp_shared->get_selected_resources_map();
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

    display_option(" 1   ", "Start resource", *cmd_start, std_color);
    display_option(" 2   ", "Stop resource", *cmd_stop, std_color);
    display_option(" 3   ", "Adjust resource", *cmd_adjust, std_color);
    display_option(" 4   ", "Make primary", *cmd_primary, std_color);
    display_option(" 5   ", "Make secondary", *cmd_secondary, std_color);
    display_option(" 6   ", "Connect", *cmd_connect, std_color);
    display_option(" 7   ", "Disconnect", *cmd_disconnect, std_color);
    display_option(" 8   ", "Run verification", *cmd_verify, std_color);
    display_option(" 9   ", "Pause resynchronization", *cmd_pause_sync, std_color);
    display_option("10   ", "Resume resynchronization", *cmd_resume_sync, std_color);
    display_option("11   ", "Force make primary", *cmd_force_primary, caution_color);
    display_option("12   ", "Discard & resolve split-brain", *cmd_connect_discard, caution_color);
    display_option("13   ", "Invalidate & resynchronize", *cmd_invalidate, caution_color);

    display_option_query(5, 16);
}

void MDspResourceActions::selection_action(const action_func_type action_func)
{
    try
    {
        dsp_comp_hub.dsp_common->application_working();
        if (!dsp_comp_hub.dsp_shared->ovrd_resource_selection && dsp_comp_hub.dsp_shared->have_resources_selection())
        {
            ResourcesMap& selection_map = dsp_comp_hub.dsp_shared->get_selected_resources_map();
            ResourcesMap::KeysIterator iter(selection_map);
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
    command->add_argument(drbdcmd::DRBDADM_CMD);
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
    command->add_argument(drbdcmd::DRBDADM_CMD);
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
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_SECONDARY);
    command->add_argument(rsc_name);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Switch to secondary, resource ");
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
