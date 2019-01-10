#include <DrbdConnection.h>

#include "integerparse.h"

const std::string DrbdConnection::PROP_KEY_CONNECTION = "connection";
const std::string DrbdConnection::PROP_KEY_CONN_NAME = "conn-name";
const std::string DrbdConnection::PROP_KEY_PEER_NODE_ID = "peer-node-id";

const char* DrbdConnection::CS_LABEL_STANDALONE           = "StandAlone";
const char* DrbdConnection::CS_LABEL_DISCONNECTING        = "Disconnecting";
const char* DrbdConnection::CS_LABEL_UNCONNECTED          = "Unconnected";
const char* DrbdConnection::CS_LABEL_TIMEOUT              = "Timeout";
const char* DrbdConnection::CS_LABEL_BROKEN_PIPE          = "BrokenPipe";
const char* DrbdConnection::CS_LABEL_NETWORK_FAILURE      = "NetworkFailure";
const char* DrbdConnection::CS_LABEL_PROTOCOL_ERROR       = "ProtocolError";
const char* DrbdConnection::CS_LABEL_CONNECTING           = "Connecting";
const char* DrbdConnection::CS_LABEL_TEAR_DOWN            = "TearDown";
const char* DrbdConnection::CS_LABEL_CONNECTED            = "Connected";
const char* DrbdConnection::CS_LABEL_UNKNOWN              = "Unknown";

// @throws std::bad_alloc
DrbdConnection::DrbdConnection(std::string& connection_name, uint8_t peer_node_id):
    name(connection_name),
    node_id(peer_node_id)
{
}

const std::string& DrbdConnection::get_name() const
{
    return name;
}

// @throws std::bad_alloc, EventMessageException
void DrbdConnection::update(PropsMap& event_props)
{
    std::string* role_prop = event_props.get(&PROP_KEY_ROLE);
    std::string* conn_prop = event_props.get(&PROP_KEY_CONNECTION);

    if (role_prop != nullptr)
    {
        role = parse_role(*role_prop);
    }

    if (conn_prop != nullptr)
    {
        conn_state = parse_state(*conn_prop);
    }
}

const uint8_t DrbdConnection::get_node_id() const
{
    return node_id;
}

DrbdConnection::state DrbdConnection::get_connection_state() const
{
    return conn_state;
}

const char* DrbdConnection::get_connection_state_label() const
{
    const char* label = CS_LABEL_UNKNOWN;
    switch (conn_state)
    {
        case DrbdConnection::state::BROKEN_PIPE:
            label = CS_LABEL_BROKEN_PIPE;
            break;
        case DrbdConnection::state::CONNECTED:
            label = CS_LABEL_CONNECTED;
            break;
        case DrbdConnection::state::CONNECTING:
            label = CS_LABEL_CONNECTING;
            break;
        case DrbdConnection::state::DISCONNECTING:
            label = CS_LABEL_DISCONNECTING;
            break;
        case DrbdConnection::state::NETWORK_FAILURE:
            label = CS_LABEL_NETWORK_FAILURE;
            break;
        case DrbdConnection::state::PROTOCOL_ERROR:
            label = CS_LABEL_PROTOCOL_ERROR;
            break;
        case DrbdConnection::state::STANDALONE:
            label = CS_LABEL_STANDALONE;
            break;
        case DrbdConnection::state::TEAR_DOWN:
            label = CS_LABEL_TEAR_DOWN;
            break;
        case DrbdConnection::state::TIMEOUT:
            label = CS_LABEL_TIMEOUT;
            break;
        case DrbdConnection::state::UNCONNECTED:
            label = CS_LABEL_UNCONNECTED;
            break;
        case DrbdConnection::state::UNKNOWN:
            // fall-through
        default:
            break;
    }
    return label;
}

bool DrbdConnection::has_connection_alert()
{
    return conn_alert;
}

bool DrbdConnection::has_role_alert()
{
    return role_alert;
}

void DrbdConnection::clear_state_flags()
{
    conn_alert = false;
    role_alert = false;
    StateFlags::clear_state_flags();
}

StateFlags::state DrbdConnection::update_state_flags()
{
    StateFlags::state last_state = obj_state;

    conn_alert = false;
    role_alert = false;

    // Check the connection state
    switch (conn_state)
    {
        case DrbdConnection::state::CONNECTED:
            // no warning, no alert
            break;
        case DrbdConnection::state::BROKEN_PIPE:
            // fall-through
        case DrbdConnection::state::CONNECTING:
            // fall-through
        case DrbdConnection::state::DISCONNECTING:
            // fall-through
        case DrbdConnection::state::NETWORK_FAILURE:
            // fall-through
        case DrbdConnection::state::PROTOCOL_ERROR:
            // fall-through
        case DrbdConnection::state::STANDALONE:
            // fall-through
        case DrbdConnection::state::TEAR_DOWN:
            // fall-through
        case DrbdConnection::state::TIMEOUT:
            // fall-through
        case DrbdConnection::state::UNCONNECTED:
            // fall-through
        case DrbdConnection::state::UNKNOWN:
            // fall-through
        default:
            conn_alert = true;
            set_alert();
            break;
    }

    // Check the peer resource role
    switch (role)
    {
        case DrbdRole::resource_role::PRIMARY:
            // fall-through
        case DrbdRole::resource_role::SECONDARY:
            // no warning, no alert
            break;
        case DrbdRole::resource_role::UNKNOWN:
            // fall-through
        default:
            // If there is no alert for the connection state,
            // but the role is still unknown, issue an alert
            // for the peer role
            if (!conn_alert)
            {
                role_alert = true;
            }
            set_alert();
            break;
    }

    // If the state may have improved, adjust to child objects status
    if (last_state != StateFlags::state::NORM)
    {
        static_cast<void> (child_state_flags_changed());
    }

    return obj_state;
}

StateFlags::state DrbdConnection::child_state_flags_changed()
{
    if (!(role_alert || conn_alert))
    {
        StateFlags::clear_state_flags();

        // If any marks/warnings/alerts are set on peer volumes, mark the connection
        VolumesMap::ValuesIterator peer_vol_iter = volumes_iterator();
        size_t peer_vol_count = peer_vol_iter.get_size();
        for (size_t peer_vol_index = 0; peer_vol_index < peer_vol_count; ++peer_vol_index)
        {
            DrbdVolume& peer_vol = *(peer_vol_iter.next());
            if (peer_vol.has_mark_state())
            {
                // Peer volume is in an abnormal state, mark this connection
                set_mark();
                break;
            }
        }
    }

    return obj_state;
}

// @throws std::bad_alloc, EventMessageException
DrbdConnection::state DrbdConnection::parse_state(std::string& state_name)
{
    DrbdConnection::state state = DrbdConnection::state::UNKNOWN;

    if (state_name == CS_LABEL_STANDALONE)
    {
        state = DrbdConnection::state::STANDALONE;
    }
    else
    if (state_name == CS_LABEL_CONNECTING)
    {
        state = DrbdConnection::state::CONNECTING;
    }
    else
    if (state_name == CS_LABEL_DISCONNECTING)
    {
        state = DrbdConnection::state::DISCONNECTING;
    }
    else
    if (state_name == CS_LABEL_UNCONNECTED)
    {
        state = DrbdConnection::state::UNCONNECTED;
    }
    else
    if (state_name == CS_LABEL_TIMEOUT)
    {
        state = DrbdConnection::state::TIMEOUT;
    }
    else
    if (state_name == CS_LABEL_BROKEN_PIPE)
    {
        state = DrbdConnection::state::BROKEN_PIPE;
    }
    else
    if (state_name == CS_LABEL_NETWORK_FAILURE)
    {
        state = DrbdConnection::state::NETWORK_FAILURE;
    }
    else
    if (state_name == CS_LABEL_PROTOCOL_ERROR)
    {
        state = DrbdConnection::state::PROTOCOL_ERROR;
    }
    else
    if (state_name == CS_LABEL_TEAR_DOWN)
    {
        state = DrbdConnection::state::TEAR_DOWN;
    }
    else
    if (state_name == CS_LABEL_CONNECTED)
    {
        state = DrbdConnection::state::CONNECTED;
    }
    else
    if (state_name != CS_LABEL_UNKNOWN)
    {
        std::string error_msg("Invalid DRBD event: Invalid connection state");
        std::string debug_info("Invalid connection state value");
        throw EventMessageException(&error_msg, &debug_info, nullptr);
    }

    return state;
}


// Creates (allocates and initializes) a new DrbdConnection object from a map of properties
//
// @param event_props Reference to the map of properties from a 'drbdsetup events2' line
// @return Pointer to a newly created DrbdConnection object
// @throws std::bad_alloc, EventMessageException
DrbdConnection* DrbdConnection::new_from_props(PropsMap& event_props)
{
    DrbdConnection* new_conn {nullptr};
    std::string* conn_name = event_props.get(&PROP_KEY_CONN_NAME);
    std::string* node_id_str = event_props.get(&PROP_KEY_PEER_NODE_ID);
    if (conn_name != nullptr && node_id_str != nullptr)
    {
        try
        {
            uint8_t new_node_id = dsaext::parse_unsigned_int8(*node_id_str);
            new_conn = new DrbdConnection(*conn_name, new_node_id);
        }
        catch (dsaext::NumberFormatException&)
        {
            // no-op
        }
    }
    if (new_conn == nullptr)
    {
        std::string debug_info("Missing connection name, node id or unparsable node id");
        throw EventMessageException(nullptr, &debug_info, nullptr);
    }
    return new_conn;
}
