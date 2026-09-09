#include <terminal/MDspExportSelection.h>
#include <terminal/HelpText.h>
#include <terminal/KeyCodes.h>
#include <iostream>
#include <fstream>

MDspExportSelection::MDspExportSelection(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub)
{
    path_input = std::unique_ptr<InputField>(
        new InputField(comp_hub, 5, 13, 400, 80)
    );

    cmd_fn_toggle_exp_rsc =
        [this]() -> void
        {
            exp_rsc = !exp_rsc;
            dsp_comp_hub.dsp_selector->refresh_display();
        };
    cmd_fn_toggle_exp_vlm =
        [this]() -> void
        {
            exp_vlm = !exp_vlm;
            dsp_comp_hub.dsp_selector->refresh_display();
        };
    cmd_fn_toggle_exp_con =
        [this]() -> void
        {
            exp_con = !exp_con;
            dsp_comp_hub.dsp_selector->refresh_display();
        };
    cmd_fn_toggle_exp_peer_vlm =
        [this]() -> void
        {
            exp_peer_vlm = !exp_peer_vlm;
            dsp_comp_hub.dsp_selector->refresh_display();
        };
    cmd_fn_export =
        [this]() -> void
        {
            export_selection();
        };

    ClickableCommand::Builder bld;

    bld.coords.start_col = 5;
    bld.coords.end_col = 45;
    bld.coords.row = 5;
    bld.auto_nr = 1;

    cmd_toggle_exp_rsc = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_toggle_exp_rsc)
    );
    add_option(*cmd_toggle_exp_rsc);
    cmd_toggle_exp_vlm = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_toggle_exp_vlm)
    );
    add_option(*cmd_toggle_exp_vlm);
    cmd_toggle_exp_con = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_toggle_exp_con)
    );
    add_option(*cmd_toggle_exp_con);
    cmd_toggle_exp_peer_vlm = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_toggle_exp_peer_vlm)
    );
    add_option(*cmd_toggle_exp_peer_vlm);

    ++bld.coords.row;
    cmd_export = std::unique_ptr<ClickableCommand>(
        bld.create_with_auto_nr(cmd_fn_export)
    );
    add_option(*cmd_export);
}

MDspExportSelection::~MDspExportSelection() noexcept
{
}

void MDspExportSelection::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_EXPORT_SLCT);

    dsp_comp_hub.dsp_io->cursor_xy(3, 4);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
    dsp_comp_hub.dsp_io->write_text("Export selection:");
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

    display_selectable(5, "Resources", *cmd_toggle_exp_rsc, exp_rsc);
    display_selectable(5, "Volumes", *cmd_toggle_exp_vlm, exp_vlm);
    display_selectable(5, "Connections", *cmd_toggle_exp_con, exp_con);
    display_selectable(5, "Peer volumes", *cmd_toggle_exp_peer_vlm, exp_peer_vlm);

    display_option(5, "Export to file", *cmd_export, dsp_comp_hub.active_color_table->option_text);

    dsp_comp_hub.dsp_io->cursor_xy(3, 12);
    dsp_comp_hub.dsp_io->write_text("Export to file:");
    path_input->display();

    if (!message.empty())
    {
        dsp_comp_hub.dsp_io->cursor_xy(5, 15);
        dsp_comp_hub.dsp_io->write_text(message.c_str());
    }

    display_option_query(5, 17);
}

uint64_t MDspExportSelection::get_update_mask() noexcept
{
    return 0;
}

bool MDspExportSelection::key_pressed(const uint32_t key)
{
    bool intercepted = false;
    if (key == KeyCodes::FUNC_01)
    {
        helptext::open_help_page(helptext::id_type::EXPS_HELP, dsp_comp_hub);
        intercepted = true;
    }
    else
    {
        const bool delegated = is_focus_delegated();
        if (delegated && key == static_cast<uint32_t> ('/'))
        {
            path_input->key_pressed(key);
            intercepted = true;
        }
        else
        {
            intercepted = MDspMenuBase::key_pressed(key);
        }
        if (delegated && !intercepted)
        {
            path_input->key_pressed(key);
            intercepted = true;
        }
    }
    return intercepted;
}

bool MDspExportSelection::mouse_action(MouseEvent& mouse)
{
    bool intercepted = MDspMenuBase::mouse_action(mouse);
    if (!intercepted)
    {
        intercepted = path_input->mouse_action(mouse);
        if (intercepted)
        {
            delegate_focus(true);
        }
        else
        {
            InputField& option_field = get_option_field();
            intercepted = option_field.mouse_action(mouse);
            if (intercepted)
            {
                delegate_focus(false);
            }
        }

        if (intercepted)
        {
            dsp_comp_hub.dsp_selector->refresh_display();
        }
    }
    return intercepted;
}

void MDspExportSelection::text_cursor_ops()
{
    if (is_focus_delegated())
    {
        path_input->cursor();
    }
    else
    {
        MDspMenuBase::text_cursor_ops();
    }
}

void MDspExportSelection::display_activated()
{
    delegate_focus(false);
}

void MDspExportSelection::display_deactivated()
{
    message.clear();
}

void MDspExportSelection::display_closed()
{
    path_input->clear_text();
    message.clear();

    exp_rsc = true;
    exp_vlm = false;
    exp_con = false;
    exp_peer_vlm = false;
}

void MDspExportSelection::cursor_to_previous_item()
{
    const bool state = is_focus_delegated();
    delegate_focus(!state);
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspExportSelection::cursor_to_next_item()
{
    const bool state = is_focus_delegated();
    delegate_focus(!state);
    dsp_comp_hub.dsp_selector->refresh_display();
}

void MDspExportSelection::export_selection()
{
    dsp_comp_hub.dsp_common->application_working();

    const std::string& filename = path_input->get_text();
    if (!filename.empty() && (exp_rsc || exp_vlm || exp_con || exp_peer_vlm))
    {
        std::fstream file_out(filename.c_str(), std::ios_base::out);
        if (file_out.is_open())
        {
            ResourceSelectionMap::NodesIterator rsc_iter(
                *(dsp_comp_hub.dsp_shared->selected_resources)
            );
            for (ResourceSelectionMap::Node* rsc_node = rsc_iter.next();
                 rsc_node != nullptr && file_out.good();
                 rsc_node = rsc_iter.next())
            {
                const std::string& rsc_name = *(rsc_node->get_key());
                ResourceSubSelections& sub_selections = *(rsc_node->get_value());

                if (exp_rsc)
                {
                    file_out << rsc_name << '\n';
                }

                if (exp_vlm && sub_selections.volume_selection)
                {
                    VolumeSelectionMap::KeysIterator vlm_iter(*(sub_selections.volume_selection));
                    for (const uint16_t* vlm_nr = vlm_iter.next();
                         vlm_nr != nullptr && file_out.good();
                         vlm_nr = vlm_iter.next())
                    {
                        file_out << rsc_name << '/' << static_cast<unsigned int> (*vlm_nr) << '\n';
                    }
                }

                if ((exp_con || exp_peer_vlm) && sub_selections.connection_selection)
                {
                    ConnectionSelectionMap::NodesIterator con_iter(*(sub_selections.connection_selection));
                    for (ConnectionSelectionMap::Node* con_node = con_iter.next();
                         con_node != nullptr && file_out.good();
                         con_node = con_iter.next())
                    {
                        const std::string& con_name = *(con_node->get_key());

                        if (exp_con)
                        {
                            file_out << rsc_name << ':' << con_name << '\n';
                        }

                        if (exp_peer_vlm)
                        {
                            VolumeSelectionMap* const selected_peer_volumes = con_node->get_value();
                            if (selected_peer_volumes != nullptr)
                            {
                                VolumeSelectionMap::KeysIterator peer_vlm_iter(*selected_peer_volumes);
                                for (const uint16_t* peer_vlm_nr = peer_vlm_iter.next();
                                     peer_vlm_nr != nullptr && file_out.good();
                                     peer_vlm_nr = peer_vlm_iter.next())
                                {
                                    file_out << rsc_name << ':' << con_name << '/' <<
                                            static_cast<unsigned int> (*peer_vlm_nr) << '\n';
                                }
                            }
                        }
                    }
                }
            }

            if (file_out.good())
            {
                message = "Export complete.";
            }
            else
            {
                message = "Export failed: I/O error";
            }

            file_out.close();
        }
        else
        {
            message = "Export failed: Cannot open output file";
        }
    }
    else
    {
        if (filename.empty())
        {
            message = "Enter export file path";
        }
        else
        if (!(exp_rsc || exp_vlm || exp_con || exp_peer_vlm))
        {
            message = "Select objects to export";
        }
    }

    dsp_comp_hub.dsp_selector->refresh_display();
}
