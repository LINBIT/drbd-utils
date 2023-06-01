#include <objects/ResourceDirectory.h>

#include <comparators.h>
#include <exceptions.h>

// @throws std::bad_alloc
ResourceDirectory::ResourceDirectory(
    MessageLog& log_ref,
    MessageLog& debug_log_ref
):
    log(log_ref),
    debug_log(debug_log_ref)
{
    rsc_map     = std::unique_ptr<ResourcesMap>(new ResourcesMap(&comparators::compare_string));
    prb_rsc_map = std::unique_ptr<ResourcesMap>(new ResourcesMap(&comparators::compare_string));
}

ResourceDirectory::~ResourceDirectory() noexcept
{
    // Cleanup resources map
    {
        ResourcesMap::NodesIterator dtor_iter(*rsc_map);
        // Free all DrbdResource mappings
        prb_rsc_map->clear();
        while (dtor_iter.has_next())
        {
            ResourcesMap::Node* node = dtor_iter.next();
            delete node->get_key();
            delete node->get_value();
        }
        rsc_map->clear();
    }
}

ResourcesMap& ResourceDirectory::get_resources_map() noexcept
{
    return *rsc_map;
}

ResourcesMap& ResourceDirectory::get_problem_resources_map() noexcept
{
    return *prb_rsc_map;
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void ResourceDirectory::create_connection(PropsMap& event_props, const std::string& event_line)
{
    try
    {
        ResourcesMap::Node& rsc_node = get_resource_node(event_props, event_line);
        std::string& rsc_key = *(rsc_node.get_key());
        DrbdResource& rsc = *(rsc_node.get_value());

        std::unique_ptr<DrbdConnection> conn(DrbdConnection::new_from_props(event_props));
        conn->update(event_props);
        static_cast<void> (conn->update_state_flags());
        rsc.add_connection(conn.get());
        StateFlags::state rsc_last_state = rsc.get_state();
        StateFlags::state rsc_new_state = rsc.child_state_flags_changed();
        problem_resources_update(rsc_key, rsc, rsc_last_state, rsc_new_state);
        static_cast<void> (conn.release());
    }
    catch (dsaext::DuplicateInsertException& dup_exc)
    {
        std::string error_msg("Invalid DRBD event: Duplicate connection creation");
        std::string debug_info("Duplicate connection creation");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void ResourceDirectory::create_device(PropsMap& event_props, const std::string& event_line)
{
    try
    {
        ResourcesMap::Node& rsc_node = get_resource_node(event_props, event_line);
        std::string& rsc_key = *(rsc_node.get_key());
        DrbdResource& rsc = *(rsc_node.get_value());

        std::unique_ptr<DrbdVolume> vol(DrbdVolume::new_from_props(event_props));
        vol->update(event_props);
        static_cast<void> (vol->update_state_flags());
        rsc.add_volume(vol.get());
        StateFlags::state rsc_last_state = rsc.get_state();
        StateFlags::state rsc_new_state = rsc.child_state_flags_changed();
        problem_resources_update(rsc_key, rsc, rsc_last_state, rsc_new_state);
        static_cast<void> (vol.release());
    }
    catch (dsaext::DuplicateInsertException& dup_exc)
    {
        std::string error_msg("Invalid DRBD event: Duplicate volume creation");
        std::string debug_info("Duplicate device (volume) creation");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void ResourceDirectory::create_peer_device(PropsMap& event_props, const std::string& event_line)
{
    try
    {
        ResourcesMap::Node& rsc_node = get_resource_node(event_props, event_line);
        std::string& rsc_key = *(rsc_node.get_key());
        DrbdResource& rsc = *(rsc_node.get_value());
        DrbdConnection& conn = get_connection(rsc, event_props, event_line);

        std::unique_ptr<DrbdVolume> vol(DrbdVolume::new_from_props(event_props));
        vol->update(event_props);
        vol->set_connection(&conn);
        static_cast<void> (vol->update_state_flags());
        conn.add_volume(vol.get());
        static_cast<void> (conn.child_state_flags_changed());
        StateFlags::state rsc_last_state = rsc.get_state();
        StateFlags::state rsc_new_state = rsc.child_state_flags_changed();
        problem_resources_update(rsc_key, rsc, rsc_last_state, rsc_new_state);
        static_cast<void> (vol.release());
    }
    catch (dsaext::DuplicateInsertException& dup_exc)
    {
        std::string error_msg("Invalid DRBD event: Duplicate peer volume creation");
        std::string debug_info("Duplicate peer device (peer volume) creation");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
}

// @throws std::bad_alloc, EventMessageException
void ResourceDirectory::create_resource(PropsMap& event_props, const std::string& event_line)
{
    try
    {
        std::unique_ptr<DrbdResource> rsc_mgr(DrbdResource::new_from_props(event_props));
        DrbdResource* const rsc = rsc_mgr.get();

        rsc->update(event_props);
        static_cast<void> (rsc->update_state_flags());

        std::unique_ptr<std::string> rsc_name_mgr(new std::string(rsc->get_name()));
        const std::string* const rsc_key = rsc_name_mgr.get();

        rsc_map->insert(rsc_key, rsc);
        if (rsc->has_mark_state() && prb_rsc_map->get(rsc_key) == nullptr)
        {
            prb_rsc_map->insert(rsc_key, rsc);
        }
        static_cast<void> (rsc_mgr.release());
        static_cast<void> (rsc_name_mgr.release());
    }
    catch (dsaext::DuplicateInsertException& dup_exc)
    {
        std::string error_msg("Invalid DRBD event: Duplicate resource creation");
        std::string debug_info("Duplicate resource creation");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void ResourceDirectory::update_connection(PropsMap& event_props, const std::string& event_line)
{
    ResourcesMap::Node& rsc_node = get_resource_node(event_props, event_line);
    std::string& rsc_key = *(rsc_node.get_key());
    DrbdResource& rsc = *(rsc_node.get_value());
    DrbdConnection& conn = get_connection(rsc, event_props, event_line);

    conn.update(event_props);

    // Adjust connection state flags
    StateFlags::state conn_last_state = conn.get_state();
    StateFlags::state conn_new_state = conn.update_state_flags();
    if (conn_last_state != conn_new_state &&
        (conn_last_state == StateFlags::state::NORM || conn_new_state == StateFlags::state::NORM))
    {
        // Connection state flags changed, adjust resource state flags
        StateFlags::state rsc_last_state = rsc.get_state();
        StateFlags::state rsc_new_state = rsc.child_state_flags_changed();
        problem_resources_update(rsc_key, rsc, rsc_last_state, rsc_new_state);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void ResourceDirectory::update_device(PropsMap& event_props, const std::string& event_line)
{
    ResourcesMap::Node& rsc_node = get_resource_node(event_props, event_line);
    std::string& rsc_key = *(rsc_node.get_key());
    DrbdResource& rsc = *(rsc_node.get_value());
    DrbdVolume& vol = get_device(dynamic_cast<VolumesContainer&> (rsc), event_props, event_line);

    vol.update(event_props);

    // Adjust volume state flags
    static_cast<void> (vol.update_state_flags());

    // Volume state flags changed, adjust resource state flags
    StateFlags::state rsc_last_state = rsc.get_state();
    StateFlags::state rsc_new_state = rsc.child_state_flags_changed();
    problem_resources_update(rsc_key, rsc, rsc_last_state, rsc_new_state);
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void ResourceDirectory::update_peer_device(PropsMap& event_props, const std::string& event_line)
{
    ResourcesMap::Node& rsc_node = get_resource_node(event_props, event_line);
    std::string& rsc_key = *(rsc_node.get_key());
    DrbdResource& rsc = *(rsc_node.get_value());
    DrbdConnection& conn = get_connection(rsc, event_props, event_line);
    DrbdVolume& vol = get_device(dynamic_cast<VolumesContainer&> (conn), event_props, event_line);

    vol.update(event_props);

    // Adjust volume state flags
    StateFlags::state vol_last_state = vol.get_state();
    StateFlags::state vol_new_state = vol.update_state_flags();
    if (vol_last_state != vol_new_state &&
        (vol_last_state == StateFlags::state::NORM || vol_new_state == StateFlags::state::NORM))
    {
        // Volume state flags changed, adjust connection state flags
        StateFlags::state conn_last_state = conn.get_state();
        StateFlags::state conn_new_state = conn.child_state_flags_changed();
        if (conn_last_state != conn_new_state &&
            (conn_last_state == StateFlags::state::NORM || conn_new_state == StateFlags::state::NORM))
        {
            // Connection state flags changed, adjust resource state flags
            StateFlags::state rsc_last_state = rsc.get_state();
            StateFlags::state rsc_new_state = rsc.child_state_flags_changed();
            problem_resources_update(rsc_key, rsc, rsc_last_state, rsc_new_state);
        }
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void ResourceDirectory::update_resource(PropsMap& event_props, const std::string& event_line)
{
    ResourcesMap::Node& rsc_node = get_resource_node(event_props, event_line);
    std::string& rsc_key = *(rsc_node.get_key());
    DrbdResource& rsc = *(rsc_node.get_value());

    rsc.update(event_props);

    StateFlags::state rsc_last_state = rsc.get_state();
    StateFlags::state rsc_new_state = rsc.update_state_flags();
    problem_resources_update(rsc_key, rsc, rsc_last_state, rsc_new_state);
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void ResourceDirectory::rename_resource(PropsMap& event_props, const std::string& event_line)
{
    bool is_problem_resource = false;
    std::unique_ptr<DrbdResource> rsc_obj;
    {
        const std::string& evt_rsc_name = lookup_resource_name(event_props, event_line);
        ResourcesMap::Node* const node = rsc_map->get_node(&evt_rsc_name);
        if (node == nullptr)
        {
            std::string error_msg("Non-existent resource referenced by the DRBD events source");
            std::string debug_info("Received a DRBD rename event for a non-existent resource");
            throw EventObjectException(&error_msg, &debug_info, &event_line);
        }

        // Get the lookup key for the resources and problem resources maps
        const std::string* const rsc_key = node->get_key();
        ResourcesMap::Node* const prb_node = prb_rsc_map->get_node(rsc_key);
        if (prb_node != nullptr)
        {
            is_problem_resource = true;
            prb_rsc_map->remove_node(prb_node);
        }
        prb_rsc_map->remove(rsc_key);
        // Extract the resource object
        rsc_obj = std::unique_ptr<DrbdResource>(node->get_value());
        // Remove the entry for that resource
        rsc_map->remove_node(node);
        // Deallocate the lookup key (the resource's name)
        delete rsc_key;
    }

    try
    {
        // Change the resource's name
        rsc_obj->rename(event_props);

        // Allocate a new lookup key with resource's new name
        std::unique_ptr<std::string> new_rsc_name(new std::string(rsc_obj->get_name()));
        // Reinsert the entries for that resource
        const std::string* const new_rsc_name_ptr = new_rsc_name.get();
        DrbdResource* const rsc_obj_ptr = rsc_obj.get();
        rsc_map->insert(new_rsc_name_ptr, rsc_obj_ptr);
        if (is_problem_resource)
        {
            prb_rsc_map->insert(new_rsc_name_ptr, rsc_obj_ptr);
        }

        new_rsc_name.release();
        rsc_obj.release();
    }
    catch (dsaext::DuplicateInsertException& dup_exc)
    {
        std::string error_msg("Invalid DRBD 'rename' event: Duplicate resource entry");
        std::string debug_info("'rename' event: Duplicate resource entry:");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void ResourceDirectory::destroy_connection(PropsMap& event_props, const std::string& event_line)
{
    ResourcesMap::Node& rsc_node = get_resource_node(event_props, event_line);
    std::string& rsc_key = *(rsc_node.get_key());
    DrbdResource& rsc = *(rsc_node.get_value());

    bool conn_marked = false;
    {
        DrbdConnection& conn = get_connection(rsc, event_props, event_line);
        conn_marked = conn.has_mark_state();
        rsc.remove_connection(conn.get_name());
    }

    if (conn_marked)
    {
        StateFlags::state rsc_last_state = rsc.get_state();
        StateFlags::state rsc_new_state = rsc.child_state_flags_changed();
        problem_resources_update(rsc_key, rsc, rsc_last_state, rsc_new_state);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void ResourceDirectory::destroy_device(PropsMap& event_props, const std::string& event_line)
{
    ResourcesMap::Node& rsc_node = get_resource_node(event_props, event_line);
    std::string& rsc_key = *(rsc_node.get_key());
    DrbdResource& rsc = *(rsc_node.get_value());

    bool vol_marked = false;
    {
        DrbdVolume& vol = get_device(dynamic_cast<VolumesContainer&> (rsc), event_props, event_line);
        vol_marked = vol.has_mark_state();
        rsc.remove_volume(vol.get_volume_nr());
    }

    if (vol_marked)
    {
        StateFlags::state rsc_last_state = rsc.get_state();
        StateFlags::state rsc_new_state = rsc.child_state_flags_changed();
        problem_resources_update(rsc_key, rsc, rsc_last_state, rsc_new_state);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void ResourceDirectory::destroy_peer_device(PropsMap& event_props, const std::string& event_line)
{
    ResourcesMap::Node& rsc_node = get_resource_node(event_props, event_line);
    std::string& rsc_key = *(rsc_node.get_key());
    DrbdResource& rsc = *(rsc_node.get_value());

    DrbdConnection& conn = get_connection(rsc, event_props, event_line);
    bool peer_vol_marked = false;
    {
        // May report non-existing volume if required
        DrbdVolume& peer_vol =  get_device(dynamic_cast<VolumesContainer&> (conn), event_props, event_line);
        peer_vol_marked = peer_vol.has_mark_state();
    }

    std::string* vol_nr_str = event_props.get(&DrbdVolume::PROP_KEY_VOL_NR);
    if (vol_nr_str != nullptr)
    {
        try
        {
            uint16_t vol_nr = DrbdVolume::parse_volume_nr(*vol_nr_str);
            conn.remove_volume(vol_nr);
            if (peer_vol_marked)
            {
                static_cast<void> (conn.child_state_flags_changed());
                StateFlags::state rsc_last_state = rsc.get_state();
                StateFlags::state rsc_new_state = rsc.child_state_flags_changed();
                problem_resources_update(rsc_key, rsc, rsc_last_state, rsc_new_state);
            }
        }
        catch (dsaext::NumberFormatException&)
        {
            std::string error_msg("Invalid DRBD event: Unparsable volume number");
            std::string debug_info("Unparsable volume number");
            throw EventMessageException(&error_msg, &debug_info, &event_line);
        }
    }
    else
    {
        std::string error_msg("Invalid DRBD event: Missing volume number");
        std::string debug_info("Missing volume number");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void ResourceDirectory::destroy_resource(PropsMap& event_props, const std::string& event_line)
{
    std::string* const rsc_name = event_props.get(&DrbdResource::PROP_KEY_RES_NAME);
    if (rsc_name != nullptr)
    {
        ResourcesMap::Node* node = rsc_map->get_node(rsc_name);
        if (node != nullptr)
        {
            prb_rsc_map->remove(rsc_name);
            delete node->get_key();
            delete node->get_value();
            rsc_map->remove_node(node);
        }
        else
        {
            std::string error_msg("DRBD event line references a non-existent resource");
            std::string debug_info("DRBD event line references a non-existent resource");
            throw EventObjectException(&error_msg, &debug_info, &event_line);
        }
    }
    else
    {
        std::string error_msg("Received DRBD event line does not contain a resource name");
        std::string debug_info("DRBD event line contains no resource name");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
DrbdConnection& ResourceDirectory::get_connection(
    DrbdResource&       rsc,
    PropsMap&           event_props,
    const std::string&  event_line
)
{
    DrbdConnection* conn {nullptr};
    std::string* conn_name = event_props.get(&DrbdConnection::PROP_KEY_CONN_NAME);
    if (conn_name != nullptr)
    {
        conn = rsc.get_connection(*conn_name);
    }
    else
    {
        std::string error_msg("Invalid DRBD event: Missing connection information");
        std::string debug_info("Missing 'connection' field");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
    if (conn == nullptr)
    {
        std::string error_msg("Non-existent connection referenced by the DRBD events source");
        std::string debug_info("DRBD event line references a non-existent connection");
        throw EventObjectException(&error_msg, &debug_info, &event_line);
    }
    return *conn;
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
DrbdVolume& ResourceDirectory::get_device(
    VolumesContainer&   vol_con,
    PropsMap&           event_props,
    const std::string&  event_line
)
{
    DrbdVolume* vol {nullptr};
    std::string* vol_nr_str = event_props.get(&DrbdVolume::PROP_KEY_VOL_NR);
    if (vol_nr_str != nullptr)
    {
        try
        {
            uint16_t vol_nr = DrbdVolume::parse_volume_nr(*vol_nr_str);
            vol = vol_con.get_volume(vol_nr);
        }
        catch (dsaext::NumberFormatException&)
        {
            std::string error_msg("Invalid DRBD event: Invalid volume number");
            std::string debug_info("Unparsable volume number");
            throw EventMessageException(&error_msg, &debug_info, &event_line);
        }
    }
    else
    {
        std::string error_msg("Invalid DRBD event: Missing volume number");
        std::string debug_info("Missing volume number");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
    if (vol == nullptr)
    {
        std::string error_msg("Non-existent volume id referenced by the DRBD events source");
        std::string debug_info("DRBD event line references a non-existent volume id");
        throw EventObjectException(&error_msg, &debug_info, &event_line);
    }
    return *vol;
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
ResourcesMap::Node& ResourceDirectory::get_resource_node(PropsMap& event_props, const std::string& event_line)
{
    const std::string& rsc_name = lookup_resource_name(event_props, event_line);
    ResourcesMap::Node* const rsc_node = rsc_map->get_node(&rsc_name);
    if (rsc_node == nullptr)
    {
        std::string error_msg("Non-existent resource referenced by the DRBD events source");
        std::string debug_info("DRBD event line references a non-existent resource");
        throw EventObjectException(&error_msg, &debug_info, &event_line);
    }
    return *rsc_node;
}

// @throws EventMessageException
const std::string& ResourceDirectory::lookup_resource_name(PropsMap& event_props, const std::string& event_line)
{
    const std::string* const rsc_name = event_props.get(&DrbdResource::PROP_KEY_RES_NAME);
    if (rsc_name == nullptr)
    {
        std::string error_msg("Received DRBD event line does not contain a resource name");
        std::string debug_info("DRBD event line contains no resource name");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
    return *rsc_name;
}

// @throws std::bad_alloc
void ResourceDirectory::problem_resources_update(
    std::string&        rsc_key,
    DrbdResource&       rsc,
    StateFlags::state   rsc_last_state,
    StateFlags::state   rsc_new_state
)
{
    if (rsc_last_state == StateFlags::state::NORM && rsc_new_state != StateFlags::state::NORM)
    {
        if (prb_rsc_map->get(&rsc_key) == nullptr)
        {
            prb_rsc_map->insert(&rsc_key, &rsc);
        }
    }
    else
    if (rsc_last_state != StateFlags::state::NORM && rsc_new_state == StateFlags::state::NORM)
    {
        prb_rsc_map->remove(&rsc_key);
    }
}

uint32_t ResourceDirectory::get_problem_count() const
{
    return static_cast<uint32_t> (prb_rsc_map->get_size());
}
