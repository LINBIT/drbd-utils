#include <terminal/SharedData.h>
#include <comparators.h>

SharedData::SharedData()
{
    selected_resources = std::unique_ptr<ResourceSelectionMap>(
        new ResourceSelectionMap(&comparators::compare_string)
    );

    selected_actq_entries = std::unique_ptr<TaskEntryMap>(new TaskEntryMap(&comparators::compare<uint64_t>));
    selected_pndq_entries = std::unique_ptr<TaskEntryMap>(new TaskEntryMap(&comparators::compare<uint64_t>));
    selected_sspq_entries = std::unique_ptr<TaskEntryMap>(new TaskEntryMap(&comparators::compare<uint64_t>));
    selected_finq_entries = std::unique_ptr<TaskEntryMap>(new TaskEntryMap(&comparators::compare<uint64_t>));

    selected_log_entries = std::unique_ptr<MessageMap>(new MessageMap(&comparators::compare<uint64_t>));
    selected_debug_log_entries = std::unique_ptr<MessageMap>(new MessageMap(&comparators::compare<uint64_t>));
}

SharedData::~SharedData() noexcept
{
    clear_resources_selection_impl();

    generic_id_clear_selection(*selected_actq_entries);
    generic_id_clear_selection(*selected_pndq_entries);
    generic_id_clear_selection(*selected_sspq_entries);
    generic_id_clear_selection(*selected_finq_entries);

    generic_id_clear_selection(*selected_log_entries);
    generic_id_clear_selection(*selected_debug_log_entries);
}

void SharedData::update_monitor_rsc(const std::string& rsc_name)
{
    if (rsc_name != monitor_rsc)
    {
        monitor_rsc = rsc_name;
        monitor_con.clear();
        monitor_vlm = DisplayConsts::VLM_NONE;
        monitor_peer_vlm = DisplayConsts::VLM_NONE;
    }
}

void SharedData::update_monitor_con(const std::string& con_name)
{
    if (con_name != monitor_con)
    {
        monitor_con = con_name;
        // The peer volumes are supposed to be the same for each connection,
        // therefore, the cursor and selection are not reset
    }
}

void SharedData::update_monitor_vlm(const uint16_t vlm_nr)
{
    monitor_vlm = vlm_nr;
}

void SharedData::update_monitor_peer_vlm(const uint16_t peer_vlm_nr)
{
    monitor_peer_vlm = peer_vlm_nr;
}

void SharedData::clear_monitor_rsc()
{
    monitor_rsc.clear();
    monitor_con.clear();
    monitor_vlm = DisplayConsts::VLM_NONE;
    monitor_peer_vlm = DisplayConsts::VLM_NONE;
}

void SharedData::clear_monitor_con()
{
    monitor_con.clear();
    // The peer volumes are supposed to be the same for each connection,
    // therefore, the cursor is not reset
}

void SharedData::clear_monitor_vlm()
{
    monitor_vlm = DisplayConsts::VLM_NONE;
}

void SharedData::clear_monitor_peer_vlm()
{
    monitor_peer_vlm = DisplayConsts::VLM_NONE;
}

void SharedData::clear_resources_selection()
{
    clear_resources_selection_impl();
}

void SharedData::clear_connections_selection(const std::string& resource_name)
{
    ResourceSubSelections* const sub_selections = selected_resources->get(&resource_name);
    if (sub_selections != nullptr)
    {
        clear_connections_selection_impl(*sub_selections);
    }
}

void SharedData::clear_connections_selection(ResourceSubSelections& sub_selections)
{
    clear_connections_selection_impl(sub_selections);
}

void SharedData::clear_volumes_selection(const std::string& resource_name)
{
    ResourceSubSelections* const sub_selections = selected_resources->get(&resource_name);
    if (sub_selections != nullptr)
    {
        clear_volumes_selection_impl(*sub_selections);
    }
}

void SharedData::clear_volumes_selection(ResourceSubSelections& sub_selections)
{
    clear_volumes_selection_impl(sub_selections);
}

void SharedData::clear_peer_volumes_selection(const std::string& resource_name, const std::string& connection_name)
{
    ResourceSubSelections* const sub_selections = selected_resources->get(&resource_name);
    if (sub_selections != nullptr)
    {
        ConnectionSelectionMap* const selected_connections = sub_selections->connection_selection.get();
        if (selected_connections != nullptr)
        {
            ConnectionSelectionMap::Node* const connection_node = selected_connections->get_node(&connection_name);
            if (connection_node != nullptr)
            {
                clear_peer_volumes_selection_impl(*connection_node);
            }
        }
    }
}

void SharedData::clear_peer_volumes_selection(ConnectionSelectionMap::Node& connection_node)
{
    clear_peer_volumes_selection_impl(connection_node);
}

void SharedData::clear_resources_selection_impl() noexcept
{
    ResourceSelectionMap::NodesIterator iter(*selected_resources);
    while (iter.has_next())
    {
        ResourceSelectionMap::Node* const selected_resource = iter.next();

        const std::string* const key = selected_resource->get_key();
        ResourceSubSelections* const sub_selections = selected_resource->get_value();

        clear_connections_selection_impl(*sub_selections);
        clear_volumes_selection_impl(*sub_selections);

        delete key;
        delete sub_selections;
    }
    selected_resources->clear();
    selection_stats.rsc_count = 0;
    selection_stats.vlm_count = 0;
    selection_stats.con_count = 0;
    selection_stats.peer_vlm_count = 0;
}

void SharedData::clear_connections_selection_impl(ResourceSubSelections& sub_selections) noexcept
{
    ConnectionSelectionMap* const selected_connections = sub_selections.connection_selection.get();
    if (selected_connections != nullptr)
    {
        ConnectionSelectionMap::NodesIterator iter(*selected_connections);
        while (iter.has_next())
        {
            ConnectionSelectionMap::Node* const selected_connection = iter.next();
            clear_peer_volumes_selection_impl(*selected_connection);
            const std::string* const key = selected_connection->get_key();
            delete key;
        }
        stats_subtract(selection_stats.con_count, static_cast<uint64_t> (selected_connections->get_size()));
        selected_connections->clear();
        sub_selections.connection_selection = nullptr;
    }
}

void SharedData::clear_volumes_selection_impl(ResourceSubSelections& sub_selections) noexcept
{
    VolumeSelectionMap* const selected_volumes = sub_selections.volume_selection.get();
    if (selected_volumes != nullptr)
    {
        VolumeSelectionMap::KeysIterator iter(*selected_volumes);
        while (iter.has_next())
        {
            const uint16_t* const key = iter.next();
            delete key;
        }
        stats_subtract(selection_stats.vlm_count, static_cast<uint64_t> (selected_volumes->get_size()));
        selected_volumes->clear();
        sub_selections.volume_selection = nullptr;
    }
}

void SharedData::clear_peer_volumes_selection_impl(ConnectionSelectionMap::Node& connection_node) noexcept
{
    VolumeSelectionMap* const selected_peer_volumes = connection_node.get_value();
    if (selected_peer_volumes != nullptr)
    {
        VolumeSelectionMap::KeysIterator iter(*selected_peer_volumes);
        while (iter.has_next())
        {
            const uint16_t* const key = iter.next();
            delete key;
        }
        stats_subtract(selection_stats.peer_vlm_count, static_cast<uint64_t> (selected_peer_volumes->get_size()));
        selected_peer_volumes->clear();
        connection_node.set_value(nullptr);
        delete selected_peer_volumes;
    }
}

ResourceSelectionMap::Node* SharedData::select_resource(const std::string& name)
{
    ResourceSelectionMap::Node* entry = selected_resources->get_node(&name);
    if (entry == nullptr)
    {
        std::unique_ptr<std::string> key_mgr(new std::string(name));
        std::unique_ptr<ResourceSubSelections> value_mgr(new ResourceSubSelections());
        std::unique_ptr<ResourceSelectionMap::Node> node_mgr(
            new ResourceSelectionMap::Node(key_mgr.get(), value_mgr.get())
        );

        entry = node_mgr.get();
        try
        {
            selected_resources->insert_node(entry);

            key_mgr.release();
            value_mgr.release();
            node_mgr.release();

            stats_add(selection_stats.rsc_count, 1);
        }
        catch (dsaext::DuplicateInsertException&)
        {
            // TODO: Logging to the debug log may be useful here
        }
    }
    return entry;
}

void SharedData::deselect_resource(const std::string& name)
{
    ResourceSelectionMap::Node* const existing_entry = selected_resources->get_node(&name);
    if (existing_entry != nullptr)
    {
        const std::string* const key = existing_entry->get_key();
        ResourceSubSelections* const sub_selections = existing_entry->get_value();
        clear_connections_selection(*sub_selections);
        clear_volumes_selection(*sub_selections);

        delete key;
        delete sub_selections;

        selected_resources->remove_node(existing_entry);

        stats_subtract(selection_stats.rsc_count, 1);
    }
}

ConnectionSelectionMap::Node* SharedData::select_connection(
    const std::string& resource_name,
    const std::string& connection_name
)
{
    ResourceSelectionMap::Node* resource_node = select_resource(resource_name);
    ResourceSubSelections* const sub_selections = resource_node->get_value();

    return select_connection(*sub_selections, connection_name);
}

ConnectionSelectionMap::Node* SharedData::select_connection(
    ResourceSubSelections& sub_selections,
    const std::string& connection_name
)
{
    ConnectionSelectionMap::Node* entry = nullptr;
    if (!sub_selections.connection_selection)
    {
        sub_selections.connection_selection = std::unique_ptr<ConnectionSelectionMap>(
            new ConnectionSelectionMap(&comparators::compare_string)
        );
    }
    else
    {
        entry = sub_selections.connection_selection->get_node(&connection_name);
    }

    if (entry == nullptr)
    {
        std::unique_ptr<std::string> key_mgr(new std::string(connection_name));
        std::unique_ptr<ConnectionSelectionMap::Node> node_mgr(
            new ConnectionSelectionMap::Node(key_mgr.get(), nullptr)
        );

        entry = node_mgr.get();

        try
        {
            sub_selections.connection_selection->insert_node(entry);

            key_mgr.release();
            node_mgr.release();

            stats_add(selection_stats.con_count, 1);
        }
        catch (dsaext::DuplicateInsertException&)
        {
            // TODO: Logging to the debug log may be useful here
        }
    }

    return entry;
}

void SharedData::deselect_connection(const std::string& resource_name, const std::string& connection_name)
{
    ResourceSubSelections* const sub_selections = selected_resources->get(&resource_name);
    if (sub_selections != nullptr)
    {
        deselect_connection(*sub_selections, connection_name);
    }
}

void SharedData::deselect_connection(ResourceSubSelections& sub_selections, const std::string& connection_name)
{
    ConnectionSelectionMap* const selected_connections = sub_selections.connection_selection.get();
    if (selected_connections != nullptr)
    {
        ConnectionSelectionMap::Node* const connection_node = selected_connections->get_node(&connection_name);
        if (connection_node != nullptr)
        {
            clear_peer_volumes_selection_impl(*connection_node);

            const std::string* const key = connection_node->get_key();
            delete key;

            selected_connections->remove_node(connection_node);

            stats_subtract(selection_stats.con_count, 1);
        }
        if (selected_connections->get_size() == 0)
        {
            sub_selections.connection_selection = nullptr;
        }
    }
}

VolumeSelectionMap::Node* SharedData::select_volume(const std::string& resource_name, const uint16_t vlm_nr)
{
    ResourceSelectionMap::Node* resource_node = select_resource(resource_name);
    ResourceSubSelections* const sub_selections = resource_node->get_value();

    return select_volume(*sub_selections, vlm_nr);
}

VolumeSelectionMap::Node* SharedData::select_volume(ResourceSubSelections& sub_selections, const uint16_t vlm_nr)
{
    VolumeSelectionMap::Node* entry = nullptr;
    if (!sub_selections.volume_selection)
    {
        sub_selections.volume_selection = std::unique_ptr<VolumeSelectionMap>(
            new VolumeSelectionMap(&comparators::compare<uint16_t>)
        );
    }
    else
    {
        entry = sub_selections.volume_selection->get_node(&vlm_nr);
    }

    if (entry == nullptr)
    {
        std::unique_ptr<uint16_t> key_mgr(new uint16_t);
        uint16_t* const key = key_mgr.get();
        *key = vlm_nr;
        std::unique_ptr<VolumeSelectionMap::Node> node_mgr(
            new VolumeSelectionMap::Node(key, nullptr)
        );

        entry = node_mgr.get();
        try
        {
            sub_selections.volume_selection->insert_node(entry);

            key_mgr.release();
            node_mgr.release();

            stats_add(selection_stats.vlm_count, 1);
        }
        catch (dsaext::DuplicateInsertException&)
        {
            // TODO: Logging to the debug log may be useful here
        }
    }

    return entry;
}

void SharedData::deselect_volume(const std::string& resource_name, const uint16_t vlm_nr)
{
    ResourceSubSelections* const sub_selections = selected_resources->get(&resource_name);
    if (sub_selections != nullptr)
    {
        deselect_volume(*sub_selections, vlm_nr);
    }
}

void SharedData::deselect_volume(ResourceSubSelections& sub_selections, const uint16_t vlm_nr)
{
    VolumeSelectionMap* const selected_volumes = sub_selections.volume_selection.get();
    if (selected_volumes != nullptr)
    {
        VolumeSelectionMap::Node* const volume_node = selected_volumes->get_node(&vlm_nr);
        if (volume_node != nullptr)
        {
            const uint16_t* const key = volume_node->get_key();
            delete key;

            selected_volumes->remove_node(volume_node);

            stats_subtract(selection_stats.vlm_count, 1);
        }
        if (selected_volumes->get_size() == 0)
        {
            sub_selections.volume_selection = nullptr;
        }
    }
}

VolumeSelectionMap::Node* SharedData::select_peer_volume(
    const std::string& resource_name,
    const std::string& connection_name,
    const uint16_t vlm_nr
)
{
    ConnectionSelectionMap::Node* const connection_node = select_connection(resource_name, connection_name);
    return select_peer_volume(*connection_node, vlm_nr);
}

VolumeSelectionMap::Node* SharedData::select_peer_volume(
    ConnectionSelectionMap::Node& connection_node,
    const uint16_t vlm_nr
)
{
    VolumeSelectionMap::Node* entry = nullptr;
    VolumeSelectionMap* selected_peer_volumes = connection_node.get_value();
    if (selected_peer_volumes == nullptr)
    {
        std::unique_ptr<VolumeSelectionMap> selected_peer_volumes_mgr(
            new VolumeSelectionMap(&comparators::compare<uint16_t>)
        );
        selected_peer_volumes = selected_peer_volumes_mgr.get();
        connection_node.set_value(selected_peer_volumes);

        selected_peer_volumes_mgr.release();
    }
    else
    {
        entry = selected_peer_volumes->get_node(&vlm_nr);
    }

    if (entry == nullptr)
    {
        std::unique_ptr<uint16_t> key_mgr(new uint16_t);
        uint16_t* const key = key_mgr.get();
        *key = vlm_nr;
        std::unique_ptr<VolumeSelectionMap::Node> node_mgr(
            new VolumeSelectionMap::Node(key, nullptr)
        );

        entry = node_mgr.get();
        try
        {
            selected_peer_volumes->insert_node(entry);

            key_mgr.release();
            node_mgr.release();

            stats_add(selection_stats.peer_vlm_count, 1);
        }
        catch (dsaext::DuplicateInsertException&)
        {
            // TODO: Logging to the debug log may be useful here
        }
    }

    return entry;
}

void SharedData::deselect_peer_volume(
    const std::string& resource_name,
    const std::string& connection_name,
    const uint16_t vlm_nr
)
{
    ResourceSubSelections* const sub_selections = selected_resources->get(&resource_name);
    if (sub_selections != nullptr && sub_selections->connection_selection)
    {
        ConnectionSelectionMap::Node* const connection_node =
            sub_selections->connection_selection->get_node(&connection_name);
        if (connection_node != nullptr)
        {
            deselect_peer_volume(*connection_node, vlm_nr);
        }
    }
}

void SharedData::deselect_peer_volume(
    ConnectionSelectionMap::Node& connection_node,
    const uint16_t vlm_nr
)
{
    VolumeSelectionMap* const selected_peer_volumes = connection_node.get_value();
    if (selected_peer_volumes != nullptr)
    {
        VolumeSelectionMap::Node* const peer_volume_node = selected_peer_volumes->get_node(&vlm_nr);
        if (peer_volume_node != nullptr)
        {
            uint16_t* const key = peer_volume_node->get_key();

            delete key;
            selected_peer_volumes->remove_node(peer_volume_node);

            stats_subtract(selection_stats.peer_vlm_count, 1);
        }

        if (selected_peer_volumes->get_size() == 0)
        {
            connection_node.set_value(nullptr);
            delete selected_peer_volumes;
        }
    }
}

bool SharedData::toggle_resource_selection(const std::string& resource_name)
{
    bool selected = false;
    if (is_resource_selected(resource_name))
    {
        deselect_resource(resource_name);
    }
    else
    {
        select_resource(resource_name);
        selected = true;
    }
    return selected;
}

bool SharedData::toggle_connection_selection(const std::string& resource_name, const std::string& connection_name)
{
    bool selected = false;
    if (is_connection_selected(resource_name, connection_name))
    {
        deselect_connection(resource_name, connection_name);
    }
    else
    {
        select_connection(resource_name, connection_name);
        selected = true;
    }
    return selected;
}

bool SharedData::toggle_volume_selection(const std::string& resource_name, const uint16_t vlm_nr)
{
    bool selected = false;
    if (is_volume_selected(resource_name, vlm_nr))
    {
        deselect_volume(resource_name, vlm_nr);
    }
    else
    {
        select_volume(resource_name, vlm_nr);
        selected = true;
    }
    return selected;
}

bool SharedData::toggle_peer_volume_selection(
    const std::string& resource_name,
    const std::string& connection_name,
    const uint16_t vlm_nr
)
{
    bool selected = false;
    if (is_peer_volume_selected(resource_name, connection_name, vlm_nr))
    {
        deselect_peer_volume(resource_name, connection_name, vlm_nr);
    }
    else
    {
        select_peer_volume(resource_name, connection_name, vlm_nr);
        selected = true;
    }
    return selected;
}

bool SharedData::have_resources_selection()
{
    return selected_resources->get_size() > 0;
}

bool SharedData::have_connections_selection(const std::string& resource_name)
{
    bool have_selection = false;
    ResourceSubSelections* const sub_selections = selected_resources->get(&resource_name);
    if (sub_selections != nullptr)
    {
        have_selection = have_connections_selection(*sub_selections);
    }
    return have_selection;
}

bool SharedData::have_connections_selection(const ResourceSubSelections& sub_selections)
{
    return sub_selections.connection_selection && sub_selections.connection_selection->get_size() > 0;
}

bool SharedData::have_volumes_selection(const std::string& resource_name)
{
    bool have_selection = false;
    ResourceSubSelections* const sub_selections = selected_resources->get(&resource_name);
    if (sub_selections != nullptr)
    {
        have_selection = have_volumes_selection(*sub_selections);
    }
    return have_selection;
}

bool SharedData::have_volumes_selection(const ResourceSubSelections& sub_selections)
{
    return sub_selections.volume_selection && sub_selections.volume_selection->get_size() > 0;
}

bool SharedData::have_peer_volumes_selection(const std::string& resource_name, const std::string& connection_name)
{
    bool have_selection = false;
    ResourceSubSelections* const sub_selections = selected_resources->get(&resource_name);
    if (sub_selections != nullptr)
    {
        if (sub_selections->connection_selection)
        {
            ConnectionSelectionMap::Node* const connection_node =
                sub_selections->connection_selection->get_node(&connection_name);
            if (connection_node != nullptr)
            {
                have_selection = have_peer_volumes_selection(*connection_node);
            }
        }
    }
    return have_selection;
}

bool SharedData::have_peer_volumes_selection(const ConnectionSelectionMap::Node& connection_node)
{
    VolumeSelectionMap* const selected_peer_volumes = connection_node.get_value();
    return selected_peer_volumes != nullptr && selected_peer_volumes->get_size() > 0;
}

bool SharedData::is_resource_selected(const std::string& resource_name)
{
    return selected_resources->get_node(&resource_name) != nullptr;
}

bool SharedData::is_connection_selected(const std::string& resource_name, const std::string& connection_name)
{
    bool have_selection = false;
    ResourceSubSelections* const sub_selections = selected_resources->get(&resource_name);
    if (sub_selections != nullptr)
    {
        have_selection = is_connection_selected(*sub_selections, connection_name);
    }
    return have_selection;
}

bool SharedData::is_connection_selected(
    const ResourceSubSelections& sub_selections,
    const std::string& connection_name
)
{
    return sub_selections.connection_selection &&
        sub_selections.connection_selection->get_node(&connection_name) != nullptr;
}

bool SharedData::is_volume_selected(const std::string& resource_name, const uint16_t vlm_nr)
{
    bool have_selection = false;
    ResourceSubSelections* const sub_selections = selected_resources->get(&resource_name);
    if (sub_selections != nullptr)
    {
        have_selection = is_volume_selected(*sub_selections, vlm_nr);
    }
    return have_selection;
}

bool SharedData::is_volume_selected(
    const ResourceSubSelections& sub_selections,
    const uint16_t vlm_nr
)
{
    return sub_selections.volume_selection && sub_selections.volume_selection->get_node(&vlm_nr) != nullptr;
}

bool SharedData::is_peer_volume_selected(
    const std::string& resource_name,
    const std::string& connection_name,
    const uint16_t vlm_nr
)
{
    bool have_selection = false;
    ResourceSubSelections* const sub_selections = selected_resources->get(&resource_name);
    if (sub_selections != nullptr && sub_selections->connection_selection)
    {
        ConnectionSelectionMap::Node* const connection_node =
            sub_selections->connection_selection->get_node(&connection_name);
        if (connection_node != nullptr)
        {
            have_selection = is_peer_volume_selected(*connection_node, vlm_nr);
        }
    }
    return have_selection;
}

bool SharedData::is_peer_volume_selected(
    ConnectionSelectionMap::Node& connection_node,
    const uint16_t vlm_nr
)
{
    bool have_selection = false;
    VolumeSelectionMap* const selected_peer_volumes = connection_node.get_value();
    if (selected_peer_volumes != nullptr)
    {
        have_selection = selected_peer_volumes->get_node(&vlm_nr) != nullptr;
    }
    return have_selection;
}


ResourceSelectionMap& SharedData::get_selected_resources_map()
{
    return *selected_resources;
}

ConnectionSelectionMap* SharedData::get_selected_connections_map(const std::string& rsc_name)
{
    ConnectionSelectionMap* selected_connections = nullptr;
    ResourceSubSelections* const sub_selections = selected_resources->get(&rsc_name);
    if (sub_selections != nullptr)
    {
        selected_connections = sub_selections->connection_selection.get();
    }
    return selected_connections;
}

VolumeSelectionMap* SharedData::get_selected_volumes_map(const std::string& rsc_name)
{
    VolumeSelectionMap* selected_volumes = nullptr;
    ResourceSubSelections* const sub_selections = selected_resources->get(&rsc_name);
    if (sub_selections != nullptr)
    {
        selected_volumes = sub_selections->volume_selection.get();
    }
    return selected_volumes;
}

VolumeSelectionMap* SharedData::get_selected_peer_volumes_map(
    const std::string& rsc_name,
    const std::string& con_name
)
{
    VolumeSelectionMap* selected_peer_volumes = nullptr;
    ResourceSubSelections* const sub_selections = selected_resources->get(&rsc_name);
    if (sub_selections != nullptr && sub_selections->connection_selection)
    {
        selected_peer_volumes = sub_selections->connection_selection->get(&con_name);
    }
    return selected_peer_volumes;
}

void SharedData::select_task(TaskEntryMap& selection_map, const uint64_t entry_id)
{
    generic_id_select(selection_map, entry_id);
}

void SharedData::deselect_task(TaskEntryMap& selection_map, const uint64_t entry_id)
{
    generic_id_deselect(selection_map, entry_id);
}

bool SharedData::toggle_task_selection(TaskEntryMap& selection_map, const uint64_t entry_id)
{
    return generic_id_toggle(selection_map, entry_id);
}

bool SharedData::have_task_selection(TaskEntryMap& selection_map)
{
    return generic_id_have_selection(selection_map);
}

bool SharedData::is_task_selected(TaskEntryMap& selection_map, const uint64_t entry_id)
{
    return generic_id_is_selected(selection_map, entry_id);
}

void SharedData::clear_task_selection(TaskEntryMap& selection_map)
{
    generic_id_clear_selection(selection_map);
}

void SharedData::select_log_entry(MessageMap& selection_map, const uint64_t entry_id)
{
    generic_id_select(selection_map, entry_id);
}

void SharedData::deselect_log_entry(MessageMap& selection_map, const uint64_t entry_id)
{
    generic_id_deselect(selection_map, entry_id);
}

bool SharedData::toggle_log_entry_selection(MessageMap& selection_map, const uint64_t entry_id)
{
    return generic_id_toggle(selection_map, entry_id);
}

bool SharedData::have_log_entry_selection(MessageMap& selection_map)
{
    return generic_id_have_selection(selection_map);
}

bool SharedData::is_log_entry_selected(MessageMap& selection_map, const uint64_t entry_id)
{
    return generic_id_is_selected(selection_map, entry_id);
}

void SharedData::clear_log_entry_selection(MessageMap& selection_map)
{
    generic_id_clear_selection(selection_map);
}

void SharedData::generic_id_select(QTree<uint64_t, void>& selection_map, const uint64_t entry_id)
{
    QTree<uint64_t, void>::Node* const existing_entry = selection_map.get_node(&entry_id);
    if (existing_entry == nullptr)
    {
        std::unique_ptr<uint64_t> key_mgr(new uint64_t);
        uint64_t* key_ptr = key_mgr.get();
        *key_ptr = entry_id;

        selection_map.insert(key_ptr, nullptr);
        key_mgr.release();
    }
}

void SharedData::generic_id_deselect(QTree<uint64_t, void>& selection_map, const uint64_t entry_id)
{
    QTree<uint64_t, void>::Node* const existing_entry = selection_map.get_node(&entry_id);
    if (existing_entry != nullptr)
    {
        delete existing_entry->get_key();
        selection_map.remove_node(existing_entry);
    }
}

bool SharedData::generic_id_toggle(QTree<uint64_t, void>& selection_map, const uint64_t entry_id)
{
    bool selected = false;
    QTree<uint64_t, void>::Node* const existing_entry = selection_map.get_node(&entry_id);
    if (existing_entry == nullptr)
    {
        select_task(selection_map, entry_id);
        selected = true;
    }
    else
    {
        deselect_task(selection_map, entry_id);
    }
    return selected;
}

bool SharedData::generic_id_have_selection(QTree<uint64_t, void>& selection_map)
{
    return selection_map.get_size() > 0;
}

bool SharedData::generic_id_is_selected(QTree<uint64_t, void>& selection_map, const uint64_t entry_id)
{
    return selection_map.get_node(&entry_id) != nullptr;
}

void SharedData::generic_id_clear_selection(TaskEntryMap& selection_map) noexcept
{
    TaskEntryMap::KeysIterator iter(selection_map);
    while (iter.has_next())
    {
        uint64_t* const entry_id = iter.next();
        delete entry_id;
    }
    selection_map.clear();
}

SharedData::SelectionStatistics::SelectionStatistics()
{
}

SharedData::SelectionStatistics::~SelectionStatistics() noexcept
{
}

void SharedData::stats_add(uint64_t& counter, const uint64_t value)
{
    counter += value;
}

void SharedData::stats_subtract(uint64_t& counter, const uint64_t value)
{
    counter = (counter >= value ? counter - value : 0);
}

SharedData::SelectionStatistics SharedData::get_selection_statistics()
{
    SelectionStatistics current_stats(selection_stats);
    return current_stats;
}
