#include <terminal/MDspOverview.h>
#include <string_transformations.h>
#include <terminal/selection_filter.h>
#include <terminal/KeyCodes.h>
#include <terminal/HelpText.h>

MDspOverview::MDspOverview(const ComponentsHub& comp_hub):
    MDspMenuBase::MDspMenuBase(comp_hub)
{
    cmd_fn_refresh_analysis =
        [this]() -> void
        {
            analyze_drbd_state();
            dsp_comp_hub.dsp_selector->refresh_display();
        };

    cmd_refresh_analysis = std::unique_ptr<ClickableCommand>(
        new ClickableCommand("R", 1, 1, 1, 20, cmd_fn_refresh_analysis)
    );
    // No add_option for this command; it's handled in key_pressed and mouse_action to be
    // active on all pages and in display_content to be repositioned above the command line
    // on terminal size changes

    fn_dsp_vlm_disk =
        [this](const DrbdVolume::disk_state state, const uint32_t counter, uint16_t& dsp_line) -> void
        {
            if (counter > 0)
            {
                const uint32_t rsc_counter = get_count(*vlm_disk_states_per_rsc, state);

                const std::string state_label(DrbdVolume::label_for_disk_state(state));
                if (state == DrbdVolume::disk_state::UP_TO_DATE ||
                    (state == DrbdVolume::disk_state::DISKLESS && counter == statistics.vlm_client_count))
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(5, dsp_line);
                }
                else
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(3, dsp_line);
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
                    dsp_comp_hub.dsp_io->write_char(' ');
                }
                dsp_comp_hub.dsp_io->write_string_field(state_label, 30, false);

                dsp_comp_hub.dsp_io->cursor_xy(40, dsp_line);
                dsp_write_counter(counter);
                dsp_comp_hub.dsp_io->write_text(" in ");
                dsp_write_counter(rsc_counter);
                dsp_comp_hub.dsp_io->write_text(" resources");

                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

                ++dsp_line;

                if (state == DrbdVolume::disk_state::DISKLESS &&
                    (statistics.vlm_client_count > 0 && counter != statistics.vlm_client_count))
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(7, dsp_line);
                    dsp_comp_hub.dsp_io->write_text("- Diskless client:");
                    dsp_comp_hub.dsp_io->cursor_xy(29, dsp_line);
                    dsp_write_counter(statistics.vlm_client_count);

                    const uint32_t failed_counter = statistics.vlm_client_count < counter ?
                            counter - statistics.vlm_client_count : 0;
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(43, dsp_line);
                    dsp_comp_hub.dsp_io->write_text("Failed:");
                    dsp_comp_hub.dsp_io->cursor_xy(63, dsp_line);
                    dsp_write_counter(failed_counter);

                    ++dsp_line;
                }
            }
        };

    fn_dsp_con_state =
        [this](const DrbdConnection::state state, const uint32_t counter, uint16_t& dsp_line) -> void
        {
            if (counter > 0)
            {
                const uint32_t rsc_counter = get_count(*con_states_per_rsc, state);

                const std::string con_state_label(DrbdConnection::label_for_connection_state(state));
                if (state == DrbdConnection::state::CONNECTED)
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(5, dsp_line);
                }
                else
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(3, dsp_line);
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
                    dsp_comp_hub.dsp_io->write_char(' ');
                }
                dsp_comp_hub.dsp_io->write_string_field(con_state_label, 30, false);

                dsp_comp_hub.dsp_io->cursor_xy(40, dsp_line);
                dsp_write_counter(counter);
                dsp_comp_hub.dsp_io->write_text(" in ");
                dsp_write_counter(rsc_counter);
                dsp_comp_hub.dsp_io->write_text(" resources");

                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

                ++dsp_line;
            }
        };

    fn_dsp_con_sync_state =
        [this](const DrbdConnection::sync_state_type state, const uint32_t counter, uint16_t& dsp_line) -> void
        {
            if (counter > 0 &&
                (state == DrbdConnection::sync_state_type::SPLIT ||
                 state == DrbdConnection::sync_state_type::UNRELATED))
            {
                const uint32_t rsc_counter = get_count(*con_sync_states_per_rsc, state);

                const std::string con_sync_state_label(DrbdConnection::label_for_sync_state(state));
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                dsp_comp_hub.dsp_io->cursor_xy(3, dsp_line);
                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
                dsp_comp_hub.dsp_io->write_char(' ');

                dsp_comp_hub.dsp_io->write_string_field(con_sync_state_label, 30, false);
                dsp_comp_hub.dsp_io->cursor_xy(40, dsp_line);
                dsp_write_counter(counter);
                dsp_comp_hub.dsp_io->write_text(" in ");
                dsp_write_counter(rsc_counter);
                dsp_comp_hub.dsp_io->write_text(" resources");
            }
        };

    fn_dsp_peer_vlm_disk =
        [this](const DrbdVolume::disk_state state, const uint32_t counter, uint16_t& dsp_line) -> void
        {
            if (counter > 0)
            {
                const uint32_t rsc_counter = get_count(*peer_vlm_disk_states_per_rsc, state);

                const std::string disk_state_label(DrbdVolume::label_for_disk_state(state));
                if (state == DrbdVolume::disk_state::UP_TO_DATE ||
                    (state == DrbdVolume::disk_state::DISKLESS && counter == statistics.peer_vlm_client_count))
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(5, dsp_line);
                }
                else
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(3, dsp_line);
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
                    dsp_comp_hub.dsp_io->write_char(' ');
                }
                dsp_comp_hub.dsp_io->write_string_field(disk_state_label, 30, false);

                dsp_comp_hub.dsp_io->cursor_xy(40, dsp_line);
                dsp_write_counter(counter);
                dsp_comp_hub.dsp_io->write_text(" in ");
                dsp_write_counter(rsc_counter);
                dsp_comp_hub.dsp_io->write_text(" resources");

                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

                ++dsp_line;

                if (state == DrbdVolume::disk_state::DISKLESS &&
                    (statistics.peer_vlm_client_count > 0 && counter != statistics.peer_vlm_client_count))
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(7, dsp_line);
                    dsp_comp_hub.dsp_io->write_text("- Diskless client:");
                    dsp_comp_hub.dsp_io->cursor_xy(29, dsp_line);
                    dsp_write_counter(statistics.peer_vlm_client_count);

                    const uint32_t failed_counter = statistics.peer_vlm_client_count < counter ?
                            counter - statistics.peer_vlm_client_count : 0;
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(43, dsp_line);
                    dsp_comp_hub.dsp_io->write_text("Failed:");
                    dsp_comp_hub.dsp_io->cursor_xy(63, dsp_line);
                    dsp_write_counter(failed_counter);

                    ++dsp_line;
                }
            }
        };

    fn_dsp_peer_vlm_repl =
        [this](const DrbdVolume::repl_state state, const uint32_t counter, uint16_t& dsp_line) -> void
        {
            if (counter > 0)
            {
                const uint32_t rsc_counter = get_count(*peer_vlm_repl_states_per_rsc, state);

                const std::string repl_state_label(DrbdVolume::label_for_replication_state(state));
                if (state == DrbdVolume::repl_state::ESTABLISHED)
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(5, dsp_line);
                }
                else
                if (state == DrbdVolume::repl_state::SYNC_SOURCE ||
                    state == DrbdVolume::repl_state::SYNC_TARGET ||
                    state == DrbdVolume::repl_state::VERIFY_SOURCE ||
                    state == DrbdVolume::repl_state::VERIFY_TARGET ||
                    state == DrbdVolume::repl_state::PAUSED_SYNC_SOURCE ||
                    state == DrbdVolume::repl_state::PAUSED_SYNC_TARGET)
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->warn.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(3, dsp_line);
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_character_table->sym_warn.c_str());
                    dsp_comp_hub.dsp_io->write_char(' ');
                }
                else
                {
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
                    dsp_comp_hub.dsp_io->cursor_xy(3, dsp_line);
                    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
                    dsp_comp_hub.dsp_io->write_char(' ');
                }
                dsp_comp_hub.dsp_io->write_string_field(repl_state_label, 30, false);

                dsp_comp_hub.dsp_io->cursor_xy(40, dsp_line);
                dsp_write_counter(counter);
                dsp_comp_hub.dsp_io->write_text(" in ");
                dsp_write_counter(rsc_counter);
                dsp_comp_hub.dsp_io->write_text(" resources");

                dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

                ++dsp_line;
            }
        };
}

MDspOverview::~MDspOverview() noexcept
{
    deallocate_all_maps();
}

void MDspOverview::display_activated()
{
    set_page_count(6);
}

void MDspOverview::display_content()
{
    dsp_comp_hub.dsp_common->display_page_id(DisplayId::MDSP_OVERVIEW);

    if (!have_maps)
    {
        analyze_drbd_state();
    }

    cmd_refresh_analysis->clickable_area.page = get_page_nr();
    cmd_refresh_analysis->clickable_area.row = dsp_comp_hub.term_rows - DisplayConsts::CMD_LINE_Y - 1;

    const uint32_t MAX_STATE_LINE = 16;

    const uint32_t page_nr = get_page_nr();
    if (page_nr == 1)
    {
        // Resources overview

        dsp_comp_hub.dsp_io->cursor_xy(3, DisplayConsts::PAGE_NAV_Y + 1);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
        dsp_comp_hub.dsp_io->write_text("DRBD state overview - Resources");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

        uint16_t dsp_line = 5;

        DrbdRole::resource_role key = DrbdRole::resource_role::PRIMARY;
        const uint32_t primary_count = get_count(*rsc_roles, key);

        key = DrbdRole::resource_role::SECONDARY;
        const uint32_t secondary_count = get_count(*rsc_roles, key);

        key = DrbdRole::resource_role::UNKNOWN;
        const uint32_t unknown_count = get_count(*rsc_roles, key);

        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->norm.c_str());

        uint16_t dsp_column = 21;
        dsp_comp_hub.dsp_io->cursor_xy(5, dsp_line);
        dsp_comp_hub.dsp_io->write_text("Primary:");
        dsp_comp_hub.dsp_io->cursor_xy(dsp_column, dsp_line);
        dsp_write_counter(primary_count);
        ++dsp_line;

        dsp_comp_hub.dsp_io->cursor_xy(5, dsp_line);
        dsp_comp_hub.dsp_io->write_text("Secondary:");
        dsp_comp_hub.dsp_io->cursor_xy(dsp_column, dsp_line);
        dsp_write_counter(secondary_count);
        ++dsp_line;

        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

        if (unknown_count > 0)
        {
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
            dsp_comp_hub.dsp_io->cursor_xy(3, dsp_line);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
            dsp_comp_hub.dsp_io->write_text(" Unknown:");
            dsp_comp_hub.dsp_io->cursor_xy(dsp_column, dsp_line);
            dsp_write_counter(unknown_count);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
            ++dsp_line;
        }

        ++dsp_line;

        const uint32_t no_quorum_count = get_count(*rsc_quorum_map, false);

        if (no_quorum_count > 0)
        {
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->alert.c_str());
            dsp_comp_hub.dsp_io->cursor_xy(3, dsp_line);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_character_table->sym_alert.c_str());
            dsp_comp_hub.dsp_io->write_text(" Resources without quorum: ");
            dsp_write_counter(no_quorum_count);
            dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());
            ++dsp_line;
        }

        ++dsp_line;

        dsp_column = 41;
        dsp_comp_hub.dsp_io->cursor_xy(5, dsp_line);
        dsp_comp_hub.dsp_io->write_text("Resources with any Primary nodes: ");
        dsp_comp_hub.dsp_io->cursor_xy(dsp_column, dsp_line);
        dsp_write_counter(rsc_primary_count);
        ++dsp_line;
        dsp_comp_hub.dsp_io->cursor_xy(5, dsp_line);
        dsp_comp_hub.dsp_io->write_text("Resources with no Primary nodes: ");
        dsp_comp_hub.dsp_io->cursor_xy(dsp_column, dsp_line);
        dsp_write_counter(rsc_no_primary_count);
        ++dsp_line;
    }
    else
    if (page_nr == 2)
    {
        // Volume states

        dsp_comp_hub.dsp_io->cursor_xy(3, DisplayConsts::PAGE_NAV_Y + 1);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
        dsp_comp_hub.dsp_io->write_text("DRBD state overview - Volumes");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

        uint16_t dsp_line = 5;
        iterate_map_entries(*vlm_disk_states, dsp_line, MAX_STATE_LINE, fn_dsp_vlm_disk);
    }
    else
    if (page_nr == 3)
    {
        dsp_comp_hub.dsp_io->cursor_xy(3, DisplayConsts::PAGE_NAV_Y + 1);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
        dsp_comp_hub.dsp_io->write_text("DRBD state overview - Connections");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

        uint16_t dsp_line = 5;
        iterate_map_entries(*con_states, dsp_line, MAX_STATE_LINE, fn_dsp_con_state);

        ++dsp_line;
        iterate_map_entries(*con_sync_states, dsp_line, MAX_STATE_LINE, fn_dsp_con_sync_state);
    }
    else
    if (page_nr == 4)
    {
        dsp_comp_hub.dsp_io->cursor_xy(3, DisplayConsts::PAGE_NAV_Y + 1);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
        dsp_comp_hub.dsp_io->write_text("DRBD state overview - Peer volumes - Disk state");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

        uint16_t dsp_line = 5;
        iterate_map_entries(*peer_vlm_disk_states, dsp_line, MAX_STATE_LINE, fn_dsp_peer_vlm_disk);
    }
    else
    if (page_nr == 5)
    {
        dsp_comp_hub.dsp_io->cursor_xy(3, DisplayConsts::PAGE_NAV_Y + 1);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
        dsp_comp_hub.dsp_io->write_text("DRBD state overview - Peer volumes - Replication state");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

        uint16_t dsp_line = 5;
        iterate_map_entries(*peer_vlm_repl_states, dsp_line, MAX_STATE_LINE, fn_dsp_peer_vlm_repl);
    }
    else
    if (page_nr == 6)
    {
        dsp_comp_hub.dsp_io->cursor_xy(3, DisplayConsts::PAGE_NAV_Y + 1);
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->emphasis_text.c_str());
        dsp_comp_hub.dsp_io->write_text("DRBD state overview - Cluster statistics");
        dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->rst.c_str());

        uint16_t dsp_line = 5;

        dsp_comp_hub.dsp_io->cursor_xy(5, dsp_line);
        dsp_comp_hub.dsp_io->write_text("Resources:");
        dsp_comp_hub.dsp_io->cursor_xy(25, dsp_line);
        dsp_write_counter(statistics.rsc_count);

        ++dsp_line;

        dsp_comp_hub.dsp_io->cursor_xy(5, dsp_line);
        dsp_comp_hub.dsp_io->write_text("Volumes:");
        dsp_comp_hub.dsp_io->cursor_xy(25, dsp_line);
        dsp_write_counter(statistics.vlm_disk_count);

        ++dsp_line;

        dsp_comp_hub.dsp_io->cursor_xy(5, dsp_line);
        dsp_comp_hub.dsp_io->write_text("Volumes (Client):");
        dsp_comp_hub.dsp_io->cursor_xy(25, dsp_line);
        dsp_write_counter(statistics.vlm_client_count);

        ++dsp_line;

        dsp_comp_hub.dsp_io->cursor_xy(5, dsp_line);
        dsp_comp_hub.dsp_io->write_text("Connections:");
        dsp_comp_hub.dsp_io->cursor_xy(25, dsp_line);
        dsp_write_counter(statistics.con_count);
    }

    dsp_comp_hub.dsp_io->cursor_xy(1, dsp_comp_hub.term_rows - DisplayConsts::CMD_LINE_Y - 1);
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->hotkey_field.c_str());
    dsp_comp_hub.dsp_io->write_text(" R ");
    dsp_comp_hub.dsp_io->write_text(dsp_comp_hub.active_color_table->hotkey_label.c_str());
    dsp_comp_hub.dsp_io->write_text(" Refresh analysis");
    dsp_comp_hub.dsp_common->application_idle();
}

void MDspOverview::analyze_drbd_state()
{
    dsp_comp_hub.dsp_common->application_working();

    if (!have_maps)
    {
        allocate_all_maps();
    }
    else
    {
        reset_all_map_counters();
    }

    ClusterStats statsCounters;

    rsc_primary_count = 0;
    rsc_no_primary_count = 0;

    selection_filter::FilterChain<DrbdResource> chain_primary_rsc;
    selection_filter::FilterChain<DrbdConnection> chain_primary_con;

    selection_filter::make_resource_role_selector(chain_primary_rsc, DrbdRole::PRIMARY);
    selection_filter::make_connection_role_selector(chain_primary_con, DrbdRole::PRIMARY);

    ResourcesMap::ValuesIterator rsc_iter(*(dsp_comp_hub.rsc_map));
    while (rsc_iter.has_next())
    {
        ++statsCounters.rsc_count;
        DrbdResource* const rsc = rsc_iter.next();

        const bool rsc_primary = chain_primary_rsc.match(*rsc);

        // Count resource roles
        {
            const DrbdRole::resource_role role = rsc->get_role();
            increase_counter(*rsc_roles, role);
        }

        // Count resource quorum
        {
            const bool quorum = !rsc->has_quorum_alert();
            increase_counter(*rsc_quorum_map, quorum);
        }

        // Analyze volumes
        {
            uint64_t rsc_counted_disk_states = 0;

            VolumesMap::ValuesIterator vlm_iter(rsc->volumes_iterator());
            while (vlm_iter.has_next())
            {
                DrbdVolume* const vlm = vlm_iter.next();

                const bool is_client = vlm->get_client_state() == DrbdVolume::client_state::ENABLED;

                // Count volume quorum
                {
                    const bool quorum = !vlm->has_quorum_alert();
                    increase_counter(*vlm_quorum_map, quorum);
                }

                // Count volume disk state
                {
                    const DrbdVolume::disk_state disk_state = vlm->get_disk_state();
                    increase_counter(*vlm_disk_states, disk_state);

                    if (disk_state == DrbdVolume::disk_state::DISKLESS && is_client)
                    {
                        ++statsCounters.vlm_client_count;
                    }
                    else
                    {
                        ++statsCounters.vlm_disk_count;
                    }

                    const uint64_t rsc_counted_flag =
                        static_cast<uint64_t> (1) << static_cast<uint16_t> (disk_state);
                    if ((rsc_counted_disk_states & rsc_counted_flag) != rsc_counted_flag)
                    {
                        increase_counter(*vlm_disk_states_per_rsc, disk_state);
                        rsc_counted_disk_states |= rsc_counted_flag;
                    }
                }
            } // end volumes loop
        } // end volumes analysis scope

        // Analyze connections & peer volumes
        {
            uint64_t rsc_counted_con_states = 0;
            uint64_t rsc_counted_con_sync_states = 0;

            uint64_t rsc_counted_peer_disk_states = 0;
            uint64_t rsc_counted_peer_repl_states = 0;

            bool any_con_primary = false;

            ConnectionsMap::ValuesIterator con_iter(rsc->connections_iterator());
            while (con_iter.has_next())
            {
                ++statsCounters.con_count;
                DrbdConnection* const con = con_iter.next();

                if (!rsc_primary)
                {
                    if (chain_primary_con.match(*con))
                    {
                        any_con_primary = true;
                    }
                }

                // Count connection state
                {
                    const DrbdConnection::state con_net_state = con->get_connection_state();
                    increase_counter(*con_states, con_net_state);

                    const uint64_t rsc_net_counted_flag =
                        static_cast<uint64_t> (1) << static_cast<uint16_t>(con_net_state);
                    if ((rsc_counted_con_states & rsc_net_counted_flag) != rsc_net_counted_flag)
                    {
                        increase_counter(*con_states_per_rsc, con_net_state);
                        rsc_counted_con_states |= rsc_net_counted_flag;
                    }

                    // Count connection synchronization state
                    const DrbdConnection::sync_state_type sync_state =
                        (con_net_state == DrbdConnection::state::CONNECTED ?
                         DrbdConnection::sync_state_type::RESYNCABLE :
                         con->get_sync_state());
                    increase_counter(*con_sync_states, sync_state);

                    const uint64_t rsc_sync_counted_flag =
                        static_cast<uint64_t> (1) << static_cast<uint16_t> (sync_state);
                    if ((rsc_counted_con_sync_states & rsc_sync_counted_flag) != rsc_sync_counted_flag)
                    {
                        increase_counter(*con_sync_states_per_rsc, sync_state);
                        rsc_counted_con_sync_states |= rsc_sync_counted_flag;
                    }
                }

                VolumesMap::ValuesIterator peer_vlm_iter(con->volumes_iterator());
                while (peer_vlm_iter.has_next())
                {
                    DrbdVolume* const peer_vlm = peer_vlm_iter.next();

                    const bool is_client = peer_vlm->get_client_state() == DrbdVolume::client_state::ENABLED;

                    // Count peer volume disk state
                    {
                        const DrbdVolume::disk_state disk_state = peer_vlm->get_disk_state();
                        increase_counter(*peer_vlm_disk_states, disk_state);

                        const uint64_t rsc_counted_flag =
                            static_cast<uint64_t> (1) << static_cast<uint16_t> (disk_state);
                        if ((rsc_counted_peer_disk_states & rsc_counted_flag) != rsc_counted_flag)
                        {
                            increase_counter(*peer_vlm_disk_states_per_rsc, disk_state);
                            rsc_counted_peer_disk_states |= rsc_counted_flag;
                        }

                        if (disk_state == DrbdVolume::disk_state::DISKLESS && is_client)
                        {
                            ++statsCounters.peer_vlm_client_count;
                        }
                    }

                    // Count peer volume replication state
                    {
                        const DrbdVolume::repl_state repl_state = peer_vlm->get_replication_state();
                        increase_counter(*peer_vlm_repl_states, repl_state);

                        const uint64_t rsc_counted_flag =
                            static_cast<uint64_t> (1) << static_cast<uint16_t> (repl_state);
                        if ((rsc_counted_peer_repl_states & rsc_counted_flag) != rsc_counted_flag)
                        {
                            increase_counter(*peer_vlm_repl_states_per_rsc, repl_state);
                            rsc_counted_peer_repl_states |= rsc_counted_flag;
                        }
                    }
                } // end peer volumes loop
            } // end connections loop

            if (rsc_primary || any_con_primary)
            {
                ++rsc_primary_count;
            }
            else
            {
                ++rsc_no_primary_count;
            }

        } // end connections analysis scope
    } // end resource loop

    // Update statistics
    statistics = statsCounters;
}

// Format and display a counter in the range [0, 9999999] with tousands-separators and right-aligned
// (field width 9 characters, e.g. "1,000,000" or "   10,000")
void MDspOverview::dsp_write_counter(const uint32_t counter)
{
    std::string formatted_counter;
    string_transformations::format_uint64(counter, formatted_counter, false);

    const size_t nr_length = formatted_counter.length();
    if (nr_length < 9)
    {
        dsp_comp_hub.dsp_io->write_fill_char(' ', 9 - nr_length);
        dsp_comp_hub.dsp_io->write_string_field(formatted_counter, 9, false);
    }
}

bool MDspOverview::key_pressed(const uint32_t key)
{
    bool intercepted = MDspBase::key_pressed(key);
    if (!intercepted)
    {
        // Option pages
        if (key == KeyCodes::FUNC_01)
        {
            helptext::open_help_page(helptext::id_type::OVRVW_HELP, dsp_comp_hub);
            intercepted = true;
        }
        else
        if (key == static_cast<uint32_t> ('r') || key == static_cast<uint32_t> ('R'))
        {
            if (cmd_refresh_analysis->handler_func != nullptr)
            {
                (*(cmd_refresh_analysis->handler_func))();
            }
            intercepted = true;
        }
    }
    return intercepted;
}

bool MDspOverview::mouse_action(MouseEvent& mouse)
{
    bool intercepted = MDspBase::mouse_action(mouse);
    if (!intercepted)
    {
        const uint32_t page_nr = get_page_nr();
        if (cmd_refresh_analysis->clickable_area.is_click_in_area(page_nr, mouse.coord_row, mouse.coord_column))
        {
            if (cmd_refresh_analysis->handler_func != nullptr)
            {
                (*(cmd_refresh_analysis->handler_func))();
            }
            intercepted = true;
        }
    }
    return intercepted;
}

uint64_t MDspOverview::get_update_mask() noexcept
{
    return 0;
}

void MDspOverview::display_closed()
{
    MDspBase::display_closed();
    deallocate_all_maps();
}

void MDspOverview::synchronize_data()
{
}

void MDspOverview::cursor_to_next_item()
{
}

void MDspOverview::cursor_to_previous_item()
{
}

void MDspOverview::text_cursor_ops()
{
    // no-op; prevents MDspMenuBase from positioning the cursor for the option field, which is not used
}

// @throws std::bad_alloc
void MDspOverview::allocate_all_maps()
{
    rsc_roles = std::unique_ptr<RscRoleMap>(
        new RscRoleMap(&comparators::compare<DrbdResource::resource_role>)
    );
    vlm_disk_states = std::unique_ptr<VlmDiskStateMap>(
        new VlmDiskStateMap(&comparators::compare<DrbdVolume::disk_state>)
    );
    vlm_repl_states = std::unique_ptr<VlmReplStateMap>(
        new VlmReplStateMap(&comparators::compare<DrbdVolume::repl_state>)
    );
    con_states = std::unique_ptr<ConStateMap>(
        new ConStateMap(&comparators::compare<DrbdConnection::state>)
    );
    con_sync_states = std::unique_ptr<ConSyncStateMap>(
        new ConSyncStateMap(&comparators::compare<DrbdConnection::sync_state_type>)
    );
    peer_vlm_disk_states = std::unique_ptr<VlmDiskStateMap>(
        new VlmDiskStateMap(&comparators::compare<DrbdVolume::disk_state>)
    );
    peer_vlm_repl_states = std::unique_ptr<VlmReplStateMap>(
        new VlmReplStateMap(&comparators::compare<DrbdVolume::repl_state>)
    );
    vlm_quorum_map = std::unique_ptr<QuorumMap>(
        new QuorumMap(&bool_comparator)
    );

    vlm_disk_states_per_rsc = std::unique_ptr<VlmDiskStateMap>(
        new VlmDiskStateMap(&comparators::compare<DrbdVolume::disk_state>)
    );
    vlm_repl_states_per_rsc = std::unique_ptr<VlmReplStateMap>(
        new VlmReplStateMap(&comparators::compare<DrbdVolume::repl_state>)
    );
    con_states_per_rsc = std::unique_ptr<ConStateMap>(
        new ConStateMap(&comparators::compare<DrbdConnection::state>)
    );
    con_sync_states_per_rsc = std::unique_ptr<ConSyncStateMap>(
        new ConSyncStateMap(&comparators::compare<DrbdConnection::sync_state_type>)
    );
    peer_vlm_disk_states_per_rsc = std::unique_ptr<VlmDiskStateMap>(
        new VlmDiskStateMap(&comparators::compare<DrbdVolume::disk_state>)
    );
    peer_vlm_repl_states_per_rsc = std::unique_ptr<VlmReplStateMap>(
        new VlmReplStateMap(&comparators::compare<DrbdVolume::repl_state>)
    );
    rsc_quorum_map = std::unique_ptr<QuorumMap>(
        new QuorumMap(&bool_comparator)
    );

    init_all_map_counters();

    have_maps = true;
}

void MDspOverview::init_all_map_counters()
{
    {
        RscRoleMap& map = *rsc_roles;
        allocate_map_counter(map, DrbdRole::resource_role::PRIMARY);
        allocate_map_counter(map, DrbdRole::resource_role::SECONDARY);
        allocate_map_counter(map, DrbdRole::resource_role::UNKNOWN);
    }

    init_vlm_disk_state_counters(*vlm_disk_states);
    init_vlm_repl_state_counters(*vlm_repl_states);
    init_con_state_counters(*con_states);
    init_con_sync_state_counters(*con_sync_states);
    init_vlm_disk_state_counters(*peer_vlm_disk_states);
    init_vlm_repl_state_counters(*peer_vlm_repl_states);

    {
        QuorumMap& map = *vlm_quorum_map;
        allocate_map_counter(map, true);
        allocate_map_counter(map, false);
    }

    init_vlm_disk_state_counters(*vlm_disk_states_per_rsc);
    init_vlm_repl_state_counters(*vlm_repl_states_per_rsc);
    init_con_state_counters(*con_states_per_rsc);
    init_con_sync_state_counters(*con_sync_states_per_rsc);
    init_vlm_disk_state_counters(*peer_vlm_disk_states_per_rsc);
    init_vlm_repl_state_counters(*peer_vlm_repl_states_per_rsc);

    {
        QuorumMap& map = *rsc_quorum_map;
        allocate_map_counter(map, true);
        allocate_map_counter(map, false);
    }
}

void MDspOverview::reset_all_map_counters()
{
    reset_map_counters(*rsc_roles);

    reset_map_counters(*vlm_disk_states);
    reset_map_counters(*vlm_repl_states);
    reset_map_counters(*con_states);
    reset_map_counters(*con_sync_states);
    reset_map_counters(*peer_vlm_disk_states);
    reset_map_counters(*peer_vlm_repl_states);

    reset_map_counters(*vlm_disk_states_per_rsc);
    reset_map_counters(*vlm_repl_states_per_rsc);
    reset_map_counters(*con_states_per_rsc);
    reset_map_counters(*con_sync_states_per_rsc);
    reset_map_counters(*peer_vlm_disk_states_per_rsc);
    reset_map_counters(*peer_vlm_repl_states_per_rsc);

    reset_map_counters(*rsc_quorum_map);
}

void MDspOverview::init_vlm_disk_state_counters(VlmDiskStateMap& map)
{
    allocate_map_counter(map, DrbdVolume::disk_state::ATTACHING);
    allocate_map_counter(map, DrbdVolume::disk_state::CONSISTENT);
    allocate_map_counter(map, DrbdVolume::disk_state::DETACHING);
    allocate_map_counter(map, DrbdVolume::disk_state::DISKLESS);
    allocate_map_counter(map, DrbdVolume::disk_state::FAILED);
    allocate_map_counter(map, DrbdVolume::disk_state::INCONSISTENT);
    allocate_map_counter(map, DrbdVolume::disk_state::NEGOTIATING);
    allocate_map_counter(map, DrbdVolume::disk_state::OUTDATED);
    allocate_map_counter(map, DrbdVolume::disk_state::UNKNOWN);
    allocate_map_counter(map, DrbdVolume::disk_state::UP_TO_DATE);
}

void MDspOverview::init_vlm_repl_state_counters(VlmReplStateMap& map)
{
    allocate_map_counter(map, DrbdVolume::repl_state::AHEAD);
    allocate_map_counter(map, DrbdVolume::repl_state::BEHIND);
    allocate_map_counter(map, DrbdVolume::repl_state::ESTABLISHED);
    allocate_map_counter(map, DrbdVolume::repl_state::OFF);
    allocate_map_counter(map, DrbdVolume::repl_state::PAUSED_SYNC_SOURCE);
    allocate_map_counter(map, DrbdVolume::repl_state::PAUSED_SYNC_TARGET);
    allocate_map_counter(map, DrbdVolume::repl_state::STARTING_SYNC_SOURCE);
    allocate_map_counter(map, DrbdVolume::repl_state::STARTING_SYNC_TARGET);
    allocate_map_counter(map, DrbdVolume::repl_state::SYNC_SOURCE);
    allocate_map_counter(map, DrbdVolume::repl_state::SYNC_TARGET);
    allocate_map_counter(map, DrbdVolume::repl_state::UNKNOWN);
    allocate_map_counter(map, DrbdVolume::repl_state::VERIFY_SOURCE);
    allocate_map_counter(map, DrbdVolume::repl_state::VERIFY_TARGET);
    allocate_map_counter(map, DrbdVolume::repl_state::WF_BITMAP_SOURCE);
    allocate_map_counter(map, DrbdVolume::repl_state::WF_BITMAP_TARGET);
    allocate_map_counter(map, DrbdVolume::repl_state::WF_SYNC_UUID);
}

void MDspOverview::init_con_state_counters(ConStateMap& map)
{
    allocate_map_counter(map, DrbdConnection::state::BROKEN_PIPE);
    allocate_map_counter(map, DrbdConnection::state::CONNECTED);
    allocate_map_counter(map, DrbdConnection::state::CONNECTING);
    allocate_map_counter(map, DrbdConnection::state::DISCONNECTING);
    allocate_map_counter(map, DrbdConnection::state::NETWORK_FAILURE);
    allocate_map_counter(map, DrbdConnection::state::PROTOCOL_ERROR);
    allocate_map_counter(map, DrbdConnection::state::STANDALONE);
    allocate_map_counter(map, DrbdConnection::state::TEAR_DOWN);
    allocate_map_counter(map, DrbdConnection::state::TIMEOUT);
    allocate_map_counter(map, DrbdConnection::state::UNCONNECTED);
    allocate_map_counter(map, DrbdConnection::state::UNKNOWN);
}

void MDspOverview::init_con_sync_state_counters(ConSyncStateMap& map)
{
    allocate_map_counter(map, DrbdConnection::sync_state_type::RESYNCABLE);
    allocate_map_counter(map, DrbdConnection::sync_state_type::SPLIT);
    allocate_map_counter(map, DrbdConnection::sync_state_type::UNRELATED);
}

void MDspOverview::deallocate_all_maps() noexcept
{
    have_maps = false;

    deallocate_map(rsc_roles);
    deallocate_map(vlm_disk_states);
    deallocate_map(vlm_repl_states);
    deallocate_map(con_states);
    deallocate_map(con_sync_states);
    deallocate_map(peer_vlm_disk_states);
    deallocate_map(peer_vlm_repl_states);
    deallocate_map(vlm_quorum_map);

    deallocate_map(vlm_disk_states_per_rsc);
    deallocate_map(vlm_repl_states_per_rsc);
    deallocate_map(con_states_per_rsc);
    deallocate_map(con_sync_states_per_rsc);
    deallocate_map(peer_vlm_disk_states_per_rsc);
    deallocate_map(peer_vlm_repl_states_per_rsc);
    deallocate_map(rsc_quorum_map);
}

int MDspOverview::bool_comparator(const bool* const value, const bool* const other) noexcept
{
    int result = 0;
    if (!(*value) && *other)
    {
        result = -1;
    }
    else
    if (*value && !(*other))
    {
        result = 1;
    }
    return result;
}

MDspOverview::ClusterStats::ClusterStats()
{
}

MDspOverview::ClusterStats::~ClusterStats() noexcept
{
}
