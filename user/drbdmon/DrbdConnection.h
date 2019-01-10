#ifndef DRBDCONNECTION_H
#define	DRBDCONNECTION_H

#include <new>
#include <string>
#include <cstdint>

#include <VolumesContainer.h>
#include <DrbdRole.h>
#include <StateFlags.h>

#include <map_types.h>
// https://github.com/raltnoeder/cppdsaext
#include <dsaext.h>
#include <utils.h>
#include <exceptions.h>

class DrbdConnection : public VolumesContainer, public DrbdRole, private StateFlags
{
  public:
    enum class state : uint16_t
    {
        STANDALONE,
        DISCONNECTING,
        UNCONNECTED,
        TIMEOUT,
        BROKEN_PIPE,
        NETWORK_FAILURE,
        PROTOCOL_ERROR,
        TEAR_DOWN,
        CONNECTING,
        CONNECTED,
        UNKNOWN
    };

    static const std::string PROP_KEY_CONNECTION;
    static const std::string PROP_KEY_CONN_NAME;
    static const std::string PROP_KEY_PEER_NODE_ID;

    static const char* CS_LABEL_STANDALONE;
    static const char* CS_LABEL_DISCONNECTING;
    static const char* CS_LABEL_UNCONNECTED;
    static const char* CS_LABEL_TIMEOUT;
    static const char* CS_LABEL_BROKEN_PIPE;
    static const char* CS_LABEL_NETWORK_FAILURE;
    static const char* CS_LABEL_PROTOCOL_ERROR;
    static const char* CS_LABEL_TEAR_DOWN;
    static const char* CS_LABEL_CONNECTING;
    static const char* CS_LABEL_CONNECTED;
    static const char* CS_LABEL_UNKNOWN;

    // @throws std::bad_alloc
    DrbdConnection(std::string& connection_name, uint8_t node_id);
    DrbdConnection(const DrbdConnection& orig) = delete;
    DrbdConnection& operator=(const DrbdConnection& orig) = delete;
    DrbdConnection(DrbdConnection&& orig) = delete;
    DrbdConnection& operator=(DrbdConnection&& orig) = delete;
    virtual ~DrbdConnection() noexcept override
    {
    }

    virtual const std::string& get_name() const;
    virtual const uint8_t get_node_id() const;

    // @throws std::bad_alloc, EventMessageException
    virtual void update(PropsMap& event_props);

    virtual state get_connection_state() const;
    virtual const char* get_connection_state_label() const;

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
    virtual bool has_connection_alert();
    virtual bool has_role_alert();

    // Creates (allocates and initializes) a new DrbdConnection object from a map of properties
    //
    // @param event_props Reference to the map of properties from a 'drbdsetup events2' line
    // @return Pointer to a newly created DrbdConnection object
    // @throws std::bad_alloc, EventMessageException
    static DrbdConnection* new_from_props(PropsMap& event_props);

    // @throws std::bad_alloc, EventMessageException
    static state parse_state(std::string& state_name);

  private:
    const std::string name;
    const uint8_t node_id;
    state conn_state;
    bool conn_alert {false};
    bool role_alert {false};
};

#endif	/* DRBDCONNECTION_H */
