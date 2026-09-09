#include <terminal/MDspBulkActions.h>
#include <terminal/SharedData.h>
#include <terminal/DrbdCommandsImpl.h>
#include <terminal/HelpText.h>
#include <terminal/KeyCodes.h>
#include <cppdsaext/src/integerparse.h>

MDspBulkActions::MDspBulkActions(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub)
{
    skip_count_input = std::unique_ptr<InputField>(new InputField(dsp_comp_hub, 5, 5, 8, 8));
    apply_count_input = std::unique_ptr<InputField>(new InputField(dsp_comp_hub, 5, 8, 8, 8));

    rsc_program_input = std::unique_ptr<InputField>(new InputField(dsp_comp_hub, 5, 14, 400, 80));
    vlm_program_input = std::unique_ptr<InputField>(new InputField(dsp_comp_hub, 5, 14, 400, 80));
    con_program_input = std::unique_ptr<InputField>(new InputField(dsp_comp_hub, 5, 14, 400, 80));
    peer_vlm_program_input = std::unique_ptr<InputField>(new InputField(dsp_comp_hub, 5, 14, 400, 80));

    setup_cmd_functions();
    setup_pages();

    range_info_msg.reserve(90);
}

MDspBulkActions::~MDspBulkActions() noexcept
{
}

void MDspBulkActions::display_closed()
{
    reset_display();
    active_input = nullptr;
    set_page_nr(1);
    last_page = 0;
    if (!keep_range)
    {
        skip_count_input->clear_text();
        apply_count_input->clear_text();
        range_info_msg.clear();
        range_error_msg.clear();
    }
    rsc_program_input->clear_text();
    vlm_program_input->clear_text();
    con_program_input->clear_text();
    peer_vlm_program_input->clear_text();
    MDspMenuBase::display_closed();
}

void MDspBulkActions::display_content()
{
    const uint32_t page = get_page_nr();

    if (dsp_comp_hub.enable_drbd_actions)
    {
        if (page != last_page)
        {
            delegate_focus(false);
            active_input = nullptr;
        }

        if (page != range_page)
        {
            if (range_info_msg.empty() && (!skip_count_input->is_empty() || !apply_count_input->is_empty()))
            {
                try
                {
                    RangeSpec range = get_exec_range();
                    if (range.skip_count != 0)
                    {
                        range_info_msg = "Skip ";
                        range_info_msg += std::to_string(static_cast<unsigned long> (range.skip_count));
                    }
                    if (range.apply_count != 0)
                    {
                        range_info_msg += (range_info_msg.empty() ? "Process " : ", process ");
                        range_info_msg += std::to_string(static_cast<unsigned long> (range.apply_count));
                    }
                }
                catch (dsaext::NumberFormatException&)
                {
                    // no-op
                }
            }
        }

        display_actions();
    }
    else
    {
        dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_BULK_ACT);

        dsp_comp_hub.dsp_io->cursor_xy(1, DisplayConsts::PAGE_NAV_Y + 3);
        dsp_comp_hub.dsp_io->write_text("This page is currently disabled");
    }

    last_page = page;
}

void MDspBulkActions::display_actions()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_BULK_ACT);

    const uint32_t page = get_page_nr();
    if (page == rsc_page)
    {
        display_resource_actions();
    }
    else
    if (page == vlm_page)
    {
        display_volume_actions();
    }
    else
    if (page == con_page)
    {
        display_connection_actions();
    }
    else
    if (page == peer_vlm_page)
    {
        display_peer_volume_actions();
    }
    else
    if (page == range_page)
    {
        display_range_options();
    }

    if (page != range_page && !range_info_msg.empty() && range_error_msg.empty())
    {
        dsp_comp_hub.dsp_io->cursor_xy(5, 16);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
        dsp_comp_hub.dsp_io->write_text(range_info_msg.c_str());
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
    }

    display_option_query(5, 17);
}

void MDspBulkActions::display_resource_actions()
{
    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Execute action for all selected resources:");
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

    const std::string& std_color = dsp_comp_hub.active_color_table->option_text;
    const std::string& caution_color = dsp_comp_hub.active_color_table->caution_text;

    display_option(5, "Start (up)", *cmd_rsc_start, std_color);
    display_option(5, "Stop (down)", *cmd_rsc_stop, std_color);

    display_option(5, "Adjust", *cmd_rsc_adjust, std_color);
    display_option(5, "Adjust, skip disk actions", *cmd_rsc_adjust_skip_disk, std_color);
    display_option(5, "Adjust, skip network actions", *cmd_rsc_adjust_skip_net, std_color);
    display_option(5, "Adjust, skip disk & network actions", *cmd_rsc_adjust_skip_disk_net, std_color);
    display_option(5, "Set primary role", *cmd_rsc_primary, std_color);
    display_option(5, "Set secondary role", *cmd_rsc_secondary, std_color);
    display_option(5, "Force primary role", *cmd_rsc_force_primary, caution_color);
    display_option(5, "Force secondary role", *cmd_rsc_force_secondary, caution_color);

    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
    dsp_comp_hub.dsp_io->cursor_xy(3, 13);
    dsp_comp_hub.dsp_io->write_text("Execute resource actions program:");
    rsc_program_input->display();
    display_option(5, "Execute program", *cmd_rsc_program, std_color);
}

void MDspBulkActions::display_volume_actions()
{
    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Execute action for all selected volumes:");
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

    const std::string& std_color = dsp_comp_hub.active_color_table->option_text;
    const std::string& caution_color = dsp_comp_hub.active_color_table->caution_text;

    display_option(5, "Attach", *cmd_vlm_attach, std_color);
    display_option(5, "Detach", *cmd_vlm_detach, std_color);
    display_option(5, "Invalidate local volume data", *cmd_vlm_invalidate, caution_color);

    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
    dsp_comp_hub.dsp_io->cursor_xy(3, 13);
    dsp_comp_hub.dsp_io->write_text("Execute volume actions program:");
    vlm_program_input->display();
    display_option(5, "Execute program", *cmd_vlm_program, std_color);
}

void MDspBulkActions::display_connection_actions()
{
    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Execute action for all selected connections:");
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

    const std::string& std_color = dsp_comp_hub.active_color_table->option_text;
    const std::string& caution_color = dsp_comp_hub.active_color_table->caution_text;

    display_option(5, "Connect", *cmd_con_connect, std_color);
    display_option(5, "Disconnect", *cmd_con_disconnect, std_color);
    display_option(5, "Discard & resolve split-brain", *cmd_con_discard, caution_color);

    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
    dsp_comp_hub.dsp_io->cursor_xy(3, 13);
    dsp_comp_hub.dsp_io->write_text("Execute connection actions program:");
    con_program_input->display();
    display_option(5, "Execute program", *cmd_con_program, std_color);
}

void MDspBulkActions::display_peer_volume_actions()
{
    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Execute action for all selected peer volumes:");
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

    const std::string& std_color = dsp_comp_hub.active_color_table->option_text;
    const std::string& caution_color = dsp_comp_hub.active_color_table->caution_text;

    display_option(5, "Pause resynchronization", *cmd_peer_vlm_pause_sync, std_color);
    display_option(5, "Resume resynchronization", *cmd_peer_vlm_resume_sync, std_color);
    display_option(5, "Verify contents", *cmd_peer_vlm_verify, std_color);
    display_option(5, "Invalidate peer volume data", *cmd_peer_vlm_invalidate_remote, caution_color);

    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
    dsp_comp_hub.dsp_io->cursor_xy(3, 13);
    dsp_comp_hub.dsp_io->write_text("Execute peer volume actions program:");
    peer_vlm_program_input->display();
    display_option(5, "Execute program", *cmd_peer_vlm_program, std_color);
}

void MDspBulkActions::display_range_options()
{
    const std::string& std_color = dsp_comp_hub.active_color_table->option_text;

    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text("Number of objects to skip:");
    skip_count_input->display();

    dsp_comp_hub.dsp_io->cursor_xy(3, 7);
    dsp_comp_hub.dsp_io->write_text("Number of objects to process:");
    apply_count_input->display();

    std::string keep_range_label;
    keep_range_label.reserve(50);
    keep_range_label = keep_range ?
        dsp_comp_hub.active_character_table->checked_box : dsp_comp_hub.active_character_table->unchecked_box;
    keep_range_label += " Keep range when display is closed";
    display_option(5, keep_range_label.c_str(), *cmd_toggle_keep_range, std_color);

    if (!range_error_msg.empty())
    {
        dsp_comp_hub.dsp_io->cursor_xy(5, 12);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
        dsp_comp_hub.dsp_io->write_text(range_error_msg.c_str());
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
    }

    range_info_msg.clear();
}

void MDspBulkActions::toggle_keep_range()
{
    keep_range = !keep_range;
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspBulkActions::text_cursor_ops()
{
    if (is_focus_delegated() && active_input != nullptr)
    {
        active_input->cursor();
    }
    else
    {
        MDspMenuBase::text_cursor_ops();
    }
}

uint64_t MDspBulkActions::get_update_mask() noexcept
{
    return 0;
}

bool MDspBulkActions::key_pressed(const uint32_t key)
{
    bool intercepted = false;

    // Override the capture of the '/' key by the command line logic for those fields that take path arguments
    if (key == static_cast<uint32_t> ('/') &&
        (active_input == rsc_program_input.get() || active_input == vlm_program_input.get() ||
         active_input == con_program_input.get() || active_input == peer_vlm_program_input.get()))
    {
        active_input->key_pressed(key);
        intercepted = true;
    }

    if (!intercepted)
    {
        intercepted = MDspMenuBase::key_pressed(key);

        if (!intercepted)
        {
            if (key == KeyCodes::FUNC_01)
            {
                helptext::open_help_page(helptext::id_type::BULKA_HELP, dsp_comp_hub);
                intercepted = true;
            }
            else
            {
                if (is_focus_delegated() && active_input != nullptr)
                {
                    active_input->key_pressed(key);
                    intercepted = true;
                }
            }
        }
    }

    return intercepted;
}

bool MDspBulkActions::mouse_action(MouseEvent& mouse)
{
    bool intercepted = MDspMenuBase::mouse_action(mouse);
    if (!intercepted)
    {
        const uint32_t page = get_page_nr();
        if (page == range_page)
        {
            if (skip_count_input->mouse_action(mouse))
            {
                active_input = skip_count_input.get();
                intercepted = true;
            }
            else
            if (apply_count_input->mouse_action(mouse))
            {
                active_input = apply_count_input.get();
                intercepted = true;
            }

            if (intercepted)
            {
                delegate_focus(true);
            }
            else
            {
                InputField& option_field = get_option_field();
                if (option_field.mouse_action(mouse))
                {
                    active_input = nullptr;
                    delegate_focus(false);
                    intercepted = true;
                }
            }

            if (intercepted)
            {
                dsp_comp_hub.dsp_selector->refresh_display();
            }
        }
        else
        if (page == rsc_page)
        {
            intercepted = mouse_action_program_input(rsc_program_input, mouse);
        }
        else
        if (page == vlm_page)
        {
            intercepted = mouse_action_program_input(vlm_program_input, mouse);
        }
        else
        if (page == con_page)
        {
            intercepted = mouse_action_program_input(con_program_input, mouse);
        }
        else
        if (page == peer_vlm_page)
        {
            intercepted = mouse_action_program_input(peer_vlm_program_input, mouse);
        }
    }
    return intercepted;
}

void MDspBulkActions::cursor_to_next_item()
{
    const uint32_t page = get_page_nr();
    if (page == range_page)
    {
        if (is_focus_delegated())
        {
            if (active_input == skip_count_input.get())
            {
                active_input = apply_count_input.get();
            }
            else
            {
                delegate_focus(false);
                active_input = nullptr;
            }
        }
        else
        {
            active_input = skip_count_input.get();
            delegate_focus(true);
        }
        dsp_comp_hub.dsp_selector->refresh_display();
    }
    else
    if (page == rsc_page)
    {
        switch_program_input(rsc_program_input);
    }
    else
    if (page == vlm_page)
    {
        switch_program_input(vlm_program_input);
    }
    else
    if (page == con_page)
    {
        switch_program_input(con_program_input);
    }
    else
    if (page == peer_vlm_page)
    {
        switch_program_input(peer_vlm_program_input);
    }
}

void MDspBulkActions::cursor_to_previous_item()
{
    const uint32_t page = get_page_nr();
    if (page == range_page)
    {
        if (is_focus_delegated())
        {
            if (active_input == apply_count_input.get())
            {
                active_input = skip_count_input.get();
            }
            else
            {
                delegate_focus(false);
                active_input = nullptr;
            }
        }
        else
        {
            active_input = apply_count_input.get();
            delegate_focus(true);
        }
        dsp_comp_hub.dsp_selector->refresh_display();
    }
    else
    if (page == rsc_page)
    {
        switch_program_input(rsc_program_input);
    }
    else
    if (page == vlm_page)
    {
        switch_program_input(vlm_program_input);
    }
    else
    if (page == con_page)
    {
        switch_program_input(con_program_input);
    }
    else
    if (page == peer_vlm_page)
    {
        switch_program_input(peer_vlm_program_input);
    }
}

void MDspBulkActions::setup_cmd_functions()
{
    cmd_fn_rsc_start =
        [this]() -> void
        {
            rsc_function action = rsc_function_for_action(&DrbdCommands::exec_start);
            execute_resource_actions(action);
        };
    cmd_fn_rsc_stop =
        [this]() -> void
        {
            rsc_function action = rsc_function_for_action(&DrbdCommands::exec_stop);
            execute_resource_actions(action);
        };
    cmd_fn_rsc_adjust =
        [this]() -> void
        {
            rsc_function action = rsc_function_for_action(&DrbdCommands::exec_adjust);
            execute_resource_actions(action);
        };
    cmd_fn_rsc_adjust_skip_disk =
        [this]() -> void
        {
            rsc_function action = rsc_function_for_action(&DrbdCommands::exec_adjust_skip_disk);
            execute_resource_actions(action);
        };
    cmd_fn_rsc_adjust_skip_net =
        [this]() -> void
        {
            rsc_function action = rsc_function_for_action(&DrbdCommands::exec_adjust_skip_net);
            execute_resource_actions(action);
        };
    cmd_fn_rsc_adjust_skip_disk_net =
        [this]() -> void
        {
            rsc_function action = rsc_function_for_action(&DrbdCommands::exec_adjust_skip_disk_net);
            execute_resource_actions(action);
        };
    cmd_fn_rsc_primary =
        [this]() -> void
        {
            rsc_function action = rsc_function_for_action(&DrbdCommands::exec_primary);
            execute_resource_actions(action);
        };
    cmd_fn_rsc_secondary =
        [this]() -> void
        {
            rsc_function action = rsc_function_for_action(&DrbdCommands::exec_secondary);
            execute_resource_actions(action);
        };
    cmd_fn_rsc_force_primary =
        [this]() -> void
        {
            rsc_function action = rsc_function_for_action(&DrbdCommands::exec_force_primary);
            execute_resource_actions(action);
        };
    cmd_fn_rsc_force_secondary =
        [this]() -> void
        {
            rsc_function action = rsc_function_for_action(&DrbdCommands::exec_force_secondary);
            execute_resource_actions(action);
        };

    cmd_fn_vlm_attach =
        [this]() -> void
        {
            vlm_function action = vlm_function_for_action(&DrbdCommands::exec_attach);
            execute_volume_actions(action);
        };
    cmd_fn_vlm_detach =
        [this]() -> void
        {
            vlm_function action = vlm_function_for_action(&DrbdCommands::exec_detach);
            execute_volume_actions(action);
        };
    cmd_fn_vlm_invalidate =
        [this]() -> void
        {
            vlm_function action = vlm_function_for_action(&DrbdCommands::exec_invalidate);
            execute_volume_actions(action);
        };

    cmd_fn_con_connect =
        [this]() -> void
        {
            con_function action = con_function_for_action(&DrbdCommands::exec_connect);
            execute_connection_actions(action);
        };
    cmd_fn_con_disconnect =
        [this]() -> void
        {
            con_function action = con_function_for_action(&DrbdCommands::exec_disconnect);
            execute_connection_actions(action);
        };
    cmd_fn_con_discard =
        [this]() -> void
        {
            con_function action = con_function_for_action(&DrbdCommands::exec_discard_connect);
            execute_connection_actions(action);
        };

    cmd_fn_peer_vlm_pause_sync =
        [this]() -> void
        {
            peer_vlm_function action = peer_vlm_function_for_action(&DrbdCommands::exec_pause_sync);
            execute_peer_volume_actions(action);
        };
    cmd_fn_peer_vlm_resume_sync =
        [this]() -> void
        {
            peer_vlm_function action = peer_vlm_function_for_action(&DrbdCommands::exec_resume_sync);
            execute_peer_volume_actions(action);
        };
    cmd_fn_peer_vlm_verify =
        [this]() -> void
        {
            peer_vlm_function action = peer_vlm_function_for_action(&DrbdCommands::exec_verify);
            execute_peer_volume_actions(action);
        };
    cmd_fn_peer_vlm_invalidate_remote =
        [this]() -> void
        {
            peer_vlm_function action = peer_vlm_function_for_action(&DrbdCommands::exec_invalidate_remote);
            execute_peer_volume_actions(action);
        };

    cmd_fn_toggle_keep_range =
        [this]() -> void
        {
            toggle_keep_range();
        };

    cmd_fn_rsc_program =
        [this]() -> void
        {
            exec_rsc_program();
        };

    cmd_fn_vlm_program =
        [this]() -> void
        {
            exec_vlm_program();
        };

    cmd_fn_con_program =
        [this]() -> void
        {
            exec_con_program();
        };

    cmd_fn_peer_vlm_program =
        [this]() -> void
        {
            exec_peer_vlm_program();
        };
}

void MDspBulkActions::setup_pages()
{
    ClickableCommand::Builder bld;

    bld.coords.page = 1;
    bld.coords.start_col = 5;
    bld.coords.end_col = 45;
    bld.coords.row = 5;

    bld.auto_nr = 1;
    rsc_page = bld.coords.page;

    cmd_rsc_start = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_rsc_start)
    );
    add_option(*cmd_rsc_start);
    cmd_rsc_stop = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_rsc_stop)
    );
    add_option(*cmd_rsc_stop);
    cmd_rsc_adjust = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_rsc_adjust)
    );
    add_option(*cmd_rsc_adjust);
    cmd_rsc_adjust_skip_disk = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_rsc_adjust_skip_disk)
    );
    add_option(*cmd_rsc_adjust_skip_disk);
    cmd_rsc_adjust_skip_net = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_rsc_adjust_skip_net)
    );
    add_option(*cmd_rsc_adjust_skip_net);
    cmd_rsc_adjust_skip_disk_net = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_rsc_adjust_skip_disk_net)
    );

    bld.coords.start_col = 50;
    bld.coords.end_col = 90;
    bld.coords.row = 5;

    add_option(*cmd_rsc_adjust_skip_disk_net);
    cmd_rsc_primary = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_rsc_primary)
    );
    add_option(*cmd_rsc_primary);
    cmd_rsc_secondary = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_rsc_secondary)
    );
    add_option(*cmd_rsc_secondary);
    cmd_rsc_force_primary = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_rsc_force_primary)
    );
    add_option(*cmd_rsc_force_primary);
    cmd_rsc_force_secondary = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_rsc_force_secondary)
    );
    add_option(*cmd_rsc_force_secondary);

    bld.coords.start_col = 5;
    bld.coords.end_col = 45;
    bld.coords.row = 15;
    cmd_rsc_program = std::unique_ptr<ClickableCommand>(
        bld.create_with_id("R", cmd_fn_rsc_program)
    );
    add_option(*cmd_rsc_program);

    ++bld.coords.page;
    bld.auto_nr = 1;
    bld.coords.row = 5;
    vlm_page = bld.coords.page;

    cmd_vlm_attach = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_vlm_attach)
    );
    add_option(*cmd_vlm_attach);
    cmd_vlm_detach = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_vlm_detach)
    );
    add_option(*cmd_vlm_detach);
    cmd_vlm_invalidate = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_vlm_invalidate)
    );
    add_option(*cmd_vlm_invalidate);

    bld.coords.row = 15;
    cmd_vlm_program = std::unique_ptr<ClickableCommand>(
        bld.create_with_id("V", cmd_fn_vlm_program)
    );
    add_option(*cmd_vlm_program);

    ++bld.coords.page;
    bld.auto_nr = 1;
    bld.coords.row = 5;
    con_page = bld.coords.page;

    cmd_con_connect = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_con_connect)
    );
    add_option(*cmd_con_connect);
    cmd_con_disconnect = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_con_disconnect)
    );
    add_option(*cmd_con_disconnect);
    cmd_con_discard = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_con_discard)
    );
    add_option(*cmd_con_discard);

    bld.coords.row = 15;
    cmd_con_program = std::unique_ptr<ClickableCommand>(
        bld.create_with_id("C", cmd_fn_con_program)
    );
    add_option(*cmd_con_program);

    ++bld.coords.page;
    bld.auto_nr = 1;
    bld.coords.row = 5;
    peer_vlm_page = bld.coords.page;

    cmd_peer_vlm_pause_sync = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_pause_sync)
    );
    add_option(*cmd_peer_vlm_pause_sync);
    cmd_peer_vlm_resume_sync = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_resume_sync)
    );
    add_option(*cmd_peer_vlm_resume_sync);
    cmd_peer_vlm_verify = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_verify)
    );
    add_option(*cmd_peer_vlm_verify);
    cmd_peer_vlm_invalidate_remote = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_peer_vlm_invalidate_remote)
    );
    add_option(*cmd_peer_vlm_invalidate_remote);

    bld.coords.row = 15;
    cmd_peer_vlm_program = std::unique_ptr<ClickableCommand>(
        bld.create_with_id("P", cmd_fn_peer_vlm_program)
    );
    add_option(*cmd_peer_vlm_program);

    // Range page
    ++bld.coords.page;
    bld.auto_nr = 1;
    range_page = bld.coords.page;

    cmd_toggle_keep_range = std::unique_ptr<ClickableCommand>(
        bld.create_with_page_dot_auto_nr(cmd_fn_toggle_keep_range)
    );
    add_option(*cmd_toggle_keep_range);

    set_page_count(bld.coords.page);
}

void MDspBulkActions::execute_resource_actions(rsc_function& action, bool attach_objects)
{
    dsp_comp_hub.dsp_common->application_working();

    std::function<void(MDspBulkActions::rsc_function&, bool&, MDspBulkActions::RangeSpec&,
                       uint32_t&, uint32_t&, bool)> action_loop =
        [this](
            rsc_function action_ref,
            bool& range_completed, RangeSpec& range, uint32_t& skip_ctr, uint32_t& apply_ctr,
            bool attach_objects_param
        ) -> void
        {
            action_loop_for_resources(action_ref, range_completed, range, skip_ctr, apply_ctr, attach_objects_param);
        };
    execute_for_range(action_loop, action, attach_objects);
}

void MDspBulkActions::execute_volume_actions(vlm_function& action, bool attach_objects)
{
    dsp_comp_hub.dsp_common->application_working();

    std::function<void(MDspBulkActions::vlm_function&, bool&, MDspBulkActions::RangeSpec&,
                       uint32_t&, uint32_t&, bool)> action_loop =
        [this](
            vlm_function action_ref,
            bool& range_completed, RangeSpec& range, uint32_t& skip_ctr, uint32_t& apply_ctr,
            bool attach_objects_param
        ) -> void
        {
            action_loop_for_volumes(action_ref, range_completed, range, skip_ctr, apply_ctr, attach_objects_param);
        };
    execute_for_range(action_loop, action, attach_objects);
}

void MDspBulkActions::execute_connection_actions(con_function& action, bool attach_objects)
{
    dsp_comp_hub.dsp_common->application_working();

    std::function<void(MDspBulkActions::con_function&, bool&, MDspBulkActions::RangeSpec&,
                       uint32_t&, uint32_t&, bool)> action_loop =
        [this](
            con_function action_ref,
            bool& range_completed, RangeSpec& range, uint32_t& skip_ctr, uint32_t& apply_ctr,
            bool attach_objects_param
        ) -> void
        {
            action_loop_for_connections(action_ref, range_completed, range, skip_ctr, apply_ctr, attach_objects_param);
        };
    execute_for_range(action_loop, action, attach_objects);
}

void MDspBulkActions::execute_peer_volume_actions(peer_vlm_function& action, bool attach_objects)
{
    dsp_comp_hub.dsp_common->application_working();

    std::function<void(MDspBulkActions::peer_vlm_function&, bool&, MDspBulkActions::RangeSpec&,
                       uint32_t&, uint32_t&, bool)> action_loop =
        [this](
            peer_vlm_function action_ref,
            bool& range_completed, RangeSpec& range, uint32_t& skip_ctr, uint32_t& apply_ctr,
            bool attach_objects_param
        ) -> void
        {
            action_loop_for_peer_volumes(
                action_ref,
                range_completed, range, skip_ctr, apply_ctr,
                attach_objects_param
            );
        };
    execute_for_range(action_loop, action, attach_objects);
}

// @throws SubProcessQueue::QueueCapacityException
void MDspBulkActions::action_loop_for_resources(
    rsc_function&                       action,
    bool&                               range_completed,
    RangeSpec&                          range,
    uint32_t&                           skip_ctr,
    uint32_t&                           apply_ctr,
    bool                                attach_objects
)
{
    ResourceSelectionMap::KeysIterator rsc_iter(*(dsp_comp_hub.dsp_shared->selected_resources));
    for (const std::string* rsc_name = rsc_iter.next();
         rsc_name != nullptr && (range.apply_count == 0 || apply_ctr < range.apply_count);
         rsc_name = rsc_iter.next())
    {
        DrbdResource* const rsc = attach_objects ? dsp_comp_hub.rsc_map->get(rsc_name) : nullptr;
        try
        {
            if (skip_ctr >= range.skip_count)
            {
                action(*rsc_name, rsc);
                ++apply_ctr;
            }
            else
            {
                ++skip_ctr;
            }
        }
        catch (SubProcessQueue::QueueCapacityException&)
        {
            // rethrow to outer try block to avoid catching the superclass
            // in the next catch statement
            throw;
        }
        catch (SubProcess::Exception&)
        {
            log_subprocess_error(*rsc_name);
        }
    }
}

// @throws SubProcessQueue::QueueCapacityException
void MDspBulkActions::action_loop_for_volumes(
    vlm_function&                       action,
    bool&                               range_completed,
    RangeSpec&                          range,
    uint32_t&                           skip_ctr,
    uint32_t&                           apply_ctr,
    bool                                attach_objects
)
{
    ResourceSelectionMap::NodesIterator rsc_iter(*(dsp_comp_hub.dsp_shared->selected_resources));
    for (ResourceSelectionMap::Node* rsc_node = rsc_iter.next();
         rsc_node != nullptr && (range.apply_count == 0 || apply_ctr < range.apply_count);
         rsc_node = rsc_iter.next())
    {
        const std::string* const rsc_name = rsc_node->get_key();
        DrbdResource* const rsc = attach_objects ? dsp_comp_hub.rsc_map->get(rsc_name) : nullptr;
        ResourceSubSelections& sub_selections = *(rsc_node->get_value());
        if (sub_selections.volume_selection)
        {
            VolumeSelectionMap::KeysIterator vlm_iter(*(sub_selections.volume_selection));
            for (const uint16_t* vlm_nr = vlm_iter.next();
                 vlm_nr != nullptr && (range.apply_count == 0 || apply_ctr < range.apply_count);
                 vlm_nr = vlm_iter.next())
            {
                DrbdVolume* vlm = nullptr;
                if (attach_objects && rsc != nullptr)
                {
                    vlm = rsc->get_volume(*vlm_nr);
                }
                try
                {
                    if (skip_ctr >= range.skip_count)
                    {
                        action(*rsc_name, rsc, *vlm_nr, vlm);
                        ++apply_ctr;
                    }
                    else
                    {
                        ++skip_ctr;
                    }
                }
                catch (SubProcessQueue::QueueCapacityException&)
                {
                    // rethrow to outer try block to avoid catching the superclass
                    // in the next catch statement
                    throw;
                }
                catch (SubProcess::Exception&)
                {
                    log_subprocess_error(*rsc_name);
                }
            }
        }
    }
}

// @throws SubProcessQueue::QueueCapacityException
void MDspBulkActions::action_loop_for_connections(
    con_function&                       action,
    bool&                               range_completed,
    RangeSpec&                          range,
    uint32_t&                           skip_ctr,
    uint32_t&                           apply_ctr,
    bool                                attach_objects
)
{
    ResourceSelectionMap::NodesIterator rsc_iter(*(dsp_comp_hub.dsp_shared->selected_resources));
    for (ResourceSelectionMap::Node* rsc_node = rsc_iter.next();
         rsc_node != nullptr && (range.apply_count == 0 || apply_ctr < range.apply_count);
         rsc_node = rsc_iter.next())
    {
        const std::string* const rsc_name = rsc_node->get_key();
        DrbdResource* const rsc = attach_objects ? dsp_comp_hub.rsc_map->get(rsc_name) : nullptr;
        ResourceSubSelections& sub_selections = *(rsc_node->get_value());
        if (sub_selections.connection_selection)
        {
            ConnectionSelectionMap::KeysIterator con_iter(*(sub_selections.connection_selection));
            for (const std::string* con_name = con_iter.next();
                 con_name != nullptr && (range.apply_count == 0 || apply_ctr < range.apply_count);
                 con_name = con_iter.next())
            {
                DrbdConnection* con = nullptr;
                if (attach_objects && rsc != nullptr)
                {
                    con = rsc->get_connection(*con_name);
                }
                try
                {
                    if (skip_ctr >= range.skip_count)
                    {
                        action(*rsc_name, rsc, *con_name, con);
                        ++apply_ctr;
                    }
                    else
                    {
                        ++skip_ctr;
                    }
                }
                catch (SubProcessQueue::QueueCapacityException&)
                {
                    // rethrow to outer try block to avoid catching the superclass
                    // in the next catch statement
                    throw;
                }
                catch (SubProcess::Exception&)
                {
                    log_subprocess_error(*rsc_name);
                }
            }
        }
    }
}

// @throws SubProcessQueue::QueueCapacityException
void MDspBulkActions::action_loop_for_peer_volumes(
    peer_vlm_function&                  action,
    bool&                               range_completed,
    RangeSpec&                          range,
    uint32_t&                           skip_ctr,
    uint32_t&                           apply_ctr,
    bool                                attach_objects
)
{
    ResourceSelectionMap::NodesIterator rsc_iter(*(dsp_comp_hub.dsp_shared->selected_resources));
    for (ResourceSelectionMap::Node* rsc_node = rsc_iter.next();
         rsc_node != nullptr && (range.apply_count == 0 || apply_ctr < range.apply_count);
         rsc_node = rsc_iter.next())
    {
        const std::string* const rsc_name = rsc_node->get_key();
        DrbdResource* const rsc = attach_objects ? dsp_comp_hub.rsc_map->get(rsc_name) : nullptr;
        ResourceSubSelections& sub_selections = *(rsc_node->get_value());
        if (sub_selections.connection_selection)
        {
            ConnectionSelectionMap::NodesIterator con_iter(*(sub_selections.connection_selection));
            for (ConnectionSelectionMap::Node* con_node = con_iter.next();
                 con_node != nullptr && (range.apply_count == 0 || apply_ctr < range.apply_count);
                 con_node = con_iter.next())
            {
                const std::string* const con_name = con_node->get_key();
                DrbdConnection* con = nullptr;
                if (attach_objects && rsc != nullptr)
                {
                    con = rsc->get_connection(*con_name);
                }
                VolumeSelectionMap* const selected_peer_volumes = con_node->get_value();
                if (selected_peer_volumes != nullptr)
                {
                    VolumeSelectionMap::KeysIterator peer_vlm_iter(*selected_peer_volumes);
                    for (const uint16_t* peer_vlm_nr = peer_vlm_iter.next();
                         peer_vlm_nr != nullptr && (range.apply_count == 0 || apply_ctr < range.apply_count);
                         peer_vlm_nr = peer_vlm_iter.next())
                    {
                        DrbdVolume* peer_vlm = nullptr;
                        if (attach_objects && con != nullptr)
                        {
                            peer_vlm = con->get_volume(*peer_vlm_nr);
                        }
                        try
                        {
                            if (skip_ctr >= range.skip_count)
                            {
                                action(*rsc_name, rsc, *con_name, con, *peer_vlm_nr, peer_vlm);
                                ++apply_ctr;
                            }
                            else
                            {
                                ++skip_ctr;
                            }
                        }
                        catch (SubProcessQueue::QueueCapacityException&)
                        {
                            // rethrow to outer try block to avoid catching the superclass
                            // in the next catch statement
                            throw;
                        }
                        catch (SubProcess::Exception&)
                        {
                            log_subprocess_error(*rsc_name);
                        }
                    }
                }
            }
        }
    }
}

// @throws NumberFormatExecption
MDspBulkActions::RangeSpec MDspBulkActions::get_exec_range()
{
    RangeSpec range;
    try
    {
        const std::string& skip_count_text = skip_count_input->get_text();
        if (!skip_count_text.empty())
        {
            range.skip_count = dsaext::parse_unsigned_int32(skip_count_text);
        }
    }
    catch (dsaext::NumberFormatException&)
    {
        range_error_msg = "Unparsable skip count";
        throw;
    }

    try
    {
        const std::string& apply_count_text = apply_count_input->get_text();
        if (!apply_count_text.empty())
        {
            range.apply_count = dsaext::parse_unsigned_int32(apply_count_text);
        }
    }
    catch (dsaext::NumberFormatException&)
    {
        range_error_msg = "Unparsable process count";
        throw;
    }
    return range;
}

void MDspBulkActions::log_subprocess_error(const std::string& rsc_name)
{
    std::string error_msg("Bulk actions: Resource ");
    error_msg += rsc_name;
    error_msg += ": Command failed: Sub-process execution error";
    dsp_comp_hub.log->add_entry(
        MessageLog::log_level::ALERT,
        error_msg
    );
}

void MDspBulkActions::log_insufficient_qcap_error()
{
    dsp_comp_hub.log->add_entry(
        MessageLog::log_level::ALERT,
        "Bulk actions: Cannot execute command, insufficient queue capacity"
    );
}

void MDspBulkActions::switch_program_input(const std::unique_ptr<InputField>& program_input)
{
    if (is_focus_delegated())
    {
        delegate_focus(false);
        active_input = nullptr;
    }
    else
    {
        active_input = program_input.get();
        delegate_focus(true);
    }
    dsp_comp_hub.dsp_selector->refresh_display();
}

bool MDspBulkActions::mouse_action_program_input(
    const std::unique_ptr<InputField>& program_input,
    MouseEvent& mouse
)
{
    bool intercepted = false;
    if (program_input->mouse_action(mouse))
    {
        active_input = program_input.get();
        delegate_focus(true);
        intercepted = true;
    }
    else
    {
        InputField& option_field = get_option_field();
        if (option_field.mouse_action(mouse))
        {
            active_input = nullptr;
            delegate_focus(false);
            intercepted = true;
        }
    }
    dsp_comp_hub.dsp_selector->refresh_display();
    return intercepted;
}

MDspBulkActions::rsc_function MDspBulkActions::rsc_function_for_action(DrbdCommands::resource_action_fn action)
{
    return [this, action](const std::string& rsc_name, const DrbdResource* const rsc) -> void
    {
        (dsp_comp_hub.drbd_cmd_exec->*action)(rsc_name);
    };
}

MDspBulkActions::vlm_function MDspBulkActions::vlm_function_for_action(DrbdCommands::volume_action_fn action)
{
    return [this, action](const std::string& rsc_name, const DrbdResource* const rsc,
                          const uint16_t vlm_nr, const DrbdVolume* const vlm) -> void
    {
        (dsp_comp_hub.drbd_cmd_exec->*action)(rsc_name, vlm_nr);
    };
}

MDspBulkActions::con_function MDspBulkActions::con_function_for_action(DrbdCommands::connection_action_fn action)
{
    return [this, action](const std::string& rsc_name, const DrbdResource* const rsc,
                          const std::string& con_name, const DrbdConnection* const con) -> void
    {
        (dsp_comp_hub.drbd_cmd_exec->*action)(rsc_name, con_name);
    };
}

MDspBulkActions::peer_vlm_function MDspBulkActions::peer_vlm_function_for_action(
    DrbdCommands::peer_volume_action_fn action
)
{
    return [this, action](
        const std::string& rsc_name,
        const DrbdResource* const rsc,
        const std::string& con_name,
        const DrbdConnection* const con,
        const uint16_t vlm_nr,
        const DrbdVolume* const vlm
    ) -> void
    {
        (dsp_comp_hub.drbd_cmd_exec->*action)(rsc_name, con_name, vlm_nr);
    };
}

void MDspBulkActions::exec_rsc_program()
{
    const std::string& program = rsc_program_input->get_text();

    if (program.length() >= 1)
    {
        rsc_function action =
            [this, program](const std::string& rsc_name, const DrbdResource* const rsc) -> void
            {
                dsp_comp_hub.drbd_cmd_exec->exec_resource_program(program, rsc_name, rsc);
            };
        execute_resource_actions(action, true);
    }
}

void MDspBulkActions::exec_vlm_program()
{
    const std::string& program = vlm_program_input->get_text();

    if (program.length() >= 1)
    {
        vlm_function action =
            [this, program](const std::string& rsc_name, const DrbdResource* const rsc,
                            const uint16_t vlm_nr, const DrbdVolume* vlm) -> void
            {
                dsp_comp_hub.drbd_cmd_exec->exec_volume_program(program, rsc_name, rsc, vlm_nr, vlm);
            };
        execute_volume_actions(action, true);
    }
}

void MDspBulkActions::exec_con_program()
{
    const std::string& program = con_program_input->get_text();

    if (program.length() >= 1)
    {
        con_function action =
            [this, program](const std::string& rsc_name, const DrbdResource* const rsc,
                            const std::string& con_name, const DrbdConnection* const con) -> void
            {
                dsp_comp_hub.drbd_cmd_exec->exec_connection_program(program, rsc_name, rsc, con_name, con);
            };
        execute_connection_actions(action, true);
    }
}

void MDspBulkActions::exec_peer_vlm_program()
{
    const std::string& program = peer_vlm_program_input->get_text();

    if (program.length() >= 1)
    {
        peer_vlm_function action =
            [this, program](
                const std::string& rsc_name,
                const DrbdResource* const rsc,
                const std::string& con_name,
                const DrbdConnection* const con,
                const uint16_t vlm_nr,
                const DrbdVolume* const vlm
            ) -> void
            {
                dsp_comp_hub.drbd_cmd_exec->exec_peer_volume_program(
                    program,
                    rsc_name, rsc,
                    con_name, con,
                    vlm_nr, vlm
                );
            };
        execute_peer_volume_actions(action, true);
    }
}
