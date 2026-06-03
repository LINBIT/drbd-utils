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
    cmd_fn_adjust_all_rsc_skip_disk =
        [this]() -> void
        {
            opt_adjust_all_skip_disk();
        };
    cmd_fn_adjust_all_rsc_skip_net =
        [this]() -> void
        {
            opt_adjust_all_skip_net();
        };
    cmd_fn_adjust_all_rsc_skip_disk_net =
        [this]() -> void
        {
            opt_adjust_all_skip_disk_net();
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

    cmd_fn_selection_filter =
        [this]() -> void
        {
            opt_selection_filter();
        };
    cmd_fn_bulk_actions =
        [this]() -> void
        {
            opt_bulk_actions();
        };
    cmd_fn_export_selection =
        [this]() -> void
        {
            opt_export_selection();
        };
    cmd_fn_import_selection =
        [this]() -> void
        {
            opt_import_selection();
        };
    cmd_fn_overview =
        [this]() -> void
        {
            opt_overview();
        };

    ClickableCommand::Builder bld;
    bld.coords.page = 1;
    bld.coords.start_col = 5;
    bld.coords.end_col = 45;
    bld.coords.row = 6;
    bld.auto_nr = 1;

    cmd_rsc_ovw = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_rsc_ovw));
    cmd_log = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_log));
    cmd_act_tsk = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_act_tsk));
    cmd_pnd_tsk = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_pnd_tsk));
    cmd_ssp_tsk = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_ssp_tsk));
    cmd_fin_tsk = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_fin_tsk));


    bld.coords.start_col = 50;
    bld.coords.end_col = 90;
    bld.coords.row = 6;

    cmd_help_idx = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_help_idx));
    cmd_about = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_about));
    cmd_configuration = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_configuration));

    bld.coords.row = 15;
    bld.coords.start_col = 5;
    bld.coords.end_col = 45;
    cmd_exit = std::unique_ptr<ClickableCommand>(bld.create_with_id("X", cmd_fn_exit));

    bld.coords.page = 2;

    bld.auto_nr = 50;
    bld.coords.start_col = 5;
    bld.coords.end_col = 45;
    bld.coords.row = 6;

    cmd_selection_filter = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_selection_filter));
    cmd_bulk_actions = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_bulk_actions));
    cmd_export_selection = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_export_selection));
    cmd_import_selection = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_import_selection));

    bld.auto_nr = 60;
    ++bld.coords.row;
    cmd_overview = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_overview));

    bld.coords.page = 3;

    bld.auto_nr = 90;
    // Long lines for the start/adjust options
    bld.coords.start_col = 5;
    bld.coords.end_col = 90;
    bld.coords.row = 6;

    cmd_start_all_rsc = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_start_all_rsc));
    cmd_adjust_all_rsc_skip_disk = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_adjust_all_rsc_skip_disk)
    );
    cmd_adjust_all_rsc_skip_net = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_adjust_all_rsc_skip_net)
    );
    cmd_adjust_all_rsc_skip_disk_net = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_adjust_all_rsc_skip_disk_net)
    );

    bld.auto_nr = 99;

    cmd_stop_all_rsc = std::unique_ptr<ClickableCommand>(bld.create_with_auto_nr(cmd_fn_stop_all_rsc));

    add_option(*cmd_rsc_ovw);
    add_option(*cmd_log);
    add_option(*cmd_act_tsk);
    add_option(*cmd_pnd_tsk);
    add_option(*cmd_ssp_tsk);
    add_option(*cmd_fin_tsk);
    add_option(*cmd_help_idx);
    add_option(*cmd_about);
    add_option(*cmd_configuration);

    add_option(*cmd_exit);

    add_option(*cmd_selection_filter);
    add_option(*cmd_bulk_actions);
    add_option(*cmd_export_selection);
    add_option(*cmd_import_selection);

    add_option(*cmd_overview);

    add_option(*cmd_start_all_rsc);
    add_option(*cmd_adjust_all_rsc_skip_disk);
    add_option(*cmd_adjust_all_rsc_skip_net);
    add_option(*cmd_adjust_all_rsc_skip_disk_net);
    add_option(*cmd_stop_all_rsc);

    InputField& option_field = get_option_field();
    option_field.set_position(17, 17);

    set_page_count(bld.coords.page);
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

    const uint32_t page = get_page_nr();
    if (page == 1)
    {
        display_option(5, "Resource overview", *cmd_rsc_ovw, std_color);
        display_option(5, "Message log", *cmd_log, std_color);
        display_option(5, "Active tasks queue", *cmd_act_tsk, std_color);
        display_option(5, "Pending tasks queue", *cmd_pnd_tsk, std_color);
        display_option(5, "Suspended tasks queue", *cmd_ssp_tsk, std_color);
        display_option(5, "Finished tasks queue", *cmd_fin_tsk, std_color);
        display_option(5, "Help index", *cmd_help_idx, std_color);
        display_option(5, "About DRBDmon", *cmd_about, std_color);
        display_option(5, "DRBDmon configuration", *cmd_configuration, std_color);

        display_option(5, "Exit DRBDmon", *cmd_exit, std_color);
    }
    else
    if (page == 2)
    {
        display_option(5, "Selection filter", *cmd_selection_filter, std_color);
        display_option(5, "Bulk actions", *cmd_bulk_actions, std_color);
        display_option(5, "Export selection", *cmd_export_selection, std_color);
        display_option(5, "Import selection", *cmd_import_selection, std_color);

        display_option(5, "DRBD state overview", *cmd_overview, std_color);
    }
    else
    if (page == 3)
    {
        if (dsp_comp_hub.enable_drbd_actions)
        {
            display_option(5, "Start/adjust all resources", *cmd_start_all_rsc, std_color);
            display_option(
                5, "Start/adjust all resources, skip disk actions",
                *cmd_adjust_all_rsc_skip_disk, std_color
            );
            display_option(
                5, "Start/adjust all resources, skip network actions",
                *cmd_adjust_all_rsc_skip_net, std_color
            );
            display_option(
                5, "Start/adjust all resources, skip disk & network actions",
                *cmd_adjust_all_rsc_skip_disk_net, std_color
            );
            display_option(5, "Stop all resources", *cmd_stop_all_rsc, caution_color);
        }
        else
        {
            dsp_comp_hub.dsp_io->cursor_xy(6, 5);
            dsp_comp_hub.dsp_io->write_text("DRBD actions are currently disabled");
        }
    }

    display_option_query(5, 17);
}

void MDspMainMenu::display_activated()
{
    MDspMenuBase::display_activated();
    set_page_nr(1);
}

bool MDspMainMenu::key_pressed(const uint32_t key)
{
    bool intercepted = MDspMenuBase::key_pressed(key);
    if (!intercepted)
    {
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::MAIN_MENU, dsp_comp_hub);
            intercepted = true;
        }
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

void MDspMainMenu::opt_selection_filter()
{
    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::SLCT_FILTER);
}

void MDspMainMenu::opt_bulk_actions()
{
    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::BULK_ACT);
}

void MDspMainMenu::opt_export_selection()
{
    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::EXPORT_SLCT);
}

void MDspMainMenu::opt_import_selection()
{
    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::IMPORT_SLCT);
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

void MDspMainMenu::opt_adjust_all_skip_disk()
{
    if (dsp_comp_hub.enable_drbd_actions)
    {
        try
        {
            std::unique_ptr<CmdLine> command(new CmdLine());
            command->add_argument(drbdcmd::DRBDADM_CMD);
            command->add_argument(drbdcmd::ARG_ADJUST);
            command->add_argument(drbdcmd::ARG_SKIP_DISK);
            command->add_argument(drbdcmd::ARG_ALL);

            std::string text("Adjust all resources, skip disk actions");

            command->set_description(text);

            dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
        }
        catch (SubProcessQueue::QueueCapacityException&)
        {
            dsp_comp_hub.log->add_entry(
                MessageLog::log_level::ALERT,
                "Command adjust all resources (skip disk actions): Cannot execute, insufficient queue capacity"
            );
        }
        catch (SubProcess::Exception&)
        {
            dsp_comp_hub.log->add_entry(
                MessageLog::log_level::ALERT,
                "Command adjust all resources (skip disk actions): Sub-process execution failed"
            );
        }

        dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::RSC_LIST);
    }
}

void MDspMainMenu::opt_adjust_all_skip_net()
{
    if (dsp_comp_hub.enable_drbd_actions)
    {
        try
        {
            std::unique_ptr<CmdLine> command(new CmdLine());
            command->add_argument(drbdcmd::DRBDADM_CMD);
            command->add_argument(drbdcmd::ARG_ADJUST);
            command->add_argument(drbdcmd::ARG_SKIP_NET);
            command->add_argument(drbdcmd::ARG_ALL);

            std::string text("Adjust all resources, skip network actions");

            command->set_description(text);

            dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
        }
        catch (SubProcessQueue::QueueCapacityException&)
        {
            dsp_comp_hub.log->add_entry(
                MessageLog::log_level::ALERT,
                "Command adjust all resources (skip network actions): Cannot execute, insufficient queue capacity"
            );
        }
        catch (SubProcess::Exception&)
        {
            dsp_comp_hub.log->add_entry(
                MessageLog::log_level::ALERT,
                "Command adjust all resources (skip network actions): Sub-process execution failed"
            );
        }

        dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::RSC_LIST);
    }
}

void MDspMainMenu::opt_adjust_all_skip_disk_net()
{
    if (dsp_comp_hub.enable_drbd_actions)
    {
        try
        {
            std::unique_ptr<CmdLine> command(new CmdLine());
            command->add_argument(drbdcmd::DRBDADM_CMD);
            command->add_argument(drbdcmd::ARG_ADJUST);
            command->add_argument(drbdcmd::ARG_SKIP_DISK);
            command->add_argument(drbdcmd::ARG_SKIP_NET);
            command->add_argument(drbdcmd::ARG_ALL);

            std::string text("Adjust all resources, skip disk & network actions");

            command->set_description(text);

            dsp_comp_hub.sub_proc_queue->add_entry(command, dsp_comp_hub.dsp_shared->activate_tasks);
        }
        catch (SubProcessQueue::QueueCapacityException&)
        {
            dsp_comp_hub.log->add_entry(
                MessageLog::log_level::ALERT,
                "Command adjust all resources (skip disk & network actions): Cannot execute, "
                "insufficient queue capacity"
            );
        }
        catch (SubProcess::Exception&)
        {
            dsp_comp_hub.log->add_entry(
                MessageLog::log_level::ALERT,
                "Command adjust all resources (skip disk & network actions): Sub-process execution failed"
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

void MDspMainMenu::opt_overview()
{
    dsp_comp_hub.dsp_selector->switch_to_display(DisplayId::display_page::OVERVIEW);
}

void MDspMainMenu::opt_exit()
{
    dsp_comp_hub.core_instance->shutdown(DrbdMonCore::finish_action::TERMINATE);
}

uint64_t MDspMainMenu::get_update_mask() noexcept
{
    return 0;
}
