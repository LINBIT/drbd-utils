#include <terminal/MDspMainMenu.h>
#include <terminal/KeyCodes.h>
#include <terminal/DisplayUpdateEvent.h>
#include <terminal/HelpText.h>
#include <subprocess/SubProcessQueue.h>
#include <subprocess/CmdLine.h>
#include <subprocess/DrbdCmdConsts.h>
#include <comparators.h>

MDspMainMenu::MDspMainMenu(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub)
{
    cmd_fn_rsc_ovw =
        [this]() -> void
        {
            opt_resource_overview();
        };
    cmd_fn_log =
        [this]() -> void
        {
            opt_log();
        };
    cmd_fn_act_tsk =
        [this]() -> void
        {
            opt_active_tasks();
        };
    cmd_fn_pnd_tsk =
        [this]() -> void
        {
            opt_pending_tasks();
        };
    cmd_fn_ssp_tsk =
        [this]() -> void
        {
            opt_suspended_tasks();
        };
    cmd_fn_fin_tsk =
        [this]() -> void
        {
            opt_finished_tasks();
        };
    cmd_fn_help_idx =
        [this]() -> void
        {
            opt_help_index();
        };
    cmd_fn_about =
        [this]() -> void
        {
            opt_about_drbdmon();
        };
    cmd_fn_configuration =
        [this]() -> void
        {
            opt_configuration();
        };
    cmd_fn_start_all_rsc =
        [this]() -> void
        {
            opt_start_all_resources();
        };
    cmd_fn_stop_all_rsc =
        [this]() -> void
        {
            opt_stop_all_resources();
        };
    cmd_fn_exit =
        [this]() -> void
        {
            opt_exit();
        };

    cmd_rsc_ovw = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "1", 1, 6, 5, 45,
            cmd_fn_rsc_ovw
        )
    );
    cmd_log = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "2", 1, 7, 5, 45,
            cmd_fn_log
        )
    );
    cmd_act_tsk = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "3", 1, 8, 5, 45,
            cmd_fn_act_tsk
        )
    );
    cmd_pnd_tsk = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "4", 1, 9, 5, 45,
            cmd_fn_pnd_tsk
        )
    );
    cmd_ssp_tsk = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "5", 1, 10, 5, 45,
            cmd_fn_ssp_tsk
        )
    );
    cmd_fin_tsk = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "6", 1, 11, 5, 45,
            cmd_fn_fin_tsk
        )
    );
    cmd_help_idx = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "7", 1, 6, 50, 90,
            cmd_fn_help_idx
        )
    );
    cmd_about = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "8", 1, 7, 50, 90,
            cmd_fn_about
        )
    );
    cmd_configuration = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "9", 1, 8, 50, 90,
            cmd_fn_configuration
        )
    );
    cmd_start_all_rsc = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "90", 1, 13, 5, 45,
            cmd_fn_start_all_rsc
        )
    );
    cmd_stop_all_rsc = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "99", 1, 13, 50, 90,
            cmd_fn_stop_all_rsc
        )
    );
    cmd_exit = std::unique_ptr<ClickableCommand>(
        new ClickableCommand(
            "X", 1, 15, 5, 45,
            cmd_fn_exit
        )
    );

    add_option(*cmd_rsc_ovw);
    add_option(*cmd_log);
    add_option(*cmd_act_tsk);
    add_option(*cmd_pnd_tsk);
    add_option(*cmd_ssp_tsk);
    add_option(*cmd_fin_tsk);
    add_option(*cmd_help_idx);
    add_option(*cmd_about);
    add_option(*cmd_configuration);

    add_option(*cmd_start_all_rsc);
    add_option(*cmd_stop_all_rsc);

    add_option(*cmd_exit);

    InputField& option_field = get_option_field();
    option_field.set_position(17, 17);
}

MDspMainMenu::~MDspMainMenu() noexcept
{
    clear_options();
}

void MDspMainMenu::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_MAIN_MENU);

    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text("Main menu");

    const std::string& std_color = dsp_comp_hub.active_color_table->option_text;
    const std::string& caution_color = dsp_comp_hub.active_color_table->caution_text;

    display_option(" 1   ", "Resource overview", *cmd_rsc_ovw, std_color);
    display_option(" 2   ", "Message log", *cmd_log, std_color);
    display_option(" 3   ", "Active tasks queue", *cmd_act_tsk, std_color);
    display_option(" 4   ", "Pending tasks queue", *cmd_pnd_tsk, std_color);
    display_option(" 5   ", "Suspended tasks queue", *cmd_ssp_tsk, std_color);
    display_option(" 6   ", "Finished tasks queue", *cmd_fin_tsk, std_color);
    display_option(" 7   ", "Help index", *cmd_help_idx, std_color);
    display_option(" 8   ", "About DRBDmon", *cmd_about, std_color);
    display_option(" 9   ", "DRBDmon configuration", *cmd_configuration, std_color);

    if (dsp_comp_hub.enable_drbd_actions)
    {
        display_option("90   ", "Start/adjust all resources", *cmd_start_all_rsc, std_color);
        display_option("99   ", "Stop all resources", *cmd_stop_all_rsc, caution_color);
    }

    display_option(" X   ", "Exit DRBDmon", *cmd_exit, std_color);

    display_option_query(5, 17);
}

bool MDspMainMenu::key_pressed(const uint32_t key)
{
    bool intercepted = false;
    // Intercept keys that don't make any sense on the main menu
    if (key != KeyCodes::PG_UP && key != KeyCodes::PG_DOWN && key != KeyCodes::FUNC_06)
    {
        intercepted = MDspMenuBase::key_pressed(key);
        if (!intercepted)
        {
            if (key == KeyCodes::FUNC_01)
            {
                helptext::open_help_page(helptext::id_type::MAIN_MENU, dsp_comp_hub);
                intercepted = true;
            }
        }
    }
    else
    {
        intercepted = true;
    }
    return intercepted;
}

bool MDspMainMenu::mouse_action(MouseEvent& mouse)
{
    bool intercepted = false;
    if (!(mouse.coord_row == DisplayConsts::PAGE_NAV_Y &&
        mouse.coord_column >= dsp_comp_hub.term_cols - DisplayConsts::PAGE_NAV_X + 4))
    {
        intercepted = MDspMenuBase::mouse_action(mouse);
    }
    else
    {
        intercepted = true;
    }
    return intercepted;
}

void MDspMainMenu::opt_resource_overview()
{
    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::RSC_LIST);
}

void MDspMainMenu::opt_log()
{
    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::LOG_VIEWER);
}

void MDspMainMenu::opt_active_tasks()
{
    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::TASKQ_ACT);
}

void MDspMainMenu::opt_pending_tasks()
{
    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::TASKQ_PND);
}

void MDspMainMenu::opt_suspended_tasks()
{
    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::TASKQ_SSP);
}

void MDspMainMenu::opt_finished_tasks()
{
    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::TASKQ_FIN);
}

void MDspMainMenu::opt_help_index()
{
    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::HELP_IDX);
}

void MDspMainMenu::opt_about_drbdmon()
{
    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::PGM_INFO);
}

void MDspMainMenu::opt_configuration()
{
    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::CONFIGURATION);
}

void MDspMainMenu::opt_start_all_resources()
{
    if (dsp_comp_hub.enable_drbd_actions)
    {
        try
        {
            std::unique_ptr<CmdLine> command(new CmdLine());
            command->add_argument(drbdcmd::DRBDADM_CMD);
            command->add_argument(drbdcmd::ARG_ADJUST);
            command->add_argument(drbdcmd::ARG_ALL);

            std::string text("Start all resources");

            command->set_description(text);

            dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
        }
        catch (SubProcessQueue::QueueCapacityException&)
        {
            dsp_comp_hub.log->add_entry(
                MessageLog::log_level::ALERT,
                "Command start all resources: Cannot execute, insufficient queue capacity"
            );
        }
        catch (SubProcess::Exception&)
        {
            dsp_comp_hub.log->add_entry(
                MessageLog::log_level::ALERT,
                "Command start all resources: Sub-process execution failed"
            );
        }

        dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::RSC_LIST);
    }
}

void MDspMainMenu::opt_stop_all_resources()
{
    if (dsp_comp_hub.enable_drbd_actions)
    {
        try
        {
            std::unique_ptr<CmdLine> command(new CmdLine());
            command->add_argument(drbdcmd::DRBDSETUP_CMD);
            command->add_argument(drbdcmd::ARG_STOP);
            command->add_argument(drbdcmd::ARG_ALL);

            std::string text("Stop all resources");

            command->set_description(text);

            dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
        }
        catch (SubProcessQueue::QueueCapacityException&)
        {
            dsp_comp_hub.log->add_entry(
                MessageLog::log_level::ALERT,
                "Command stop all resources: Cannot execute, insufficient queue capacity"
            );
        }
        catch (SubProcess::Exception&)
        {
            dsp_comp_hub.log->add_entry(
                MessageLog::log_level::ALERT,
                "Command stop all resources: Sub-process execution failed"
            );
        }

        dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::RSC_LIST);
    }
}

void MDspMainMenu::opt_exit()
{
    dsp_comp_hub.core_instance->shutdown(DrbdMonCore::finish_action::TERMINATE);
}

uint64_t MDspMainMenu::get_update_mask() noexcept
{
    return 0;
}
