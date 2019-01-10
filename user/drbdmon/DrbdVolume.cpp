#include <DrbdVolume.h>
#include <DrbdConnection.h>
#include <utils.h>
#include <integerparse.h>

#include "integerparse.h"

const std::string DrbdVolume::PROP_KEY_VOL_NR      = "volume";
const std::string DrbdVolume::PROP_KEY_MINOR       = "minor";
const std::string DrbdVolume::PROP_KEY_DISK        = "disk";
const std::string DrbdVolume::PROP_KEY_PEER_DISK   = "peer-disk";
const std::string DrbdVolume::PROP_KEY_REPLICATION = "replication";
const std::string DrbdVolume::PROP_KEY_CLIENT      = "client";
const std::string DrbdVolume::PROP_KEY_PEER_CLIENT = "peer-client";
const std::string DrbdVolume::PROP_KEY_QUORUM      = "quorum";
const std::string DrbdVolume::PROP_KEY_SYNC_PERC   = "done";

const char* DrbdVolume::DS_LABEL_DISKLESS     = "Diskless";
const char* DrbdVolume::DS_LABEL_ATTACHING    = "Attaching";
const char* DrbdVolume::DS_LABEL_DETACHING    = "Detaching";
const char* DrbdVolume::DS_LABEL_FAILED       = "Failed";
const char* DrbdVolume::DS_LABEL_NEGOTIATING  = "Negotiating";
const char* DrbdVolume::DS_LABEL_INCONSISTENT = "Inconsistent";
const char* DrbdVolume::DS_LABEL_OUTDATED     = "Outdated";
const char* DrbdVolume::DS_LABEL_UNKNOWN      = "DUnknown";
const char* DrbdVolume::DS_LABEL_CONSISTENT   = "Consistent";
const char* DrbdVolume::DS_LABEL_UP_TO_DATE   = "UpToDate";

const char* DrbdVolume::RS_LABEL_OFF                  = "Off";
const char* DrbdVolume::RS_LABEL_ESTABLISHED          = "Established";
const char* DrbdVolume::RS_LABEL_STARTING_SYNC_SOURCE = "StartingSyncS";
const char* DrbdVolume::RS_LABEL_STARTING_SYNC_TARGET = "StartingSyncT";
const char* DrbdVolume::RS_LABEL_WF_BITMAP_SOURCE     = "WFBitMapS";
const char* DrbdVolume::RS_LABEL_WF_BITMAP_TARGET     = "WFBitMapT";
const char* DrbdVolume::RS_LABEL_WF_SYNC_UUID         = "WFSyncUUID";
const char* DrbdVolume::RS_LABEL_SYNC_SOURCE          = "SyncSource";
const char* DrbdVolume::RS_LABEL_SYNC_TARGET          = "SyncTarget";
const char* DrbdVolume::RS_LABEL_PAUSED_SYNC_SOURCE   = "PausedSyncS";
const char* DrbdVolume::RS_LABEL_PAUSED_SYNC_TARGET   = "PausedSyncT";
const char* DrbdVolume::RS_LABEL_VERIFY_SOURCE        = "VerifyS";
const char* DrbdVolume::RS_LABEL_VERIFY_TARGET        = "VerifyT";
const char* DrbdVolume::RS_LABEL_AHEAD                = "Ahead";
const char* DrbdVolume::RS_LABEL_BEHIND               = "Behind";
const char* DrbdVolume::RS_LABEL_UNKNOWN              = "Unknown";

const char* DrbdVolume::CS_LABEL_ENABLED              = "yes";
const char* DrbdVolume::CS_LABEL_DISABLED             = "no";
const char* DrbdVolume::CS_LABEL_UNKNOWN              = "unknown";

const char* DrbdVolume::QU_LABEL_PRESENT              = "yes";
const char* DrbdVolume::QU_LABEL_LOST                 = "no";

// Integer / Fraction separator
const char* DrbdVolume::FRACT_SEPA = ".";

// Maximum value of the sync_perc field - 100 * percent value -> 10,000
const uint16_t DrbdVolume::MAX_SYNC_PERC = 10000;

// Maximum percent value - 100
const uint16_t DrbdVolume::MAX_PERC = 100;

// Maximum fraction value - 2 digits precision, 99
const uint16_t DrbdVolume::MAX_FRACT = 99;

DrbdVolume::DrbdVolume(uint16_t volume_nr) :
    vol_nr(volume_nr)
{
    minor_nr = -1;
    vol_disk_state = DrbdVolume::disk_state::UNKNOWN;
    vol_repl_state = DrbdVolume::repl_state::UNKNOWN;
    vol_client_state = DrbdVolume::client_state::UNKNOWN;
}

const uint16_t DrbdVolume::get_volume_nr() const
{
    return vol_nr;
}

uint16_t DrbdVolume::get_sync_perc() const
{
    return sync_perc;
}

// @throws std::bad_alloc, EventMessageException
void DrbdVolume::update(PropsMap& event_props)
{
    std::string* prop_disk = event_props.get(&PROP_KEY_DISK);
    if (prop_disk == nullptr)
    {
        prop_disk = event_props.get(&PROP_KEY_PEER_DISK);
    }

    if (prop_disk != nullptr)
    {
        vol_disk_state = parse_disk_state(*prop_disk);
    }

    std::string* prop_replication = event_props.get(&PROP_KEY_REPLICATION);
    if (prop_replication != nullptr)
    {
        vol_repl_state = parse_repl_state(*prop_replication);
    }

    std::string* minor_nr_str = event_props.get(&PROP_KEY_MINOR);
    if (minor_nr_str != nullptr)
    {
        try
        {
            minor_nr = DrbdVolume::parse_minor_nr(*minor_nr_str);
        }
        catch (dsaext::NumberFormatException&)
        {
            std::string error_msg("Invalid DRBD event: Invalid minor number");
            std::string debug_info("Invalid minor number");
            throw EventMessageException(&error_msg, &debug_info, nullptr);
        }
    }

    std::string* prop_client = event_props.get(&PROP_KEY_CLIENT);
    if (prop_client == nullptr)
    {
        prop_client = event_props.get(&PROP_KEY_PEER_CLIENT);
    }

    if (prop_client != nullptr)
    {
        vol_client_state = parse_client_state(*prop_client);
    }

    std::string* prop_quorum = event_props.get(&PROP_KEY_QUORUM);
    if (prop_quorum != nullptr)
    {
        quorum_alert = !parse_quorum_state(*prop_quorum);
    }

    {
        bool is_resyncing {false};
        switch (vol_repl_state)
        {
            case DrbdVolume::repl_state::OFF:
                // fall-through
            case DrbdVolume::repl_state::BEHIND:
                // fall-through
            case DrbdVolume::repl_state::STARTING_SYNC_TARGET:
                // fall-through
            case DrbdVolume::repl_state::SYNC_TARGET:
                // fall-through
            case DrbdVolume::repl_state::PAUSED_SYNC_TARGET:
                // fall-through
            case DrbdVolume::repl_state::VERIFY_TARGET:
                is_resyncing = true;
                break;
            case DrbdVolume::repl_state::ESTABLISHED:
                // Whenever the replication state returns to ESTABLISHED, reset
                // the sync percentage, because the drbdsetup events commonly
                // skips reporting 100 percent sync percentage
                sync_perc = MAX_SYNC_PERC;
                break;
            case DrbdVolume::repl_state::AHEAD:
                // fall-through
            case DrbdVolume::repl_state::PAUSED_SYNC_SOURCE:
                // fall-through
            case DrbdVolume::repl_state::STARTING_SYNC_SOURCE:
                // fall-through
            case DrbdVolume::repl_state::SYNC_SOURCE:
                // fall-through
            case DrbdVolume::repl_state::VERIFY_SOURCE:
                // fall-through
            case DrbdVolume::repl_state::WF_BITMAP_SOURCE:
                // fall-through
            case DrbdVolume::repl_state::WF_BITMAP_TARGET:
                // fall-through
            case DrbdVolume::repl_state::WF_SYNC_UUID:
                // fall-through
            case DrbdVolume::repl_state::UNKNOWN:
                // fall-through
            default:
                break;
        }
        if (is_resyncing)
        {
            std::string* prop_sync_perc = event_props.get(&PROP_KEY_SYNC_PERC);
            if (prop_sync_perc != nullptr)
            {
                try
                {
                    sync_perc = parse_sync_perc(*prop_sync_perc);
                }
                catch (dsaext::NumberFormatException&)
                {
                    std::string error_msg("Invalid DRBD event: Invalid sync percentage value");
                    std::string debug_info("Invalid sync percentage value");
                    throw EventMessageException(&error_msg, &debug_info, nullptr);
                }
            }
        }
    }
}

int32_t DrbdVolume::get_minor_nr() const
{
    return minor_nr;
}

// @throws std::bad_alloc, EventMessageException
void DrbdVolume::set_minor_nr(int32_t value)
{
    if (value >= -1 && value < 0x100000)
    {
        minor_nr = value;
    }
    else
    {
        std::string error_msg("Invalid DRBD event: Invalid minor number");
        std::string debug_info("Invalid minor number");
        throw EventMessageException(&error_msg, &debug_info, nullptr);
    }
}

DrbdVolume::disk_state DrbdVolume::get_disk_state() const
{
    return vol_disk_state;
}

const char* DrbdVolume::get_disk_state_label() const
{
    const char* label = DS_LABEL_UNKNOWN;
    switch (vol_disk_state)
    {
        case DrbdVolume::disk_state::ATTACHING:
            label = DS_LABEL_ATTACHING;
            break;
        case DrbdVolume::disk_state::DETACHING:
            label = DS_LABEL_DETACHING;
            break;
        case DrbdVolume::disk_state::CONSISTENT:
            label = DS_LABEL_CONSISTENT;
            break;
        case DrbdVolume::disk_state::DISKLESS:
            label = DS_LABEL_DISKLESS;
            break;
        case DrbdVolume::disk_state::FAILED:
            label = DS_LABEL_FAILED;
            break;
        case DrbdVolume::disk_state::INCONSISTENT:
            label = DS_LABEL_INCONSISTENT;
            break;
        case DrbdVolume::disk_state::NEGOTIATING:
            label = DS_LABEL_NEGOTIATING;
            break;
        case DrbdVolume::disk_state::OUTDATED:
            label = DS_LABEL_OUTDATED;
            break;
        case DrbdVolume::disk_state::UP_TO_DATE:
            label = DS_LABEL_UP_TO_DATE;
            break;
        case DrbdVolume::disk_state::UNKNOWN:
            // fall-through
        default:
            break;
    }
    return label;
}

DrbdVolume::repl_state DrbdVolume::get_replication_state() const
{
    return vol_repl_state;
}

const char* DrbdVolume::get_replication_state_label() const
{
    const char* label = RS_LABEL_UNKNOWN;
    switch (vol_repl_state)
    {
        case DrbdVolume::repl_state::AHEAD:
            label = RS_LABEL_AHEAD;
            break;
        case DrbdVolume::repl_state::BEHIND:
            label = RS_LABEL_BEHIND;
            break;
        case DrbdVolume::repl_state::ESTABLISHED:
            label = RS_LABEL_ESTABLISHED;
            break;
        case DrbdVolume::repl_state::OFF:
            label = RS_LABEL_OFF;
            break;
        case DrbdVolume::repl_state::PAUSED_SYNC_SOURCE:
            label = RS_LABEL_PAUSED_SYNC_SOURCE;
            break;
        case DrbdVolume::repl_state::PAUSED_SYNC_TARGET:
            label = RS_LABEL_PAUSED_SYNC_TARGET;
            break;
        case DrbdVolume::repl_state::STARTING_SYNC_SOURCE:
            label = RS_LABEL_STARTING_SYNC_SOURCE;
            break;
        case DrbdVolume::repl_state::STARTING_SYNC_TARGET:
            label = RS_LABEL_STARTING_SYNC_TARGET;
            break;
        case DrbdVolume::repl_state::SYNC_SOURCE:
            label = RS_LABEL_SYNC_SOURCE;
            break;
        case DrbdVolume::repl_state::SYNC_TARGET:
            label = RS_LABEL_SYNC_TARGET;
            break;
        case DrbdVolume::repl_state::VERIFY_SOURCE:
            label = RS_LABEL_VERIFY_SOURCE;
            break;
        case DrbdVolume::repl_state::VERIFY_TARGET:
            label = RS_LABEL_VERIFY_TARGET;
            break;
        case DrbdVolume::repl_state::WF_BITMAP_SOURCE:
            label = RS_LABEL_WF_BITMAP_SOURCE;
            break;
        case DrbdVolume::repl_state::WF_BITMAP_TARGET:
            label = RS_LABEL_WF_BITMAP_TARGET;
            break;
        case DrbdVolume::repl_state::WF_SYNC_UUID:
            label = RS_LABEL_WF_SYNC_UUID;
            break;
        case DrbdVolume::repl_state::UNKNOWN:
            // fall-through
        default:
            break;
    }
    return label;
}

void DrbdVolume::set_connection(DrbdConnection* conn)
{
    connection = conn;
}

bool DrbdVolume::has_disk_alert()
{
    return disk_alert;
}

bool DrbdVolume::has_replication_warning()
{
    return repl_warn || repl_alert;
}

bool DrbdVolume::has_replication_alert()
{
    return repl_alert;
}

bool DrbdVolume::has_quorum_alert()
{
    return quorum_alert;
}

void DrbdVolume::clear_state_flags()
{
    disk_alert = false;
    repl_warn  = false;
    repl_alert = false;
    StateFlags::clear_state_flags();
}

StateFlags::state DrbdVolume::update_state_flags()
{
    // Reset the state to normal
    // quorum_alert is set directly by update()
    StateFlags::clear_state_flags();
    disk_alert = false;
    repl_warn  = false;
    repl_alert = false;

    // Check the volume's disk state
    switch (vol_disk_state)
    {
        case DrbdVolume::disk_state::UP_TO_DATE:
            // UpToDate disk, no alert
            break;
        case DrbdVolume::disk_state::DISKLESS:
            // If the volume is not configured as a diskless DRBD client,
            // then trigger a disk alert
            if (vol_client_state != DrbdVolume::client_state::ENABLED)
            {
                disk_alert = true;
                set_alert();
            }
            break;
        case DrbdVolume::disk_state::UNKNOWN:
            if (connection == nullptr)
            {
                // Volume is local, always issue an alert for
                // an unknown disk state
                disk_alert = true;
                set_alert();
            }
            else
            {
                // Volume is a peer volume, if the connection
                // to the peer is faulty, do not issue an alert for
                // an unknown disk state
                if (!connection->has_warn_state())
                {
                    disk_alert = true;
                    set_alert();
                }
            }
            break;
        case DrbdVolume::disk_state::ATTACHING:
            // fall-through
        case DrbdVolume::disk_state::CONSISTENT:
            // fall-through
        case DrbdVolume::disk_state::DETACHING:
            // fall-through
        case DrbdVolume::disk_state::FAILED:
            // fall-through
        case DrbdVolume::disk_state::INCONSISTENT:
            // fall-through
        case DrbdVolume::disk_state::NEGOTIATING:
            // fall-through
        case DrbdVolume::disk_state::OUTDATED:
            // fall-through
        default:
            disk_alert = true;
            set_alert();
            break;
    }

    // Check the volume's replication state
    switch (vol_repl_state)
    {
        case DrbdVolume::repl_state::ESTABLISHED:
            // no warning, no alert
            break;
        case DrbdVolume::repl_state::PAUSED_SYNC_SOURCE:
            // fall-through
        case DrbdVolume::repl_state::PAUSED_SYNC_TARGET:
            // fall-through
        case DrbdVolume::repl_state::STARTING_SYNC_SOURCE:
            // fall-through
        case DrbdVolume::repl_state::STARTING_SYNC_TARGET:
            // fall-through
        case DrbdVolume::repl_state::SYNC_SOURCE:
            // fall-through
        case DrbdVolume::repl_state::SYNC_TARGET:
            // fall-through
        case DrbdVolume::repl_state::VERIFY_SOURCE:
            // fall-through
        case DrbdVolume::repl_state::VERIFY_TARGET:
            // fall-through
        case DrbdVolume::repl_state::WF_BITMAP_SOURCE:
            // fall-through
        case DrbdVolume::repl_state::WF_BITMAP_TARGET:
            // fall-through
        case DrbdVolume::repl_state::WF_SYNC_UUID:
            repl_warn = true;
            set_warn();
            break;
        case DrbdVolume::repl_state::UNKNOWN:
            // fall-through
        case DrbdVolume::repl_state::OFF:
            if (connection != nullptr)
            {
                // Volume is a peer volume, if the connection
                // to the peer is faulty, do not issue alerts for
                // inoperative replication
                if (!connection->has_warn_state())
                {
                    repl_alert = true;
                    set_alert();
                }
            }
            // No alert is issued for local (non-peer) volumes,
            // because those always have 'Unknown' replication state
            break;
        case DrbdVolume::repl_state::AHEAD:
            // fall-through
        case DrbdVolume::repl_state::BEHIND:
            // fall-through
        default:
            repl_alert = true;
            set_alert();
            break;
    }

    // Set alert status on the volume if the quorum has been lost
    if (quorum_alert)
    {
        set_alert();
    }

    return obj_state;
}

StateFlags::state DrbdVolume::child_state_flags_changed()
{
    // No-op, the DrbdVolume does not have child objects

    return StateFlags::state::NORM;
}

// @throws std::bad_alloc, EventMessageException
DrbdVolume::disk_state DrbdVolume::parse_disk_state(std::string& state_name)
{
    DrbdVolume::disk_state state = DrbdVolume::disk_state::UNKNOWN;

    if (state_name == DS_LABEL_DISKLESS)
    {
        state = DrbdVolume::disk_state::DISKLESS;
    }
    else
    if (state_name == DS_LABEL_ATTACHING)
    {
        state = DrbdVolume::disk_state::ATTACHING;
    }
    else
    if (state_name == DS_LABEL_DETACHING)
    {
        state = DrbdVolume::disk_state::DETACHING;
    }
    else
    if (state_name == DS_LABEL_FAILED)
    {
        state = DrbdVolume::disk_state::FAILED;
    }
    else
    if (state_name == DS_LABEL_NEGOTIATING)
    {
        state = DrbdVolume::disk_state::NEGOTIATING;
    }
    else
    if (state_name == DS_LABEL_INCONSISTENT)
    {
        state = DrbdVolume::disk_state::INCONSISTENT;
    }
    else
    if (state_name == DS_LABEL_OUTDATED)
    {
        state = DrbdVolume::disk_state::OUTDATED;
    }
    else
    if (state_name == DS_LABEL_CONSISTENT)
    {
        state = DrbdVolume::disk_state::CONSISTENT;
    }
    else
    if (state_name == DS_LABEL_UP_TO_DATE)
    {
        state = DrbdVolume::disk_state::UP_TO_DATE;
    }
    else
    if (state_name != DS_LABEL_UNKNOWN)
    {
        std::string error_msg("Invalid DRBD event: Invalid disk state");
        std::string debug_info("Invalid disk state");
        throw EventMessageException(&error_msg, &debug_info, nullptr);
    }

    return state;
}

// @throws std::bad_alloc, EventMessageException
DrbdVolume::repl_state DrbdVolume::parse_repl_state(std::string& state_name)
{
    DrbdVolume::repl_state state = DrbdVolume::repl_state::UNKNOWN;

    if (state_name == RS_LABEL_AHEAD)
    {
        state = DrbdVolume::repl_state::AHEAD;
    }
    else
    if (state_name == RS_LABEL_BEHIND)
    {
        state = DrbdVolume::repl_state::BEHIND;
    }
    else
    if (state_name == RS_LABEL_ESTABLISHED)
    {
        state = DrbdVolume::repl_state::ESTABLISHED;
    }
    else
    if (state_name == RS_LABEL_OFF)
    {
        state = DrbdVolume::repl_state::OFF;
    }
    else
    if (state_name == RS_LABEL_PAUSED_SYNC_SOURCE)
    {
        state = DrbdVolume::repl_state::PAUSED_SYNC_SOURCE;
    }
    else
    if (state_name == RS_LABEL_PAUSED_SYNC_TARGET)
    {
        state = DrbdVolume::repl_state::PAUSED_SYNC_TARGET;
    }
    else
    if (state_name == RS_LABEL_STARTING_SYNC_SOURCE)
    {
        state = DrbdVolume::repl_state::STARTING_SYNC_SOURCE;
    }
    else
    if (state_name == RS_LABEL_STARTING_SYNC_TARGET)
    {
        state = DrbdVolume::repl_state::STARTING_SYNC_TARGET;
    }
    else
    if (state_name == RS_LABEL_SYNC_SOURCE)
    {
        state = DrbdVolume::repl_state::SYNC_SOURCE;
    }
    else
    if (state_name == RS_LABEL_SYNC_TARGET)
    {
        state = DrbdVolume::repl_state::SYNC_TARGET;
    }
    else
    if (state_name == RS_LABEL_VERIFY_SOURCE)
    {
        state = DrbdVolume::repl_state::VERIFY_SOURCE;
    }
    else
    if (state_name == RS_LABEL_VERIFY_TARGET)
    {
        state = DrbdVolume::repl_state::VERIFY_TARGET;
    }
    else
    if (state_name == RS_LABEL_WF_BITMAP_SOURCE)
    {
        state = DrbdVolume::repl_state::WF_BITMAP_SOURCE;
    }
    else
    if (state_name == RS_LABEL_WF_BITMAP_TARGET)
    {
        state = DrbdVolume::repl_state::WF_BITMAP_TARGET;
    }
    else
    if (state_name == RS_LABEL_WF_SYNC_UUID)
    {
        state = DrbdVolume::repl_state::WF_SYNC_UUID;
    }
    else
    if (state_name != RS_LABEL_UNKNOWN)
    {
        std::string error_msg("Invalid DRBD event: Invalid replication state");
        std::string debug_info("Invalid replication state");
        throw EventMessageException(&error_msg, &debug_info, nullptr);
    }

    return state;
}

// @throws std::bad_alloc, EventMessageException
DrbdVolume::client_state DrbdVolume::parse_client_state(std::string& value_str)
{
    DrbdVolume::client_state state = DrbdVolume::client_state::UNKNOWN;

    if (value_str == CS_LABEL_DISABLED)
    {
        state = DrbdVolume::client_state::DISABLED;
    }
    else
    if (value_str == CS_LABEL_ENABLED)
    {
        state = DrbdVolume::client_state::ENABLED;
    }
    else
    if (value_str != CS_LABEL_UNKNOWN)
    {
        std::string error_msg("Invalid DRBD event: Invalid client (diskless) mode");
        std::string debug_info("Invalid client mode value");
        throw EventMessageException(&error_msg, &debug_info, nullptr);
    }

    return state;
}

// @throws std::bad_alloc, EventMessageException
bool DrbdVolume::parse_quorum_state(std::string& value_str)
{
    bool quorum_present {false};

    if (value_str == QU_LABEL_PRESENT)
    {
        quorum_present = true;
    }
    else
    if (value_str != QU_LABEL_LOST)
    {
        std::string error_msg("Invalid DRBD event: Invalid quorum state");
        std::string debug_info("Invalid quorum state");
        throw EventMessageException(&error_msg, &debug_info, nullptr);
    }

    return quorum_present;
}


// @throws NumberFormatException
uint16_t DrbdVolume::parse_volume_nr(std::string& value_str)
{
    return dsaext::parse_unsigned_int16(value_str);
}

// @throws NumberFormatException
int32_t DrbdVolume::parse_minor_nr(std::string& value_str)
{
    return dsaext::parse_signed_int32(value_str);
}

// @throws NumberFormatException
uint16_t DrbdVolume::parse_sync_perc(std::string& value_str)
{
    uint16_t result {0};
    size_t split_idx = value_str.find(FRACT_SEPA, 0);
    if (split_idx != std::string::npos)
    {
        std::string perc_value_str = value_str.substr(0, split_idx);
        std::string perc_fract_str = value_str.substr(split_idx + 1);

        uint16_t value = dsaext::parse_unsigned_int16(perc_value_str);
        if (value > MAX_PERC)
        {
            throw dsaext::NumberFormatException();
        }
        uint16_t fract = dsaext::parse_unsigned_int16(perc_fract_str);
        if (fract > MAX_FRACT)
        {
            throw dsaext::NumberFormatException();
        }
        result = value * 100 + fract;
        if (result > MAX_SYNC_PERC)
        {
            throw dsaext::NumberFormatException();
        }
    }
    return result;
}

// Creates (allocates and initializes) a new DrbdVolume object from a map of properties
//
// @param event_props Reference to the map of properties from a 'drbdsetup events2' line
// @return Pointer to a newly created DrbdVolume object
// @throws std::bad_alloc, EventMessageException
DrbdVolume* DrbdVolume::new_from_props(PropsMap& event_props)
{
    DrbdVolume* vol {nullptr};
    std::string* number_str = event_props.get(&PROP_KEY_VOL_NR);
    if (number_str != nullptr)
    {
        try
        {
            uint16_t vol_nr = dsaext::parse_unsigned_int16(*number_str);
            vol = new DrbdVolume(vol_nr);
        }
        catch (dsaext::NumberFormatException&)
        {
            // no-op
        }
    }
    if (vol == nullptr)
    {
        throw EventMessageException();
    }
    return vol;
}
