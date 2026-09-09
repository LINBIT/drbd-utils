#include <terminal/selection_filter.h>
#include <comparators.h>

namespace selection_filter
{
    FilterNode<DrbdResource>& make_resource_quorum_selector(
        FilterNode<DrbdResource>& chain_end,
        bool quorum_flag
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdResource>>(
            new GenericFilterNode<DrbdResource, bool>(
                &chain_end,
                [](const DrbdResource& resource, const bool& state_ref)
                {
                    return resource.has_quorum_alert() != state_ref;
                },
                quorum_flag
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdResource>& make_resource_role_selector(
        FilterNode<DrbdResource>& chain_end,
        DrbdRole::resource_role role
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdResource>>(
            new GenericFilterNode<DrbdResource, DrbdRole::resource_role>(
                &chain_end,
                [](const DrbdResource& resource, const DrbdRole::resource_role& role_ref)
                {
                    return resource.get_role() == role_ref;
                },
                role
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdVolume>& make_volume_quorum_selector(
        FilterNode<DrbdVolume>& chain_end,
        bool quorum_flag
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdVolume>>(
            new GenericFilterNode<DrbdVolume, bool>(
                &chain_end,
                [](const DrbdVolume& volume, const bool& state_ref)
                {
                    return volume.has_quorum_alert() != state_ref;
                },
                quorum_flag
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdVolume>& make_volume_disk_state_selector(
        FilterNode<DrbdVolume>& chain_end,
        DrbdVolume::disk_state state
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdVolume>>(
            new GenericFilterNode<DrbdVolume, DrbdVolume::disk_state>(
                &chain_end,
                [](const DrbdVolume& volume, const DrbdVolume::disk_state& state_ref)
                {
                    return volume.get_disk_state() == state_ref;
                },
                state
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdVolume>& make_volume_repl_state_selector(
        FilterNode<DrbdVolume>& chain_end,
        DrbdVolume::repl_state state
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdVolume>>(
            new GenericFilterNode<DrbdVolume, DrbdVolume::repl_state>(
                &chain_end,
                [](const DrbdVolume& volume, const DrbdVolume::repl_state& state_ref)
                {
                    return volume.get_replication_state() == state_ref;
                },
                state
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdVolume>& make_volume_client_state_selector(
        FilterNode<DrbdVolume>& chain_end,
        bool state
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdVolume>>(
            new GenericFilterNode<DrbdVolume, bool>(
                &chain_end,
                [](const DrbdVolume& volume, const bool& state_ref)
                {
                    return state_ref ?
                        (volume.get_disk_state() == DrbdVolume::disk_state::DISKLESS &&
                         volume.get_client_state() == DrbdVolume::client_state::ENABLED) :
                        (volume.get_disk_state() == DrbdVolume::disk_state::DISKLESS &&
                         volume.get_client_state() != DrbdVolume::client_state::ENABLED);
                },
                state
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdConnection>& make_connection_role_selector(
        FilterNode<DrbdConnection>& chain_end,
        DrbdRole::resource_role role
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdConnection>>(
            new GenericFilterNode<DrbdConnection, DrbdRole::resource_role>(
                &chain_end,
                [](const DrbdConnection& connection, const DrbdRole::resource_role& role_ref)
                {
                    return connection.get_role() == role_ref;
                },
                role
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdConnection>& make_connection_state_selector(
        FilterNode<DrbdConnection>& chain_end,
        DrbdConnection::state state
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdConnection>>(
            new GenericFilterNode<DrbdConnection, DrbdConnection::state>(
                &chain_end,
                [](const DrbdConnection& connection, const DrbdConnection::state& state_ref)
                {
                    return connection.get_connection_state() == state_ref;
                },
                state
            )
        );
        return *(chain_end.next_node);
    }

    FilterNode<DrbdConnection>& make_connection_sync_state_selector(
        FilterNode<DrbdConnection>& chain_end,
        DrbdConnection::sync_state_type state
    )
    {
        chain_end.next_node = std::unique_ptr<FilterNode<DrbdConnection>>(
            new GenericFilterNode<DrbdConnection, DrbdConnection::sync_state_type>(
                &chain_end,
                [](const DrbdConnection& connection, const DrbdConnection::sync_state_type& state_ref)
                {
                    return connection.get_sync_state() == state_ref;
                },
                state
            )
        );
        return *(chain_end.next_node);
    }

    // @throws std::bad_alloc, string_matching::PatternLimitException
    void filter_select(
        FilterSettings&     settings,
        ResourcesMap&       rsc_map,
        SharedData&         dsp_shared,
        MessageLog&         debug_log
    )
    {
        // Setup name pattern restrictions
        selection_filter::FilterChain<DrbdResource> rsc_name_chain;
        if (!settings.rsc_name_pattern.empty())
        {
            selection_filter::make_object_name_selector(rsc_name_chain, settings.rsc_name_pattern);
        }

        selection_filter::FilterChain<DrbdConnection> con_name_chain;
        if (!settings.con_name_pattern.empty())
        {
            selection_filter::make_object_name_selector(con_name_chain, settings.con_name_pattern);
        }

        // Filtering is effectively restricted to already selected resources
        const bool eff_rstr_to_slct_rsc =
            settings.restrictions.selected_resources || settings.restrictions.selected_volumes ||
            settings.restrictions.selected_connections || settings.restrictions.selected_peer_volumes;
        // Filtering is effectively restricted to already selected connections
        const bool eff_rstr_to_slct_con =
            settings.restrictions.selected_connections || settings.restrictions.selected_peer_volumes;

        // Volumes that match the volume filters and are to be selected if the resource as a whole matches
        std::unique_ptr<VolumeSelectionMap>     vlm_to_slct;
        if (settings.targets.select_volumes && !settings.restrictions.selected_volumes)
        {
            vlm_to_slct = std::unique_ptr<VolumeSelectionMap>(
                new VolumeSelectionMap(&comparators::compare<uint16_t>)
            );
        }
        // Connections and peer volumes are the last objects required to match, so they can be selected immediately

        // Filter resources or selected resources
        std::unique_ptr<ResourcesMap::ValuesIterator>           rsc_iter;
        std::unique_ptr<ResourceSelectionMap::KeysIterator>     slct_rsc_iter;
        if (eff_rstr_to_slct_rsc)
        {
            slct_rsc_iter = std::unique_ptr<ResourceSelectionMap::KeysIterator>(
                new ResourceSelectionMap::KeysIterator(*(dsp_shared.selected_resources))
            );
        }
        else
        {
            rsc_iter = std::unique_ptr<ResourcesMap::ValuesIterator>(
                new ResourcesMap::ValuesIterator(rsc_map)
            );
        }
        for (
            DrbdResource* rsc = next_resource(rsc_map, rsc_iter, slct_rsc_iter);
            rsc != nullptr;
            rsc = next_resource(rsc_map, rsc_iter, slct_rsc_iter)
        )
        {
            const std::string& rsc_name = rsc->get_name();
            ResourceSubSelections* cur_rsc_sub_selections = nullptr;

            // Apply resource filters
            bool rsc_match = rsc_name_chain.match(*rsc) != settings.invert.resource_name;
            rsc_match = rsc_match && settings.rsc_op_chain.match(*rsc);
            rsc_match = rsc_match && settings.rsc_quorum_chain.match(*rsc);
            rsc_match = rsc_match && settings.rsc_role_chain.match(*rsc);

            if (rsc_match)
            {
                // Filter volumes or selected volumes
                std::unique_ptr<VolumesMap::ValuesIterator>         vlm_iter;
                std::unique_ptr<VolumeSelectionMap::KeysIterator>   slct_vlm_iter;
                bool vlm_match = false;
                if (settings.restrictions.selected_volumes)
                {
                    ResourceSubSelections* const sub_selections =
                        dsp_shared.selected_resources->get(&rsc_name);
                    if (sub_selections != nullptr && sub_selections->volume_selection)
                    {
                        slct_vlm_iter = std::unique_ptr<VolumeSelectionMap::KeysIterator>(
                            new VolumeSelectionMap::KeysIterator(*(sub_selections->volume_selection))
                        );
                    }
                }
                else
                {
                    vlm_iter = std::unique_ptr<VolumesMap::ValuesIterator>(
                        new VolumesMap::ValuesIterator(std::move(rsc->volumes_iterator()))
                    );
                    if (!vlm_iter->has_next())
                    {
                        // If there are no volumes, but also no effective filters, the volume filter matches
                        vlm_match = !settings.filter_vlm_number &&
                                    settings.vlm_op_chain.is_empty() &&
                                    settings.vlm_quorum_chain.is_empty() &&
                                    settings.vlm_state_chain.is_empty();
                    }
                }
                for (
                    DrbdVolume* vlm = next_volume(rsc, vlm_iter, slct_vlm_iter);
                    vlm != nullptr;
                    vlm = next_volume(rsc, vlm_iter, slct_vlm_iter)
                )
                {
                    // Apply volume filters
                    bool single_vlm_match = !settings.filter_vlm_number ||
                                            ((vlm->get_volume_nr() == settings.vlm_number) !=
                                            settings.invert.volume_number);
                    single_vlm_match = single_vlm_match && settings.vlm_op_chain.match(*vlm);
                    single_vlm_match = single_vlm_match && settings.vlm_quorum_chain.match(*vlm);
                    if (single_vlm_match)
                    {
                        single_vlm_match = settings.vlm_state_chain.match(*vlm) != settings.invert.volume_state;
                    }

                    if (single_vlm_match && vlm_to_slct)
                    {
                        // Remember to select the volume if the resource matches
                        const uint16_t* const vlm_nr = &vlm->get_volume_nr_ref();
                        try
                        {
                            vlm_to_slct->insert(vlm_nr, nullptr);
                        }
                        catch (dsaext::DuplicateInsertException&)
                        {
                            std::string debug_msg("DuplicateInsertException in filter_select, "
                                                  "remembered volumes for selection");
                            debug_log.add_entry(MessageLog::log_level::WARN, debug_msg);
                        }
                    }

                    vlm_match = vlm_match || single_vlm_match;
                }

                // Resource no longer matches if none of its volumes match
                rsc_match = vlm_match;
            }

            if (rsc_match)
            {
                // Filter connections or selected connections
                std::unique_ptr<ConnectionsMap::ValuesIterator>         con_iter;
                std::unique_ptr<ConnectionSelectionMap::KeysIterator>   slct_con_iter;
                bool con_match = false;
                if (eff_rstr_to_slct_con)
                {
                    ResourceSubSelections* const sub_selections =
                        dsp_shared.selected_resources->get(&rsc_name);
                    if (sub_selections != nullptr && sub_selections->connection_selection)
                    {
                        slct_con_iter = std::unique_ptr<ConnectionSelectionMap::KeysIterator>(
                            new ConnectionSelectionMap::KeysIterator(*(sub_selections->connection_selection))
                        );
                    }
                }
                else
                {
                    con_iter = std::unique_ptr<ConnectionsMap::ValuesIterator>(
                        new ConnectionsMap::ValuesIterator(std::move(rsc->connections_iterator()))
                    );
                    if (!con_iter->has_next())
                    {
                        // If there are no connections but also no effective filters,
                        // the connection filter matches
                        con_match = con_name_chain.is_empty() && settings.con_op_chain.is_empty() &&
                                    settings.con_role_chain.is_empty();
                    }
                }
                for (
                    DrbdConnection* con = next_connection(rsc, con_iter, slct_con_iter);
                    con != nullptr;
                    con = next_connection(rsc, con_iter, slct_con_iter)
                )
                {
                    const std::string* cur_con_name = nullptr;
                    ConnectionSelectionMap::Node* cur_con_slct_node = nullptr;

                    // Apply connection filters
                    bool single_con_match = con_name_chain.match(*con) != settings.invert.connection_name;
                    single_con_match = single_con_match && settings.con_op_chain.match(*con);
                    single_con_match = single_con_match && settings.con_role_chain.match(*con);
                    if (single_con_match)
                    {
                        single_con_match = settings.con_state_chain.match(*con) != settings.invert.connection_state;
                    }

                    if (single_con_match)
                    {
                        // Filter peer volumes or selected peer volumes
                        const std::string& con_name = con->get_name();
                        std::unique_ptr<VolumesMap::ValuesIterator>         peer_vlm_iter;
                        std::unique_ptr<VolumeSelectionMap::KeysIterator>   slct_peer_vlm_iter;
                        bool peer_vlm_match = false;
                        if (settings.restrictions.selected_peer_volumes)
                        {
                            ResourceSubSelections* const sub_selections =
                                dsp_shared.selected_resources->get(&rsc_name);
                            if (sub_selections != nullptr && sub_selections->connection_selection)
                            {
                                VolumeSelectionMap* const selected_peer_volumes =
                                    sub_selections->connection_selection->get(&con_name);
                                if (selected_peer_volumes != nullptr)
                                {
                                    slct_peer_vlm_iter = std::unique_ptr<VolumeSelectionMap::KeysIterator>(
                                        new VolumeSelectionMap::KeysIterator(*selected_peer_volumes)
                                    );
                                }
                            }
                        }
                        else
                        {
                            peer_vlm_iter = std::unique_ptr<VolumesMap::ValuesIterator>(
                                new VolumesMap::ValuesIterator(std::move(con->volumes_iterator()))
                            );
                            if (!peer_vlm_iter->has_next())
                            {
                                peer_vlm_match = !settings.filter_vlm_number &&
                                                 settings.peer_vlm_op_chain.is_empty() &&
                                                 settings.peer_vlm_quorum_chain.is_empty() &&
                                                 settings.peer_vlm_repl_state_chain.is_empty() &&
                                                 settings.peer_vlm_state_chain.is_empty();
                            }
                        }
                        for (
                            DrbdVolume* peer_vlm = next_peer_volume(con, peer_vlm_iter, slct_peer_vlm_iter);
                            peer_vlm != nullptr;
                            peer_vlm = next_peer_volume(con, peer_vlm_iter, slct_peer_vlm_iter)
                        )
                        {
                            // Apply peer volume filters
                            bool single_peer_vlm_match =
                                !settings.filter_vlm_number ||
                                ((peer_vlm->get_volume_nr() == settings.vlm_number) !=
                                 settings.invert.volume_number);
                            single_peer_vlm_match = single_peer_vlm_match &&
                                                    settings.peer_vlm_op_chain.match(*peer_vlm);
                            single_peer_vlm_match = single_peer_vlm_match &&
                                                    settings.peer_vlm_quorum_chain.match(*peer_vlm);
                            if (single_peer_vlm_match)
                            {
                                single_peer_vlm_match =
                                    settings.peer_vlm_state_chain.match(*peer_vlm) !=
                                    settings.invert.peer_volume_disk_state;
                            }
                            if (single_peer_vlm_match)
                            {
                                single_peer_vlm_match =
                                    settings.peer_vlm_repl_state_chain.match(*peer_vlm) !=
                                    settings.invert.peer_volume_repl_state;
                            }

                            if (single_peer_vlm_match && settings.targets.select_peer_volumes &&
                                !settings.restrictions.selected_peer_volumes)
                            {
                                if (cur_con_slct_node == nullptr)
                                {
                                    if (cur_rsc_sub_selections == nullptr)
                                    {
                                        // Select resource
                                        ResourceSelectionMap::Node* const cur_slct_rsc_node =
                                            dsp_shared.select_resource(rsc_name);
                                        cur_rsc_sub_selections = cur_slct_rsc_node->get_value();
                                    }
                                    // Select parent connection
                                    if (cur_con_name == nullptr)
                                    {
                                        cur_con_name = &(con->get_name());
                                    }
                                    cur_con_slct_node = dsp_shared.select_connection(
                                        *cur_rsc_sub_selections, *cur_con_name
                                    );
                                }
                                const uint16_t peer_vlm_nr = peer_vlm->get_volume_nr();
                                dsp_shared.select_peer_volume(*cur_con_slct_node, peer_vlm_nr);
                            }

                            peer_vlm_match = peer_vlm_match || single_peer_vlm_match;
                        }

                        // Connection no longer matches if none of its peer volumes match
                        single_con_match = peer_vlm_match;
                    }

                    if (single_con_match && settings.targets.select_connections && !eff_rstr_to_slct_con &&
                        (!settings.targets.select_peer_volumes || cur_con_slct_node == nullptr))
                    {
                        if (cur_rsc_sub_selections == nullptr)
                        {
                            // Select resource
                            ResourceSelectionMap::Node* const cur_slct_rsc_node =
                                dsp_shared.select_resource(rsc_name);
                            cur_rsc_sub_selections = cur_slct_rsc_node->get_value();
                        }
                        cur_con_name = &(con->get_name());
                        dsp_shared.select_connection(*cur_rsc_sub_selections, *cur_con_name);
                    }

                    con_match = con_match || single_con_match;
                }

                // Resource no longer matches if none of its connections match
                rsc_match = con_match;
            }

            if (rsc_match)
            {
                if (cur_rsc_sub_selections == nullptr)
                {
                    // Select resource
                    ResourceSelectionMap::Node* const cur_slct_rsc_node = dsp_shared.select_resource(rsc_name);
                    cur_rsc_sub_selections = cur_slct_rsc_node->get_value();
                }
                // Select remembered volumes
                if (vlm_to_slct && vlm_to_slct->get_size() > 0)
                {
                    VolumeSelectionMap::KeysIterator iter(*vlm_to_slct);
                    while (iter.has_next())
                    {
                        const uint16_t* const vlm_nr = iter.next();
                        dsp_shared.select_volume(*cur_rsc_sub_selections, *vlm_nr);
                    }
                }
            }

            // Clean up volumes remembered for select
            if (vlm_to_slct)
            {
                vlm_to_slct->clear();
            }
        }
    }

    // @throws std::bad_alloc, string_matching::PatternLimitException
    void filter_deselect(
        FilterSettings&     settings,
        ResourcesMap&       rsc_map,
        SharedData&         dsp_shared,
        MessageLog&         debug_log
    )
    {
        // Setup name pattern restrictions
        std::unique_ptr<string_matching::PatternItem> rsc_name_pattern;
        if (!settings.rsc_name_pattern.empty())
        {
            string_matching::process_pattern(settings.rsc_name_pattern, rsc_name_pattern);
        }

        std::unique_ptr<string_matching::PatternItem> con_name_pattern;
        if (!settings.con_name_pattern.empty())
        {
            string_matching::process_pattern(settings.con_name_pattern, con_name_pattern);
        }

        // Volumes that match the volume filters and are to be deselected if the resource as a whole matches
        std::unique_ptr<VolumeSelectionMap>     vlm_to_deslct;
        if (settings.targets.select_volumes)
        {
            vlm_to_deslct = std::unique_ptr<VolumeSelectionMap>(
                new VolumeSelectionMap(&comparators::compare<uint16_t>)
            );
        }
        // Connections and peer volumes are the last objects required to match, so they can be deselected immediately

        const bool need_rsc_run_state_match =
            (!(settings.rsc_op_chain.is_empty() && settings.rsc_quorum_chain.is_empty() &&
               settings.rsc_role_chain.is_empty()));
        const bool need_vlm_run_state_match =
            (!(settings.vlm_op_chain.is_empty() && settings.vlm_quorum_chain.is_empty() &&
               settings.vlm_state_chain.is_empty()));
        const bool need_con_run_state_match =
            (!(settings.con_op_chain.is_empty() && settings.con_role_chain.is_empty() &&
               settings.con_state_chain.is_empty()));
        const bool need_peer_vlm_run_state_match =
            (!(settings.peer_vlm_op_chain.is_empty() && settings.peer_vlm_quorum_chain.is_empty() &&
               settings.peer_vlm_state_chain.is_empty() && settings.peer_vlm_repl_state_chain.is_empty()));

        const bool load_rsc_run_state =
            (!(settings.restrictions.selected_volumes && settings.restrictions.selected_connections &&
               settings.restrictions.selected_peer_volumes)) ||
            need_rsc_run_state_match;
        const bool load_con_run_state = !settings.restrictions.selected_peer_volumes || need_con_run_state_match;

        ResourceSelectionMap::NodesIterator slct_rsc_iter(*(dsp_shared.selected_resources));
        while (slct_rsc_iter.has_next())
        {
            ResourceSelectionMap::Node* slct_rsc_node = slct_rsc_iter.next();
            const std::string* const rsc_name = slct_rsc_node->get_key();
            ResourceSubSelections& sub_selections = *(slct_rsc_node->get_value());

            DrbdResource* rsc = nullptr;
            // If any sub-resource-objects iterations are required, point rsc to the resource
            // (will be nullptr if the resource is not online)
            if (load_rsc_run_state)
            {
                rsc = rsc_map.get(rsc_name);
            }

            bool rsc_match = true;
            if (rsc_name_pattern)
            {
                rsc_match = string_matching::match_text(*rsc_name, rsc_name_pattern.get()) !=
                            settings.invert.resource_name;
            }

            if (rsc_match)
            {
                if (need_rsc_run_state_match)
                {
                    // Resource run state is required to apply some of the filters
                    if (rsc != nullptr)
                    {
                        rsc_match = settings.rsc_op_chain.match(*rsc);
                        rsc_match = rsc_match && settings.rsc_quorum_chain.match(*rsc);
                        rsc_match = rsc_match && settings.rsc_role_chain.match(*rsc);
                    }
                }

                if (rsc_match)
                {
                    // Filter volumes or selected volumes
                    std::unique_ptr<VolumesMap::ValuesIterator>         vlm_iter;
                    std::unique_ptr<VolumeSelectionMap::KeysIterator>   slct_vlm_iter;
                    if (settings.restrictions.selected_volumes || rsc == nullptr)
                    {
                        if (sub_selections.volume_selection)
                        {
                            slct_vlm_iter = std::unique_ptr<VolumeSelectionMap::KeysIterator>(
                                new VolumeSelectionMap::KeysIterator(*(sub_selections.volume_selection))
                            );
                        }
                    }
                    else
                    {
                        vlm_iter = std::unique_ptr<VolumesMap::ValuesIterator>(
                            new VolumesMap::ValuesIterator(std::move(rsc->volumes_iterator()))
                        );
                    }
                    bool vlm_match = false;
                    const uint16_t* vlm_nr = nullptr;
                    DrbdVolume* vlm = nullptr;
                    for (
                        next_volume_for_deselect(rsc, vlm_iter, slct_vlm_iter, vlm_nr, vlm);
                        vlm_nr != nullptr;
                        next_volume_for_deselect(rsc, vlm_iter, slct_vlm_iter, vlm_nr, vlm)
                    )
                    {
                        bool single_vlm_match = !settings.filter_vlm_number ||
                            ((*vlm_nr == settings.vlm_number) != settings.invert.volume_number) ;
                        if (single_vlm_match)
                        {
                            if (need_vlm_run_state_match)
                            {
                                // Volume run state is required to apply some of the filters
                                if (vlm == nullptr)
                                {
                                    if (rsc == nullptr)
                                    {
                                        rsc = rsc_map.get(rsc_name);
                                    }

                                    if (rsc != nullptr)
                                    {
                                        vlm = rsc->get_volume(*vlm_nr);
                                    }
                                }

                                if (vlm != nullptr)
                                {
                                    single_vlm_match = settings.vlm_op_chain.match(*vlm);
                                    single_vlm_match = single_vlm_match && settings.vlm_quorum_chain.match(*vlm);
                                    single_vlm_match = single_vlm_match &&
                                        (settings.vlm_state_chain.match(*vlm) != settings.invert.volume_state);
                                }
                            }

                            if (single_vlm_match)
                            {
                                // Remember the volume for deselection if the resource as a whole matches
                                if (vlm_to_deslct)
                                {
                                    try
                                    {
                                        vlm_to_deslct->insert(vlm_nr, nullptr);
                                    }
                                    catch (dsaext::DuplicateInsertException&)
                                    {
                                        std::string debug_msg("DuplicateInsertException in filter_deselect, "
                                                              "remembered volumes for deselection");
                                        debug_log.add_entry(MessageLog::log_level::WARN, debug_msg);
                                    }
                                }
                            }

                            vlm_match = vlm_match || single_vlm_match;
                        }

                        rsc_match = vlm_match;
                    }
                }

                if (rsc_match)
                {
                    // Filter connections or selected connections
                    std::unique_ptr<ConnectionsMap::ValuesIterator>         con_iter;
                    std::unique_ptr<ConnectionSelectionMap::KeysIterator>   slct_con_iter;
                    if (settings.restrictions.selected_connections || rsc == nullptr)
                    {
                        if (sub_selections.connection_selection)
                        {
                            slct_con_iter = std::unique_ptr<ConnectionSelectionMap::KeysIterator>(
                                new ConnectionSelectionMap::KeysIterator(*(sub_selections.connection_selection))
                            );
                        }
                    }
                    else
                    {
                        con_iter = std::unique_ptr<ConnectionsMap::ValuesIterator>(
                            new ConnectionsMap::ValuesIterator(std::move(rsc->connections_iterator()))
                        );
                    }
                    bool con_match = false;
                    const std::string* con_name = nullptr;
                    DrbdConnection* con = nullptr;
                    for (
                        next_connection_for_deselect(rsc, con_iter, slct_con_iter, con_name, con);
                        con_name != nullptr;
                        next_connection_for_deselect(rsc, con_iter, slct_con_iter, con_name, con)
                    )
                    {
                        if (load_con_run_state)
                        {
                            if (con == nullptr)
                            {
                                if (rsc == nullptr)
                                {
                                    rsc = rsc_map.get(rsc_name);
                                }

                                if (rsc != nullptr)
                                {
                                    con = rsc->get_connection(*con_name);
                                }
                            }
                        }

                        bool single_con_match = true;
                        if (con_name_pattern)
                        {
                            single_con_match = (string_matching::match_text(*con_name, con_name_pattern.get()) !=
                                                settings.invert.connection_name);
                        }

                        if (single_con_match)
                        {
                            if (need_con_run_state_match)
                            {
                                // Connection run state is required to apply some of the filters
                                if (con != nullptr)
                                {
                                    single_con_match = settings.con_op_chain.match(*con);
                                    single_con_match = single_con_match && settings.con_role_chain.match(*con);
                                    single_con_match = single_con_match &&
                                        (settings.con_state_chain.match(*con) != settings.invert.connection_state);
                                }
                            }
                        }

                        if (single_con_match)
                        {
                            // Begin peer volume filterting

                            // Filter peer volumes or selected peer volumes
                            std::unique_ptr<VolumesMap::ValuesIterator>         peer_vlm_iter;
                            std::unique_ptr<VolumeSelectionMap::KeysIterator>   slct_peer_vlm_iter;
                            if (settings.restrictions.selected_peer_volumes || con == nullptr)
                            {
                                if (sub_selections.connection_selection)
                                {
                                    VolumeSelectionMap* const selected_peer_volumes =
                                        sub_selections.connection_selection->get(con_name);
                                    if (selected_peer_volumes != nullptr)
                                    {
                                        slct_peer_vlm_iter = std::unique_ptr<VolumeSelectionMap::KeysIterator>(
                                            new VolumeSelectionMap::KeysIterator(*selected_peer_volumes)
                                        );
                                    }
                                }
                            }
                            else
                            {
                                peer_vlm_iter = std::unique_ptr<VolumesMap::ValuesIterator>(
                                    new VolumesMap::ValuesIterator(std::move(con->volumes_iterator()))
                                );
                            }
                            bool peer_vlm_match = false;
                            const uint16_t* peer_vlm_nr = nullptr;
                            DrbdVolume* peer_vlm = nullptr;
                            for (
                                next_peer_volume_for_deselect(
                                    con, peer_vlm_iter, slct_peer_vlm_iter, peer_vlm_nr, peer_vlm
                                );
                                peer_vlm_nr != nullptr;
                                next_peer_volume_for_deselect(
                                    con, peer_vlm_iter, slct_peer_vlm_iter, peer_vlm_nr, peer_vlm
                                )
                            )
                            {
                                bool single_peer_vlm_match =
                                    !settings.filter_vlm_number ||
                                    ((*peer_vlm_nr == settings.vlm_number) != settings.invert.volume_number);
                                if (single_peer_vlm_match)
                                {
                                    if (need_peer_vlm_run_state_match)
                                    {
                                        // Peer volume run state is required to apply some of the filters
                                        if (peer_vlm == nullptr)
                                        {
                                            if (con == nullptr)
                                            {
                                                if (rsc == nullptr)
                                                {
                                                    rsc = rsc_map.get(rsc_name);
                                                }

                                                if (rsc != nullptr)
                                                {
                                                    con = rsc->get_connection(*con_name);
                                                }
                                            }

                                            if (con != nullptr)
                                            {
                                                peer_vlm = con->get_volume(*peer_vlm_nr);
                                            }
                                        }

                                        if (peer_vlm != nullptr)
                                        {
                                            single_peer_vlm_match = settings.peer_vlm_op_chain.match(*peer_vlm);
                                            single_peer_vlm_match = single_peer_vlm_match &&
                                                settings.peer_vlm_quorum_chain.match(*peer_vlm);
                                            single_peer_vlm_match = single_peer_vlm_match &&
                                                (settings.peer_vlm_state_chain.match(*peer_vlm) !=
                                                 settings.invert.peer_volume_disk_state);
                                            single_peer_vlm_match = single_peer_vlm_match &&
                                                (settings.peer_vlm_repl_state_chain.match(*peer_vlm) !=
                                                 settings.invert.peer_volume_repl_state);
                                        }
                                    }
                                }

                                if (!settings.targets.select_connections && single_peer_vlm_match &&
                                    settings.targets.select_peer_volumes)
                                {
                                    dsp_shared.deselect_peer_volume(*rsc_name, *con_name, *peer_vlm_nr);
                                }

                                peer_vlm_match = peer_vlm_match || single_peer_vlm_match;
                            }

                            single_con_match = single_con_match && peer_vlm_match;

                            if (!settings.targets.select_resources && single_con_match &&
                                settings.targets.select_connections)
                            {
                                dsp_shared.deselect_connection(sub_selections, *con_name);
                            }
                            // End peer volume filterting
                        }

                        con_match = con_match || single_con_match;
                    }

                    rsc_match = con_match;
                }

                if (rsc_match)
                {
                    if (!settings.targets.select_resources && vlm_to_deslct && vlm_to_deslct->get_size() > 0)
                    {
                        VolumeSelectionMap::KeysIterator iter(*vlm_to_deslct);
                        while (iter.has_next())
                        {
                            const uint16_t* const vlm_nr = iter.next();
                            dsp_shared.deselect_volume(sub_selections, *vlm_nr);
                        }
                    }

                    if (settings.targets.select_resources)
                    {
                        dsp_shared.deselect_resource(*rsc_name);
                    }
                }
            }

            // Clean up volumes remembered for deselect
            if (vlm_to_deslct)
            {
                vlm_to_deslct->clear();
            }
        }
    }

    DrbdResource* next_resource(
        ResourcesMap&                                               rsc_map,
        const std::unique_ptr<ResourcesMap::ValuesIterator>&        rsc_iter,
        const std::unique_ptr<ResourceSelectionMap::KeysIterator>&  slct_rsc_iter
    )
    {
        DrbdResource* rsc = nullptr;
        if (slct_rsc_iter)
        {
            const std::string* rsc_name = nullptr;
            do
            {
                rsc_name = slct_rsc_iter->next();
                if (rsc_name != nullptr)
                {
                    rsc = rsc_map.get(rsc_name);
                }
            }
            while (rsc_name != nullptr && rsc == nullptr);
        }
        else
        if (rsc_iter)
        {
            rsc = rsc_iter->next();
        }
        return rsc;
    }

    DrbdVolume* next_volume(
        DrbdResource* const rsc,
        const std::unique_ptr<VolumesMap::ValuesIterator>&          vlm_iter,
        const std::unique_ptr<VolumeSelectionMap::KeysIterator>&    slct_vlm_iter
    )
    {
        DrbdVolume* vlm = nullptr;
        if (slct_vlm_iter)
        {
            const uint16_t* vlm_nr = nullptr;
            do
            {
                vlm_nr = slct_vlm_iter->next();
                if (vlm_nr != nullptr)
                {
                    vlm = rsc->get_volume(*vlm_nr);
                }
            }
            while (vlm_nr != nullptr && vlm == nullptr);
        }
        else
        if (vlm_iter)
        {
            vlm = vlm_iter->next();
        }
        return vlm;
    }

    DrbdConnection* next_connection(
        DrbdResource* const rsc,
        const std::unique_ptr<ConnectionsMap::ValuesIterator>&          con_iter,
        const std::unique_ptr<ConnectionSelectionMap::KeysIterator>&    slct_con_iter
    )
    {
        DrbdConnection* con = nullptr;
        if (slct_con_iter)
        {
            const std::string* con_name = nullptr;
            do
            {
                con_name = slct_con_iter->next();
                if (con_name != nullptr)
                {
                    con = rsc->get_connection(*con_name);
                }
            }
            while (con_name != nullptr && con == nullptr);
        }
        else
        {
            con = con_iter->next();
        }
        return con;
    }

    DrbdVolume* next_peer_volume(
        DrbdConnection* const con,
        const std::unique_ptr<VolumesMap::ValuesIterator>&          peer_vlm_iter,
        const std::unique_ptr<VolumeSelectionMap::KeysIterator>&    slct_peer_vlm_iter
    )
    {
        DrbdVolume* peer_vlm = nullptr;
        if (slct_peer_vlm_iter)
        {
            const uint16_t* peer_vlm_nr = nullptr;
            do
            {
                peer_vlm_nr = slct_peer_vlm_iter->next();
                if (peer_vlm_nr != nullptr)
                {
                    peer_vlm = con->get_volume(*peer_vlm_nr);
                }
            }
            while (peer_vlm_nr != nullptr && peer_vlm == nullptr);
        }
        else
        {
            peer_vlm = peer_vlm_iter->next();
        }
        return peer_vlm;
    }

    // If iterating selected volumes, sets vlm_nr to point to the volume number of the next selected volume,
    // and sets vlm to nullptr, otherwise, if iterating all volumes, sets vlm_nr to point to the volume number
    // of the next volume, and sets vlm to point to the next volume object.
    // If given no iterators, or if no more elements are available for iteration, both, vlm and vlm_nr, are
    // set to nullptr.
    void next_volume_for_deselect(
        DrbdResource* const rsc,
        const std::unique_ptr<VolumesMap::ValuesIterator>&          vlm_iter,
        const std::unique_ptr<VolumeSelectionMap::KeysIterator>&    slct_vlm_iter,
        const uint16_t*&                                            vlm_nr,
        DrbdVolume*&                                                vlm
    )
    {
        if (slct_vlm_iter)
        {
            vlm = nullptr;
            vlm_nr = slct_vlm_iter->next();
        }
        else
        if (vlm_iter)
        {
            vlm = vlm_iter->next();
            if (vlm != nullptr)
            {
                vlm_nr = &(vlm->get_volume_nr_ref());
            }
            else
            {
                vlm_nr = nullptr;
            }
        }
        else
        {
            vlm = nullptr;
            vlm_nr = nullptr;
        }
    }

    // If iterating selected connections, sets con_name to point to the connection name of the next selected
    // connection, and sets con to nullptr, otherwise, if iterating all connections, sets con_name to point
    // to the connection name of the next connection, and sets con to point to the next connection object.
    // If given no iterators, or if no more elements are available for iteration, both, con and con_name, are
    // set to nullptr.
    void next_connection_for_deselect(
        DrbdResource* const rsc,
        const std::unique_ptr<ConnectionsMap::ValuesIterator>&          con_iter,
        const std::unique_ptr<ConnectionSelectionMap::KeysIterator>&    slct_con_iter,
        const std::string*&                                             con_name,
        DrbdConnection*&                                                con
    )
    {
        if (slct_con_iter)
        {
            con = nullptr;
            con_name = slct_con_iter->next();
        }
        else
        if (con_iter)
        {
            con = con_iter->next();
            if (con != nullptr)
            {
                con_name = &(con->get_name());
            }
            else
            {
                con_name = nullptr;
            }
        }
        else
        {
            con = nullptr;
            con_name = nullptr;
        }
    }

    // If iterating selected peer volumes, sets peer_vlm_nr to point to the volume number of the next selected
    // peer volume, and sets peer_vlm to nullptr, otherwise, if iterating all peer volumes, sets peer_vlm_nr to
    // point to the volume number of the next peer volume, and sets peer_vlm to point to the next peer volume object.
    // If given no iterators, or if no more elements are available for iteration, both, peer_vlm and peer_vlm_nr, are
    // set to nullptr.
    void next_peer_volume_for_deselect(
        DrbdConnection* const con,
        const std::unique_ptr<VolumesMap::ValuesIterator>&          peer_vlm_iter,
        const std::unique_ptr<VolumeSelectionMap::KeysIterator>&    slct_peer_vlm_iter,
        const uint16_t*&                                            peer_vlm_nr,
        DrbdVolume*&                                                peer_vlm
    )
    {
        if (slct_peer_vlm_iter)
        {
            peer_vlm = nullptr;
            peer_vlm_nr = slct_peer_vlm_iter->next();
        }
        else
        if (peer_vlm_iter)
        {
            peer_vlm = peer_vlm_iter->next();
            if (peer_vlm != nullptr)
            {
                peer_vlm_nr = &(peer_vlm->get_volume_nr_ref());
            }
            else
            {
                peer_vlm_nr = nullptr;
            }
        }
        else
        {
            peer_vlm = nullptr;
            peer_vlm_nr = nullptr;
        }
    }
}
