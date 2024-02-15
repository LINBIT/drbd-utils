#ifndef SHAREDDATA_H
#define SHAREDDATA_H

#include <default_types.h>
#include <new>
#include <memory>
#include <string>
#include <MessageLog.h>
#include <map_types.h>
#include <QTree.h>
#include <terminal/DisplayConsts.h>
#include <subprocess/SubProcessQueue.h>

class SharedData
{
  public:
    // The resource currently selected for monitoring
    std::string monitor_rsc;
    // The volume currently selected for monitoring
    uint16_t    monitor_vlm         {DisplayConsts::VLM_NONE};
    // The connection currently selected for monitoring
    std::string monitor_con;
    // The peer volume currently selected for monitoring
    uint16_t    monitor_peer_vlm    {DisplayConsts::VLM_NONE};
    // The selected message in the message log
    uint64_t	message_id			{MessageLog::ID_NONE};
    // The selected task for a task details display
    uint64_t    task_id             {SubProcessQueue::TASKQ_NONE};

    // Text for MDspHelp
    std::string help_text;
    bool        help_text_updated   {false};

    // Debug message
    std::string debug_msg;

    std::unique_ptr<ResourcesMap>   selected_resources;
    std::unique_ptr<ConnectionsMap> selected_connections;
    std::unique_ptr<VolumesMap>     selected_volumes;
    std::unique_ptr<VolumesMap>     selected_peer_volumes;

    std::unique_ptr<TaskEntryMap>   selected_actq_entries;
    std::unique_ptr<TaskEntryMap>   selected_pndq_entries;
    std::unique_ptr<TaskEntryMap>   selected_sspq_entries;
    std::unique_ptr<TaskEntryMap>   selected_finq_entries;

    std::unique_ptr<MessageMap>     selected_log_entries;

    bool    activate_tasks  {true};

    bool    ovrd_resource_selection     {false};
    bool    ovrd_connection_selection   {false};
    bool    ovrd_volume_selection       {false};
    bool    ovrd_peer_volume_selection  {false};

    SharedData();
    virtual ~SharedData() noexcept;

    virtual void update_monitor_rsc(const std::string& rsc_name);
    virtual void update_monitor_con(const std::string& con_name);
    virtual void update_monitor_vlm(const uint16_t vlm_nr);
    virtual void update_monitor_peer_vlm(const uint16_t peer_vlm_nr);

    virtual void clear_monitor_rsc();
    virtual void clear_monitor_con();
    virtual void clear_monitor_vlm();
    virtual void clear_monitor_peer_vlm();

    virtual void clear_resources_selection();
    virtual void clear_connections_selection();
    virtual void clear_volumes_selection();
    virtual void clear_peer_volumes_selection();

    virtual void select_resource(const std::string& name);
    virtual void deselect_resource(const std::string& name);
    virtual void select_connection(const std::string& name);
    virtual void deselect_connection(const std::string& name);
    virtual void select_volume(const uint16_t vlm_nr);
    virtual void deselect_volume(const uint16_t vlm_nr);
    virtual void select_peer_volume(const uint16_t vlm_nr);
    virtual void deselect_peer_volume(const uint16_t vlm_nr);

    virtual bool toggle_resource_selection(const std::string& name);
    virtual bool toggle_connection_selection(const std::string& name);
    virtual bool toggle_volume_selection(const uint16_t vlm_nr);
    virtual bool toggle_peer_volume_selection(const uint16_t vlm_nr);

    virtual bool have_resources_selection();
    virtual bool have_connections_selection();
    virtual bool have_volumes_selection();
    virtual bool have_peer_volumes_selection();

    virtual bool is_resource_selected(const std::string& name);
    virtual bool is_connection_selected(const std::string& name);
    virtual bool is_volume_selected(const uint16_t vlm_nr);
    virtual bool is_peer_volume_selected(const uint16_t vlm_nr);

    virtual ResourcesMap& get_selected_resources_map();
    virtual VolumesMap& get_selected_volumes_map();
    virtual VolumesMap& get_selected_peer_volumes_map();
    virtual ConnectionsMap& get_selected_connections_map();


    virtual void select_task(TaskEntryMap& selection_map, const uint64_t entry_id);
    virtual void deselect_task(TaskEntryMap& selection_map, const uint64_t entry_id);
    virtual bool toggle_task_selection(TaskEntryMap& selection_map, const uint64_t entry_id);
    virtual bool have_task_selection(TaskEntryMap& selection_map);
    virtual bool is_task_selected(TaskEntryMap& selection_map, const uint64_t entry_id);
    virtual void clear_task_selection(TaskEntryMap& selection_map);

    virtual void select_log_entry(MessageMap& selection_map, const uint64_t entry_id);
    virtual void deselect_log_entry(MessageMap& selection_map, const uint64_t entry_id);
    virtual bool toggle_log_entry_selection(MessageMap& selection_map, const uint64_t entry_id);
    virtual bool have_log_entry_selection(MessageMap& selection_map);
    virtual bool is_log_entry_selected(MessageMap& selection_map, const uint64_t entry_id);
    virtual void clear_log_entry_selection(MessageMap& selection_map);


  private:
    void generic_id_select(QTree<uint64_t, void>& selection_map, const uint64_t entry_id);
    void generic_id_deselect(QTree<uint64_t, void>& selection_map, const uint64_t entry_id);
    bool generic_id_toggle(QTree<uint64_t, void>& selection_map, const uint64_t entry_id);
    bool generic_id_have_selection(QTree<uint64_t, void>& selection_map);
    bool generic_id_is_selected(QTree<uint64_t, void>& selection_map, const uint64_t entry_id);
    void generic_id_clear_selection(QTree<uint64_t, void>& selection_map) noexcept;

    void clear_resources_selection_impl() noexcept;
    void clear_connections_selection_impl() noexcept;
    void clear_volumes_selection_impl() noexcept;
    void clear_peer_volumes_selection_impl() noexcept;
};

#endif /* SHAREDDATA_H */
