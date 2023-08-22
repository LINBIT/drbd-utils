#ifndef RESOURCEDIRECTORY_H
#define RESOURCEDIRECTORY_H

#include <default_types.h>
#include <DrbdMonCore.h>
#include <memory>
#include <map_types.h>
// https://github.com/raltnoeder/cppdsaext
#include <dsaext.h>
#include <objects/DrbdResource.h>
#include <objects/DrbdConnection.h>
#include <objects/DrbdVolume.h>
#include <objects/VolumesContainer.h>
#include <StringTokenizer.h>
#include <MessageLog.h>

class ResourceDirectory
{
  public:
    // @throws std::bad_alloc
    ResourceDirectory(MessageLog& log_ref, MessageLog& debug_log_ref);
    virtual ~ResourceDirectory() noexcept;

    ResourcesMap& get_resources_map() noexcept;
    ResourcesMap& get_problem_resources_map() noexcept;

    // @throws std::bad_alloc, EventMessageException
    void create_connection(PropsMap& event_props, const std::string& event_line);
    // @throws std::bad_alloc, EventMessageException
    void create_device(PropsMap& event_props, const std::string& event_line);
    // @throws std::bad_alloc, EventMessageException
    void create_peer_device(PropsMap& event_props, const std::string& event_line);
    // @throws std::bad_alloc, EventMessageException
    void create_resource(PropsMap& event_props, const std::string& event_line);

    // @throws EventMessageException, EventObjectException
    void update_connection(PropsMap& event_props, const std::string& event_line);
    // @throws EventMessageException, EventObjectException
    void update_device(PropsMap& event_props, const std::string& event_line);
    // @throws EventMessageException, EventObjectException
    void update_peer_device(PropsMap& event_props, const std::string& event_line);
    // @throws EventMessageException, EventObjectException
    void update_resource(PropsMap& event_props, const std::string& event_line);

    // @throws std::bad_alloc, EventMessageException, EventObjectException
    void rename_resource(PropsMap& event_props, const std::string& event_line);

    // @throws EventMessageException
    void destroy_connection(PropsMap& event_props, const std::string& event_line);
    // @throws EventMessageException
    void destroy_device(PropsMap& event_props, const std::string& event_line);
    // @throws EventMessageException
    void destroy_peer_device(PropsMap& event_props, const std::string& event_line);
    // @throws EventMessageException
    void destroy_resource(PropsMap& event_props, const std::string& event_line);

    // @throws EventMessageException, EventObjectException
    DrbdConnection& get_connection(DrbdResource& res, PropsMap& event_props, const std::string& event_line);
    // @throws EventMessageException, EventObjectException
    DrbdVolume& get_device(VolumesContainer& vol_con, PropsMap& event_props, const std::string& event_line);
    // @throws EventMessageException, EventObjectException
    DrbdResource& get_resource(PropsMap& event_props, const std::string& event_line);
    // @throws EventMessageException, EventObjectException
    ResourcesMap::Node& get_resource_node(PropsMap& event_props, const std::string& event_line);
    // @throws EventMessageException
    const std::string& lookup_resource_name(PropsMap& event_props, const std::string& event_line);
    // @throws std::bad_alloc
    void problem_resources_update(
        std::string&        res_key,
        DrbdResource&       res,
        StateFlags::state   res_last_state,
        StateFlags::state   res_new_state
    );
    uint32_t get_problem_count() const;

  private:
    // Map of all resources
    std::unique_ptr<ResourcesMap> rsc_map;
    // Map of resources that have some problem
    std::unique_ptr<ResourcesMap> prb_rsc_map;

    MessageLog& log;
    MessageLog& debug_log;
};

#endif /* RESOURCEDIRECTORY_H */
