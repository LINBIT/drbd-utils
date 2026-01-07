#include <terminal/MDspBulkActions.h>
#include <terminal/SharedData.h>
#include <terminal/DrbdCommandsImpl.h>
#include <cppdsaext/src/integerparse.h>

MDspBulkActions::MDspBulkActions(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub)
{
    skip_count_input = std::unique_ptr<InputField>(new InputField(dsp_comp_hub, 5, 5, 8, 8));
    apply_count_input = std::unique_ptr<InputField>(new InputField(dsp_comp_hub, 5, 8, 8, 8));

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
    if (!keep_range)
    {
        skip_count_input->clear_text();
        apply_count_input->clear_text();
        range_info_msg.clear();
        range_error_msg.clear();
    }
    MDspMenuBase::display_closed();
}

void MDspBulkActions::display_content()
{
    const uint32_t page = get_page_nr();
    if (page != range_page)
    {
        delegate_focus(false);
        active_input = nullptr;

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

    if (dsp_comp_hub.enable_drbd_actions)
    {
        display_actions();
    }
    else
    {
        dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_BULK_ACT);

        dsp_comp_hub.dsp_io->cursor_xy(1, DisplayConsts::PAGE_NAV_Y + 3);
        dsp_comp_hub.dsp_io->write_text("This page is currently disabled");
    }
}

void MDspBulkActions::display_actions()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_BULK_ACT);

    const uint32_t page = get_page_nr();
    if (page == 1)
    {
        display_resource_actions();
    }
    else
    if (page == 2)
    {
        display_volume_actions();
    }
    else
    if (page == 3)
    {
        display_connection_actions();
    }
    else
    if (page == 4)
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
    bool intercepted = MDspMenuBase::key_pressed(key);
    if (!intercepted)
    {
        const uint32_t page = get_page_nr();
        if (page == range_page)
        {
            if (is_focus_delegated() && active_input != nullptr)
            {
                active_input->key_pressed(key);
                intercepted = true;
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
}

void MDspBulkActions::setup_cmd_functions()
{
    cmd_fn_rsc_start =
        [this]() -> void
        {
            execute_resource_actions(&DrbdCommands::exec_start);
        };
    cmd_fn_rsc_stop =
        [this]() -> void
        {
            execute_resource_actions(&DrbdCommands::exec_stop);
        };
    cmd_fn_rsc_adjust =
        [this]() -> void
        {
            execute_resource_actions(&DrbdCommands::exec_adjust);
        };
    cmd_fn_rsc_adjust_skip_disk =
        [this]() -> void
        {
            execute_resource_actions(&DrbdCommands::exec_adjust_skip_disk);
        };
    cmd_fn_rsc_adjust_skip_net =
        [this]() -> void
        {
            execute_resource_actions(&DrbdCommands::exec_adjust_skip_net);
        };
    cmd_fn_rsc_adjust_skip_disk_net =
        [this]() -> void
        {
            execute_resource_actions(&DrbdCommands::exec_adjust_skip_disk_net);
        };
    cmd_fn_rsc_primary =
        [this]() -> void
        {
            execute_resource_actions(&DrbdCommands::exec_primary);
        };
    cmd_fn_rsc_secondary =
        [this]() -> void
        {
            execute_resource_actions(&DrbdCommands::exec_secondary);
        };
    cmd_fn_rsc_force_primary =
        [this]() -> void
        {
            execute_resource_actions(&DrbdCommands::exec_force_primary);
        };
    cmd_fn_rsc_force_secondary =
        [this]() -> void
        {
            execute_resource_actions(&DrbdCommands::exec_force_secondary);
        };

    cmd_fn_vlm_attach =
        [this]() -> void
        {
            execute_volume_actions(&DrbdCommands::exec_attach);
        };
    cmd_fn_vlm_detach =
        [this]() -> void
        {
            execute_volume_actions(&DrbdCommands::exec_detach);
        };
    cmd_fn_vlm_invalidate =
        [this]() -> void
        {
            execute_volume_actions(&DrbdCommands::exec_invalidate);
        };

    cmd_fn_con_connect =
        [this]() -> void
        {
            execute_connection_actions(&DrbdCommands::exec_connect);
        };
    cmd_fn_con_disconnect =
        [this]() -> void
        {
            execute_connection_actions(&DrbdCommands::exec_disconnect);
        };
    cmd_fn_con_discard =
        [this]() -> void
        {
            execute_connection_actions(&DrbdCommands::exec_discard_connect);
        };

    cmd_fn_peer_vlm_pause_sync =
        [this]() -> void
        {
            execute_peer_volume_actions(&DrbdCommands::exec_pause_sync);
        };
    cmd_fn_peer_vlm_resume_sync =
        [this]() -> void
        {
            execute_peer_volume_actions(&DrbdCommands::exec_resume_sync);
        };
    cmd_fn_peer_vlm_verify =
        [this]() -> void
        {
            execute_peer_volume_actions(&DrbdCommands::exec_verify);
        };
    cmd_fn_peer_vlm_invalidate_remote =
        [this]() -> void
        {
            execute_peer_volume_actions(&DrbdCommands::exec_invalidate_remote);
        };

    cmd_fn_toggle_keep_range =
        [this]() -> void
        {
            toggle_keep_range();
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

    ++bld.coords.page;
    bld.auto_nr = 1;
    bld.coords.row = 5;

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

    ++bld.coords.page;
    bld.auto_nr = 1;
    bld.coords.row = 5;

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

    ++bld.coords.page;
    bld.auto_nr = 1;
    bld.coords.row = 5;

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

void MDspBulkActions::execute_resource_actions(DrbdCommands::resource_action_fn action)
{
    dsp_comp_hub.dsp_common->application_working();

    std::function<void(DrbdCommands::resource_action_fn, bool&, MDspBulkActions::RangeSpec&,
                       uint32_t&, uint32_t&)> action_loop =
        [this](
            DrbdCommands::resource_action_fn action_ref,
            bool& range_completed, RangeSpec& range, uint32_t& skip_ctr, uint32_t& apply_ctr
        ) -> void
        {
            action_loop_for_resources(action_ref, range_completed, range, skip_ctr, apply_ctr);
        };
    execute_for_range(action_loop, action);
}

void MDspBulkActions::execute_volume_actions(DrbdCommands::volume_action_fn action)
{
    dsp_comp_hub.dsp_common->application_working();

    std::function<void(DrbdCommands::volume_action_fn, bool&, MDspBulkActions::RangeSpec&,
                       uint32_t&, uint32_t&)> action_loop =
        [this](
            DrbdCommands::volume_action_fn action_ref,
            bool& range_completed, RangeSpec& range, uint32_t& skip_ctr, uint32_t& apply_ctr
        ) -> void
        {
            action_loop_for_volumes(action_ref, range_completed, range, skip_ctr, apply_ctr);
        };
    execute_for_range(action_loop, action);
}

void MDspBulkActions::execute_connection_actions(DrbdCommands::connection_action_fn action)
{
    dsp_comp_hub.dsp_common->application_working();

    std::function<void(DrbdCommands::connection_action_fn, bool&, MDspBulkActions::RangeSpec&,
                       uint32_t&, uint32_t&)> action_loop =
        [this](
            DrbdCommands::connection_action_fn action_ref,
            bool& range_completed, RangeSpec& range, uint32_t& skip_ctr, uint32_t& apply_ctr
        ) -> void
        {
            action_loop_for_connections(action_ref, range_completed, range, skip_ctr, apply_ctr);
        };
    execute_for_range(action_loop, action);
}

void MDspBulkActions::execute_peer_volume_actions(DrbdCommands::peer_volume_action_fn action)
{
    dsp_comp_hub.dsp_common->application_working();

    std::function<void(DrbdCommands::peer_volume_action_fn, bool&, MDspBulkActions::RangeSpec&,
                       uint32_t&, uint32_t&)> action_loop =
        [this](
            DrbdCommands::peer_volume_action_fn action_ref,
            bool& range_completed, RangeSpec& range, uint32_t& skip_ctr, uint32_t& apply_ctr
        ) -> void
        {
            action_loop_for_peer_volumes(action_ref, range_completed, range, skip_ctr, apply_ctr);
        };
    execute_for_range(action_loop, action);
}

// @throws SubProcessQueue::QueueCapacityException
void MDspBulkActions::action_loop_for_resources(
    DrbdCommands::resource_action_fn    action,
    bool&                               range_completed,
    RangeSpec&                          range,
    uint32_t&                           skip_ctr,
    uint32_t&                           apply_ctr
)
{
    ResourceSelectionMap::KeysIterator rsc_iter(*(dsp_comp_hub.dsp_shared->selected_resources));
    for (const std::string* rsc_name = rsc_iter.next();
         rsc_name != nullptr && (range.apply_count == 0 || apply_ctr < range.apply_count);
         rsc_name = rsc_iter.next())
    {
        try
        {
            if (skip_ctr >= range.skip_count)
            {
                (dsp_comp_hub.drbd_cmd_exec->*action)(*rsc_name);
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
    DrbdCommands::volume_action_fn      action,
    bool&                               range_completed,
    RangeSpec&                          range,
    uint32_t&                           skip_ctr,
    uint32_t&                           apply_ctr
)
{
    ResourceSelectionMap::NodesIterator rsc_iter(*(dsp_comp_hub.dsp_shared->selected_resources));
    for (ResourceSelectionMap::Node* rsc_node = rsc_iter.next();
         rsc_node != nullptr && (range.apply_count == 0 || apply_ctr < range.apply_count);
         rsc_node = rsc_iter.next())
    {
        const std::string* const rsc_name = rsc_node->get_key();
        ResourceSubSelections& sub_selections = *(rsc_node->get_value());
        if (sub_selections.volume_selection)
        {
            VolumeSelectionMap::KeysIterator vlm_iter(*(sub_selections.volume_selection));
            for (const uint16_t* vlm_nr = vlm_iter.next();
                 vlm_nr != nullptr && (range.apply_count == 0 || apply_ctr < range.apply_count);
                 vlm_nr = vlm_iter.next())
            {
                try
                {
                    if (skip_ctr >= range.skip_count)
                    {
                        (dsp_comp_hub.drbd_cmd_exec->*action)(*rsc_name, *vlm_nr);
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
    DrbdCommands::connection_action_fn  action,
    bool&                               range_completed,
    RangeSpec&                          range,
    uint32_t&                           skip_ctr,
    uint32_t&                           apply_ctr
)
{
    ResourceSelectionMap::NodesIterator rsc_iter(*(dsp_comp_hub.dsp_shared->selected_resources));
    for (ResourceSelectionMap::Node* rsc_node = rsc_iter.next();
         rsc_node != nullptr && (range.apply_count == 0 || apply_ctr < range.apply_count);
         rsc_node = rsc_iter.next())
    {
        const std::string* const rsc_name = rsc_node->get_key();
        ResourceSubSelections& sub_selections = *(rsc_node->get_value());
        if (sub_selections.connection_selection)
        {
            ConnectionSelectionMap::KeysIterator con_iter(*(sub_selections.connection_selection));
            for (const std::string* con_name = con_iter.next();
                 con_name != nullptr && (range.apply_count == 0 || apply_ctr < range.apply_count);
                 con_name = con_iter.next())
            {
                try
                {
                    if (skip_ctr >= range.skip_count)
                    {
                        (dsp_comp_hub.drbd_cmd_exec->*action)(*rsc_name, *con_name);
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
    DrbdCommands::peer_volume_action_fn action,
    bool&                               range_completed,
    RangeSpec&                          range,
    uint32_t&                           skip_ctr,
    uint32_t&                           apply_ctr
)
{
    ResourceSelectionMap::NodesIterator rsc_iter(*(dsp_comp_hub.dsp_shared->selected_resources));
    for (ResourceSelectionMap::Node* rsc_node = rsc_iter.next();
         rsc_node != nullptr;
         rsc_node = rsc_iter.next())
    {
        const std::string* const rsc_name = rsc_node->get_key();
        ResourceSubSelections& sub_selections = *(rsc_node->get_value());
        if (sub_selections.connection_selection)
        {
            ConnectionSelectionMap::NodesIterator con_iter(*(sub_selections.connection_selection));
            for (ConnectionSelectionMap::Node* con_node = con_iter.next();
                 con_node != nullptr;
                 con_node = con_iter.next())
            {
                const std::string* const con_name = con_node->get_key();
                VolumeSelectionMap* const selected_peer_volumes = con_node->get_value();
                if (selected_peer_volumes != nullptr)
                {
                    VolumeSelectionMap::KeysIterator peer_vlm_iter(*selected_peer_volumes);
                    for (const uint16_t* peer_vlm_nr = peer_vlm_iter.next();
                         peer_vlm_nr != nullptr;
                         peer_vlm_nr = peer_vlm_iter.next())
                    {
                        try
                        {
                            if (skip_ctr >= range.skip_count)
                            {
                                (dsp_comp_hub.drbd_cmd_exec->*action)(*rsc_name, *con_name, *peer_vlm_nr);
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
