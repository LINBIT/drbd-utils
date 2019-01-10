#ifndef DRBDRESOURCE_H
#define	DRBDRESOURCE_H

#include <new>
#include <memory>
#include <string>

#include <VolumesContainer.h>
#include <DrbdConnection.h>
#include <DrbdVolume.h>
#include <DrbdRole.h>
#include <StateFlags.h>

#include <map_types.h>
// https://github.com/raltnoeder/cppdsaext
#include <dsaext.h>
#include <utils.h>
#include <exceptions.h>

class DrbdResource : public VolumesContainer, public DrbdRole, private StateFlags
{
  public:
    static const std::string PROP_KEY_RES_NAME;

    class ConnectionsIterator : public ConnectionsMap::ValuesIterator
    {
      public:
        ConnectionsIterator(DrbdResource& res_ref) :
            ConnectionsMap::ValuesIterator(*(res_ref.conn_list))
        {
        }

        ConnectionsIterator(const ConnectionsIterator& orig) = default;
        ConnectionsIterator& operator=(const ConnectionsIterator& orig) = default;
        ConnectionsIterator(ConnectionsIterator&& orig) = default;
        ConnectionsIterator& operator=(ConnectionsIterator&& orig) = default;

        virtual ~ConnectionsIterator() noexcept override
        {
        }
    };

    // @throws std::bad_alloc
    DrbdResource(std::string& resource_name);
    DrbdResource(const DrbdResource& orig) = delete;
    DrbdResource& operator=(const DrbdResource& orig) = delete;
    DrbdResource(DrbdResource&& orig) = delete;
    DrbdResource& operator=(DrbdResource&& orig) = delete;
    virtual ~DrbdResource() noexcept override;
    virtual const std::string& get_name() const;

    // @throws std::bad_alloc, dsaext::DuplicateInsertException
    virtual void add_connection(DrbdConnection* conn);
    virtual DrbdConnection* get_connection(const std::string& connection_name) const;
    virtual void remove_connection(const std::string& connection_name);

    // @throws std::bad_alloc, EventMessageException
    virtual void update(PropsMap& event_props);
    virtual ConnectionsIterator connections_iterator();

    using StateFlags::has_mark_state;
    using StateFlags::has_warn_state;
    using StateFlags::has_alert_state;
    using StateFlags::set_mark;
    using StateFlags::set_warn;
    using StateFlags::set_alert;
    using StateFlags::get_state;
    virtual void clear_state_flags() override;
    virtual StateFlags::state update_state_flags() override;
    virtual StateFlags::state child_state_flags_changed() override;
    virtual bool has_role_alert();
    virtual bool has_quorum_alert();

    // Creates (allocates and initializes) a new DrbdResource object from a map of properties
    //
    // @param event_props Reference to the map of properties from a 'drbdsetup events2' line
    // @return Pointer to a newly created DrbdResource object
    // @throws std::bad_alloc, EventMessageException
    static DrbdResource* new_from_props(PropsMap& event_props);

  private:
    const std::string name;
    const std::unique_ptr<ConnectionsMap> conn_list;
    bool role_alert {false};
    bool quorum_alert {false};
};

#endif	/* DRBDRESOURCE_H */
