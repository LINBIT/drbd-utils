#include <terminal/MDspConnectionActions.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/DisplayConsts.h>
#include <terminal/KeyCodes.h>
#include <terminal/HelpText.h>
#include <objects/DrbdResource.h>
#include <subprocess/SubProcessQueue.h>
#include <subprocess/CmdLine.h>
#include <subprocess/DrbdCmdConsts.h>
#include <memory>

const uint8_t   MDspConnectionActions::OPT_LIST_Y = 6;

MDspConnectionActions::MDspConnectionActions(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub)
{
    cmd_fn_connect =
        [this]() -> void
        {
            selection_action(&MDspConnectionActions::action_connect);
        };
    cmd_fn_disconnect =
        [this]() -> void
        {
            selection_action(&MDspConnectionActions::action_disconnect);
        };
    cmd_fn_discard =
        [this]() -> void
        {
            selection_action(&MDspConnectionActions::action_connect_discard);
        };

    uint16_t opt_line   = OPT_LIST_Y;
    uint16_t start_col  = 5;
    uint16_t end_col    = 45;

    cmd_connect = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "1", 1, opt_line, start_col, end_col,
            cmd_fn_connect
        )
    );
    ++opt_line;

    cmd_disconnect = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "2", 1, opt_line, start_col, end_col,
            cmd_fn_disconnect
        )
    );
    ++opt_line;

    cmd_discard = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "3", 1, opt_line, start_col, end_col,
            cmd_fn_discard
        )
    );
    ++opt_line;

    add_option(*cmd_connect);
    add_option(*cmd_disconnect);
    add_option(*cmd_discard);
}

MDspConnectionActions::~MDspConnectionActions() noexcept
{
}


void MDspConnectionActions::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_CON_ACT);

    dsp_comp_hub.dsp_io->cursor_xy(1, DisplayConsts::PAGE_NAV_Y + 1);
    dsp_comp_hub.dsp_io->write_text("Connection actions: ");

    if (!dsp_comp_hub.dsp_shared->monitor_rsc.empty())
    {
        DrbdResource* const rsc = dsp_comp_hub.get_monitor_resource();
        if (rsc != nullptr)
        {
            dsp_comp_hub.dsp_io->write_text("Resource ");
            dsp_comp_hub.dsp_io->write_string_field(
                dsp_comp_hub.dsp_shared->monitor_rsc,
                dsp_comp_hub.term_cols - 30,
                false
            );

            dsp_comp_hub.dsp_io->cursor_xy(21, DisplayConsts::PAGE_NAV_Y + 2);
            if (!dsp_comp_hub.dsp_shared->ovrd_connection_selection &&
                dsp_comp_hub.dsp_shared->have_connections_selection())
            {
                ConnectionsMap& selection_map = dsp_comp_hub.dsp_shared->get_selected_connections_map();
                const size_t count = selection_map.get_size();
                dsp_comp_hub.dsp_io->write_fmt("%lu selected connections", static_cast<unsigned long> (count));
            }
            else
            if (!dsp_comp_hub.dsp_shared->monitor_con.empty())
            {
                dsp_comp_hub.dsp_io->write_text("Connection ");
                dsp_comp_hub.dsp_io->write_string_field(
                    dsp_comp_hub.dsp_shared->monitor_con,
                    dsp_comp_hub.term_cols - 32,
                    false
                );
            }
        }
        else
        {
            dsp_comp_hub.dsp_io->write_text("Resource ");
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
        dsp_comp_hub.dsp_io->write_text("No selected resource");
    }

    const std::string& std_color = dsp_comp_hub.active_color_table->option_text;
    const std::string& caution_color = dsp_comp_hub.active_color_table->caution_text;

    display_option(" 1   ", "Connect", *cmd_connect, std_color);
    display_option(" 2   ", "Disconnect", *cmd_disconnect, std_color);
    display_option(" 3   ", "Connect & resolve split-brain", *cmd_discard, caution_color);

    display_option_query(5, 10);
}

void MDspConnectionActions::selection_action(const action_func_type action_func)
{
    try
    {
        dsp_comp_hub.dsp_common->application_working();
        if (!dsp_comp_hub.dsp_shared->ovrd_connection_selection &&
            dsp_comp_hub.dsp_shared->have_connections_selection())
        {
            ConnectionsMap& selection_map = dsp_comp_hub.dsp_shared->get_selected_connections_map();
            ConnectionsMap::KeysIterator iter(selection_map);

            const std::string& rsc_name = dsp_comp_hub.dsp_shared->monitor_rsc;
            if (!rsc_name.empty())
            {
                while (iter.has_next())
                {
                    const std::string* const con_name_ptr = iter.next();
                    (this->*action_func)(rsc_name, *con_name_ptr);
                }
                dsp_comp_hub.dsp_selector->leave_display();
            }
        }
        else
        {
            const std::string& rsc_name = dsp_comp_hub.dsp_shared->monitor_rsc;
            const std::string& con_name = dsp_comp_hub.dsp_shared->monitor_con;

            if (!rsc_name.empty() && !con_name.empty())
            {
                (this->*action_func)(rsc_name, con_name);
                dsp_comp_hub.dsp_selector->leave_display();
            }
        }
    }
    catch (SubProcessQueue::QueueCapacityException&)
    {
        dsp_comp_hub.log->add_entry(
            MessageLog::log_level::ALERT,
            "Connection actions: Cannot execute command, insufficient queue capacity"
        );
    }
    catch (SubProcess::Exception&)
    {
        dsp_comp_hub.log->add_entry(
            MessageLog::log_level::ALERT,
            "Connection actions: Command failed: Sub-process execution error"
        );
    }
    dsp_comp_hub.dsp_common->application_idle();
    reposition_text_cursor();
}

void MDspConnectionActions::action_connect(const std::string& rsc_name, const std::string& con_name)
{
    std::string cmd_target(rsc_name);
    cmd_target.append(":");
    cmd_target.append(con_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_CONNECT);
    command->add_argument(cmd_target);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Connect resource ");
    text.append(rsc_name);
    text.append(" connection to ");
    text.append(con_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspConnectionActions::action_disconnect(const std::string& rsc_name, const std::string& con_name)
{
    std::string cmd_target(rsc_name);
    cmd_target.append(":");
    cmd_target.append(con_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_DISCONNECT);
    command->add_argument(cmd_target);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Disconnect resource ");
    text.append(rsc_name);
    text.append(" connection to ");
    text.append(con_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

void MDspConnectionActions::action_connect_discard(const std::string& rsc_name, const std::string& con_name)
{
    std::string cmd_target(rsc_name);
    cmd_target.append(":");
    cmd_target.append(con_name);

    std::unique_ptr<CmdLine> command(new CmdLine());
    command->add_argument(drbdcmd::DRBDADM_CMD);
    command->add_argument(drbdcmd::ARG_CONNECT);
    command->add_argument(drbdcmd::ARG_DISCARD);
    command->add_argument(cmd_target);

    std::string text;
    text.reserve(DisplayConsts::ACTION_DESC_PREALLOC);
    text.append("Discard data, connect resource ");
    text.append(rsc_name);
    text.append(" connection to ");
    text.append(con_name);

    command->set_description(text);

    dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
}

uint64_t MDspConnectionActions::get_update_mask() noexcept
{
    return update_event::UPDATE_FLAG_DRBD;
}

bool MDspConnectionActions::key_pressed(const uint32_t key)
{
    bool intercepted = MDspMenuBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::CON_ACTIONS, dsp_comp_hub);
            intercepted = true;
        }
    }
    return intercepted;
}
