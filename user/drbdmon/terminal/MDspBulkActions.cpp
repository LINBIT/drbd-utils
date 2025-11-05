#include <terminal/MDspBulkActions.h>
#include <terminal/SharedData.h>
#include <terminal/DrbdCommandsImpl.h>

MDspBulkActions::MDspBulkActions(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub)
{
    setup_cmd_functions();
    setup_pages();
}

MDspBulkActions::~MDspBulkActions() noexcept
{
}

void MDspBulkActions::display_closed()
{
    reset_display();
    set_page_nr(1);
    MDspMenuBase::display_closed();
}

void MDspBulkActions::display_content()
{
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
    if (page == 5)
    {
        display_range_options();
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
}

uint64_t MDspBulkActions::get_update_mask() noexcept
{
    return 0;
}

bool MDspBulkActions::key_pressed(const uint32_t key)
{
    return MDspMenuBase::key_pressed(key);
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

    set_page_count(4);
}

void MDspBulkActions::execute_resource_actions(DrbdCommands::resource_action_fn action)
{
    dsp_comp_hub.dsp_common->application_working();

    ResourceSelectionMap::KeysIterator rsc_iter(*(dsp_comp_hub.dsp_shared->selected_resources));
    for (const std::string* rsc_name = rsc_iter.next();
         rsc_name != nullptr;
         rsc_name = rsc_iter.next())
    {
        (dsp_comp_hub.drbd_cmd_exec->*action)(*rsc_name);
    }

    dsp_comp_hub.dsp_selector->leave_display();
}

void MDspBulkActions::execute_volume_actions(DrbdCommands::volume_action_fn action)
{
    dsp_comp_hub.dsp_common->application_working();

    ResourceSelectionMap::NodesIterator rsc_iter(*(dsp_comp_hub.dsp_shared->selected_resources));
    for (ResourceSelectionMap::Node* rsc_node = rsc_iter.next();
         rsc_node != nullptr;
         rsc_node = rsc_iter.next())
    {
        const std::string* const rsc_name = rsc_node->get_key();
        ResourceSubSelections& sub_selections = *(rsc_node->get_value());
        if (sub_selections.volume_selection)
        {
            VolumeSelectionMap::KeysIterator vlm_iter(*(sub_selections.volume_selection));
            for (const uint16_t* vlm_nr = vlm_iter.next();
                 vlm_nr != nullptr;
                 vlm_nr = vlm_iter.next())
            {
                (dsp_comp_hub.drbd_cmd_exec->*action)(*rsc_name, *vlm_nr);
            }
        }
    }

    dsp_comp_hub.dsp_selector->leave_display();
}

void MDspBulkActions::execute_connection_actions(DrbdCommands::connection_action_fn action)
{
    dsp_comp_hub.dsp_common->application_working();

    ResourceSelectionMap::NodesIterator rsc_iter(*(dsp_comp_hub.dsp_shared->selected_resources));
    for (ResourceSelectionMap::Node* rsc_node = rsc_iter.next();
         rsc_node != nullptr;
         rsc_node = rsc_iter.next())
    {
        const std::string* const rsc_name = rsc_node->get_key();
        ResourceSubSelections& sub_selections = *(rsc_node->get_value());
        if (sub_selections.connection_selection)
        {
            ConnectionSelectionMap::KeysIterator con_iter(*(sub_selections.connection_selection));
            for (const std::string* con_name = con_iter.next();
                 con_name != nullptr;
                 con_name = con_iter.next())
            {
                (dsp_comp_hub.drbd_cmd_exec->*action)(*rsc_name, *con_name);
            }
        }
    }

    dsp_comp_hub.dsp_selector->leave_display();
}

void MDspBulkActions::execute_peer_volume_actions(DrbdCommands::peer_volume_action_fn action)
{
    dsp_comp_hub.dsp_common->application_working();

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
                        (dsp_comp_hub.drbd_cmd_exec->*action)(*rsc_name, *con_name, *peer_vlm_nr);
                    }
                }
            }
        }
    }

    dsp_comp_hub.dsp_selector->leave_display();
}
