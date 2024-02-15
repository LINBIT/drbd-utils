#include <terminal/SharedData.h>
#include <comparators.h>

SharedData::SharedData()
{
    selected_resources = std::unique_ptr<ResourcesMap>(new ResourcesMap(&comparators::compare_string));
    selected_connections = std::unique_ptr<ConnectionsMap>(new ConnectionsMap(&comparators::compare_string));
    selected_volumes = std::unique_ptr<VolumesMap>(new VolumesMap(&comparators::compare<uint16_t>));
    selected_peer_volumes = std::unique_ptr<VolumesMap>(new VolumesMap(&comparators::compare<uint16_t>));

    selected_actq_entries = std::unique_ptr<TaskEntryMap>(new TaskEntryMap(&comparators::compare<uint64_t>));
    selected_pndq_entries = std::unique_ptr<TaskEntryMap>(new TaskEntryMap(&comparators::compare<uint64_t>));
    selected_sspq_entries = std::unique_ptr<TaskEntryMap>(new TaskEntryMap(&comparators::compare<uint64_t>));
    selected_finq_entries = std::unique_ptr<TaskEntryMap>(new TaskEntryMap(&comparators::compare<uint64_t>));

    selected_log_entries = std::unique_ptr<MessageMap>(new MessageMap(&comparators::compare<uint64_t>));
}

SharedData::~SharedData() noexcept
{
    clear_resources_selection_impl();
    clear_connections_selection_impl();
    clear_volumes_selection_impl();
    clear_peer_volumes_selection_impl();

    generic_id_clear_selection(*selected_actq_entries);
    generic_id_clear_selection(*selected_pndq_entries);
    generic_id_clear_selection(*selected_sspq_entries);
    generic_id_clear_selection(*selected_finq_entries);

    generic_id_clear_selection(*selected_log_entries);
}

void SharedData::update_monitor_rsc(const std::string& rsc_name)
{
    if (rsc_name != monitor_rsc)
    {
        monitor_rsc = rsc_name;
        monitor_con.clear();
        monitor_vlm = DisplayConsts::VLM_NONE;
        monitor_peer_vlm = DisplayConsts::VLM_NONE;

        clear_connections_selection_impl();
        clear_volumes_selection_impl();
        clear_peer_volumes_selection_impl();
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

    clear_connections_selection_impl();
    clear_volumes_selection_impl();
    clear_peer_volumes_selection_impl();
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

void SharedData::clear_connections_selection()
{
    clear_connections_selection_impl();
}

void SharedData::clear_volumes_selection()
{
    clear_volumes_selection_impl();
}

void SharedData::clear_peer_volumes_selection()
{
    clear_peer_volumes_selection_impl();
}

void SharedData::clear_resources_selection_impl() noexcept
{
    ResourcesMap::KeysIterator iter(*selected_resources);
    while (iter.has_next())
    {
        const std::string* const key = iter.next();
        delete key;
    }
    selected_resources->clear();
}

void SharedData::clear_connections_selection_impl() noexcept
{
    ConnectionsMap::KeysIterator iter(*selected_connections);
    while (iter.has_next())
    {
        const std::string* const key = iter.next();
        delete key;
    }
    selected_connections->clear();
}

void SharedData::clear_volumes_selection_impl() noexcept
{
    VolumesMap::KeysIterator iter(*selected_volumes);
    while (iter.has_next())
    {
        const uint16_t* const key = iter.next();
        delete key;
    }
    selected_volumes->clear();
}

void SharedData::clear_peer_volumes_selection_impl() noexcept
{
    VolumesMap::KeysIterator iter(*selected_peer_volumes);
    while (iter.has_next())
    {
        const uint16_t* const key = iter.next();
        delete key;
    }
    selected_peer_volumes->clear();
}

void SharedData::select_resource(const std::string& name)
{
    const ResourcesMap::Node* const existing_entry = selected_resources->get_node(&name);
    if (existing_entry == nullptr)
    {
        std::unique_ptr<std::string> key_mgr(new std::string(name));

        selected_resources->insert(key_mgr.get(), nullptr);
        key_mgr.release();
    }
}

void SharedData::deselect_resource(const std::string& name)
{
    ResourcesMap::Node* const existing_entry = selected_resources->get_node(&name);
    if (existing_entry != nullptr)
    {
        delete existing_entry->get_key();
        selected_resources->remove_node(existing_entry);
    }
}

void SharedData::select_connection(const std::string& name)
{
    const ConnectionsMap::Node* const existing_entry = selected_connections->get_node(&name);
    if (existing_entry == nullptr)
    {
        std::unique_ptr<std::string> key_mgr(new std::string(name));

        selected_connections->insert(key_mgr.get(), nullptr);
        key_mgr.release();
    }
}

void SharedData::deselect_connection(const std::string& name)
{
    ConnectionsMap::Node* const existing_entry = selected_connections->get_node(&name);
    if (existing_entry != nullptr)
    {
        delete existing_entry->get_key();
        selected_connections->remove_node(existing_entry);
    }
}


void SharedData::select_volume(const uint16_t vlm_nr)
{
    const VolumesMap::Node* const existing_entry = selected_volumes->get_node(&vlm_nr);
    if (existing_entry == nullptr)
    {
        std::unique_ptr<uint16_t> key_mgr(new uint16_t);
        uint16_t* const key = key_mgr.get();
        *key = vlm_nr;

        selected_volumes->insert(key, nullptr);
        key_mgr.release();
    }
}

void SharedData::deselect_volume(const uint16_t vlm_nr)
{
    VolumesMap::Node* const existing_entry = selected_volumes->get_node(&vlm_nr);
    if (existing_entry != nullptr)
    {
        delete existing_entry->get_key();
        selected_volumes->remove_node(existing_entry);
    }
}

void SharedData::select_peer_volume(const uint16_t vlm_nr)
{
    const VolumesMap::Node* const existing_entry = selected_peer_volumes->get_node(&vlm_nr);
    if (existing_entry == nullptr)
    {
        std::unique_ptr<uint16_t> key_mgr(new uint16_t);
        uint16_t* const key = key_mgr.get();
        *key = vlm_nr;

        selected_peer_volumes->insert(key, nullptr);
        key_mgr.release();
    }
}

void SharedData::deselect_peer_volume(const uint16_t vlm_nr)
{
    VolumesMap::Node* const existing_entry = selected_peer_volumes->get_node(&vlm_nr);
    if (existing_entry != nullptr)
    {
        delete existing_entry->get_key();
        selected_peer_volumes->remove_node(existing_entry);
    }
}

bool SharedData::toggle_resource_selection(const std::string& name)
{
    bool selected = false;
    ResourcesMap::Node* const existing_entry = selected_resources->get_node(&name);
    if (existing_entry == nullptr)
    {
        select_resource(name);
        selected = true;
    }
    else
    {
        deselect_resource(name);
    }
    return selected;
}

bool SharedData::toggle_connection_selection(const std::string& name)
{
    bool selected = false;
    ConnectionsMap::Node* const existing_entry = selected_connections->get_node(&name);
    if (existing_entry == nullptr)
    {
        select_connection(name);
        selected = true;
    }
    else
    {
        deselect_connection(name);
    }
    return selected;
}


bool SharedData::toggle_volume_selection(const uint16_t vlm_nr)
{
    bool selected = false;
    VolumesMap::Node* const existing_entry = selected_volumes->get_node(&vlm_nr);
    if (existing_entry == nullptr)
    {
        select_volume(vlm_nr);
        selected = true;
    }
    else
    {
        deselect_volume(vlm_nr);
    }
    return selected;
}

bool SharedData::toggle_peer_volume_selection(const uint16_t vlm_nr)
{
    bool selected = false;
    VolumesMap::Node* const existing_entry = selected_peer_volumes->get_node(&vlm_nr);
    if (existing_entry == nullptr)
    {
        select_peer_volume(vlm_nr);
        selected = true;
    }
    else
    {
        deselect_peer_volume(vlm_nr);
    }
    return selected;
}

bool SharedData::have_resources_selection()
{
    return selected_resources->get_size() > 0;
}

bool SharedData::have_connections_selection()
{
    return selected_connections->get_size() > 0;
}

bool SharedData::have_volumes_selection()
{
    return selected_volumes->get_size() > 0;
}

bool SharedData::have_peer_volumes_selection()
{
    return selected_peer_volumes->get_size() > 0;
}

bool SharedData::is_resource_selected(const std::string& name)
{
    return selected_resources->get_node(&name) != nullptr;
}

bool SharedData::is_connection_selected(const std::string& name)
{
    return selected_connections->get_node(&name) != nullptr;
}

bool SharedData::is_volume_selected(const uint16_t vlm_nr)
{
    return selected_volumes->get_node(&vlm_nr) != nullptr;
}

bool SharedData::is_peer_volume_selected(const uint16_t vlm_nr)
{
    return selected_peer_volumes->get_node(&vlm_nr) != nullptr;
}

ResourcesMap& SharedData::get_selected_resources_map()
{
    return *selected_resources;
}

ConnectionsMap& SharedData::get_selected_connections_map()
{
    return *selected_connections;
}

VolumesMap& SharedData::get_selected_volumes_map()
{
    return *selected_volumes;
}

VolumesMap& SharedData::get_selected_peer_volumes_map()
{
    return *selected_peer_volumes;
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
