#include <GenericDisplay.h>
#include <Display.h>
#include <DrbdMon.h>

const char* Display::ANSI_CLEAR = "\x1B[f\x1B[0J\x1B[f";
const char* Display::ANSI_CLEAR_LINE = "\x1B[K";

// Format of the header line
const char* Display::FORMAT_HEADER = "\x1B[1;37;44m";

// Resource formats
const char* Display::RES_LABEL       = "\x1B[0;30;42mRES:\x1B[0m";
const char* Display::RES_FORMAT_NAME = "\x1B[0;4;32m";

// Role formats
const char* Display::ROLE_LABEL            = "\x1B[0m";
const char* Display::ROLE_FORMAT_PRIMARY   = "\x1B[0;1;36m";
const char* Display::ROLE_FORMAT_SECONDARY = "\x1B[0;36m";

// Connection formats
const char* Display::CONN_LABEL             = "\x1B[0;37;44mP:\x1B[0m";
const char* Display::CONN_FORMAT_NAME       = "\x1B[0;4;37m";
const char* Display::CONN_FORMAT_NODE_ID    = "\x1B[0m";
const char* Display::CONN_FORMAT_STATE_NORM = "\x1B[0;32m";

// Volume formats
const char* Display::VOL_LABEL               = "\x1B[0;30;46mV:\x1B[0m";
const char* Display::VOL_LABEL_MINOR         = "\x1B[0mminor #";
const char* Display::VOL_FORMAT_NR           = "\x1B[0;32m";
const char* Display::VOL_FORMAT_MINOR        = "\x1B[0;36m";
const char* Display::VOL_FORMAT_STATE_NORM   = "\x1B[0;1;32m";
const char* Display::VOL_FORMAT_STATE_CLIENT = "\x1B[0;1;33;44m";
const char* Display::VOL_FORMAT_REPL_NORM    = "\x1B[0;32m";

// Generic formats
const char* Display::FORMAT_WARN  = "\x1B[0;30;43m";
const char* Display::FORMAT_ALERT = "\x1B[1;33;41m";
const char* Display::FORMAT_RESET = "\x1B[0m";

const uint16_t Display::MIN_SIZE_X =   40;
const uint16_t Display::MAX_SIZE_X = 1024;
const uint16_t Display::MIN_SIZE_Y =   15;
const uint16_t Display::MAX_SIZE_Y = 1024;

Display::Display(std::ostream& out_ref, ResourcesMap& resources_map_ref) :
    resources_map(resources_map_ref),
    out(out_ref)
{
}

void Display::clear()
{
    out << ANSI_CLEAR;
}

void Display::initial_display()
{
    clear();
    out << "Reading initial DRBD status" << std::endl;
}

void Display::status_display()
{
    clear();
    if (show_header)
    {
        display_header();
    }
    list_resources();
}

void Display::display_header() const
{
    if (show_header)
    {
        out << FORMAT_HEADER << ANSI_CLEAR_LINE <<
            DrbdMon::PROGRAM_NAME << " v" << DrbdMon::VERSION
            << std::endl;
    }
}

void Display::list_resources()
{
    ResourcesMap::ValuesIterator iter(resources_map);
    size_t count = iter.get_size();
    for (size_t index = 0; index < count; ++index)
    {
        DrbdResource& res = *(iter.next());

        DrbdRole::resource_role role = res.get_role();
        const char* role_label = res.get_role_label();
        const std::string& name = res.get_name();

        const char* role_format = FORMAT_ALERT;
        if (role == DrbdRole::resource_role::PRIMARY)
        {
            role_format = ROLE_FORMAT_PRIMARY;
        }
        else
        if (role == DrbdRole::resource_role::SECONDARY)
        {
            role_format = ROLE_FORMAT_SECONDARY;
        }

        out << RES_LABEL << RES_FORMAT_NAME << std::left << std::setw(32) << name.c_str() <<
            FORMAT_RESET << "       " <<
            role_format << std::setw(15) << role_label << FORMAT_RESET << std::endl;

        list_volumes(res);
        list_connections(res);
    }
    if (count == 0)
    {
        out << RES_FORMAT_NAME << "No active DRBD resources.\n" << std::endl;
    }
}

void Display::list_connections(DrbdResource& res)
{
    ConnectionsMap::ValuesIterator iter = res.connections_iterator();
    size_t count = iter.get_size();
    for (size_t index = 0; index < count; ++index)
    {
        DrbdConnection& conn = *(iter.next());

        bool display_peer_volumes = true;

        DrbdRole::resource_role role = conn.get_role();
        DrbdConnection::state state = conn.get_connection_state();
        const char* role_label = conn.get_role_label();
        const char* state_label = conn.get_connection_state_label();
        const std::string& name = conn.get_name();

        const char* conn_format = FORMAT_ALERT;
        if (state == DrbdConnection::state::CONNECTED)
        {
            conn_format = CONN_FORMAT_STATE_NORM;
        }

        // Do not display the list of peer volumes if it is not
        // surprising that they are all in an unknown state
        switch (state)
        {
            case DrbdConnection::state::NETWORK_FAILURE:
                // fall-through
            case DrbdConnection::state::PROTOCOL_ERROR:
                // fall-through
            case DrbdConnection::state::STANDALONE:
                // fall-through
            case DrbdConnection::state::TEAR_DOWN:
                // fall-through
            case DrbdConnection::state::CONNECTING:
                // fall-through
            case DrbdConnection::state::UNCONNECTED:
                display_peer_volumes = false;
                break;
            case DrbdConnection::state::DISCONNECTING:
                // fall-through
            case DrbdConnection::state::TIMEOUT:
                // fall-through
            case DrbdConnection::state::BROKEN_PIPE:
                // fall-through
            case DrbdConnection::state::CONNECTED:
                // fall-through
            case DrbdConnection::state::UNKNOWN:
                // fall-through
            default:
                break;
        }

        const char* role_format = FORMAT_ALERT;
        if (role == DrbdRole::resource_role::PRIMARY)
        {
            role_format = ROLE_FORMAT_PRIMARY;
        }
        else
        if (role == DrbdRole::resource_role::SECONDARY)
        {
            role_format = ROLE_FORMAT_SECONDARY;
        }
        else
        {
            // Do not mark an unknown role as an alert if
            // the role is unknown because the network connection
            // is faulty
            switch (state)
            {
                case DrbdConnection::state::BROKEN_PIPE:
                    // fall-through
                case DrbdConnection::state::DISCONNECTING:
                    // fall-through
                case DrbdConnection::state::NETWORK_FAILURE:
                    // fall-through
                case DrbdConnection::state::PROTOCOL_ERROR:
                    // fall-through
                case DrbdConnection::state::TEAR_DOWN:
                    // fall-through
                case DrbdConnection::state::TIMEOUT:
                    // fall-through
                case DrbdConnection::state::UNCONNECTED:
                    role_format = FORMAT_WARN;
                    break;
                case DrbdConnection::state::CONNECTING:
                    // fall-through
                case DrbdConnection::state::STANDALONE:
                    role_format = ROLE_FORMAT_SECONDARY;
                    break;
                case DrbdConnection::state::CONNECTED:
                    // fall-through
                case DrbdConnection::state::UNKNOWN:
                    // fall-through
                default:
                    // All other states are marked as an alert
                    break;
            }
        }

        out << "    " <<
            CONN_LABEL << CONN_FORMAT_NAME << std::left << std::setw(20) << name.c_str() <<
            FORMAT_RESET << " " <<
            conn_format << std::left << std::setw(15) << state_label <<
            FORMAT_RESET << " " <<
            role_format << std::left << std::setw(10) << role_label <<
            FORMAT_RESET << std::endl;

        if (display_peer_volumes)
        {
            list_peer_volumes(conn);
        }
    }
}

void Display::list_volumes(DrbdResource& res)
{
    VolumesMap::ValuesIterator iter = res.volumes_iterator();
    size_t count = iter.get_size();
    for (size_t index = 0; index < count; ++index)
    {
        DrbdVolume& vol = *(iter.next());

        uint16_t vol_nr = vol.get_volume_nr();
        int32_t minor_nr = vol.get_minor_nr();
        DrbdVolume::disk_state disk_state = vol.get_disk_state();
        const char* disk_label = vol.get_disk_state_label();

        const char* disk_format = FORMAT_ALERT;
        if (disk_state == DrbdVolume::disk_state::UP_TO_DATE)
        {
            disk_format = VOL_FORMAT_STATE_NORM;
        }
        else
        if (disk_state == DrbdVolume::disk_state::DISKLESS)
        {
            disk_format = VOL_FORMAT_STATE_CLIENT;
        }

        out << "    " <<
            VOL_LABEL << VOL_FORMAT_NR <<
            std::right << std::setw(2) << static_cast<long> (vol_nr) <<
            FORMAT_RESET << " " <<
            disk_format << std::left << std::setw(14) << disk_label <<
            FORMAT_RESET << "    " <<
            VOL_LABEL_MINOR << VOL_FORMAT_MINOR << std::right << std::setw(4) << static_cast<long> (minor_nr) <<
            FORMAT_RESET << std::endl;
    }
}

void Display::list_peer_volumes(DrbdConnection& conn)
{
    VolumesMap::ValuesIterator iter = conn.volumes_iterator();
    size_t count = iter.get_size();
    for (size_t index = 0; index < count; ++index)
    {
        DrbdVolume& vol = *(iter.next());

        DrbdVolume::disk_state disk_state = vol.get_disk_state();
        DrbdVolume::repl_state repl_state = vol.get_replication_state();

        bool disk_state_norm = (disk_state == DrbdVolume::disk_state::UP_TO_DATE ||
                                disk_state == DrbdVolume::disk_state::DISKLESS);
        bool repl_state_norm = (repl_state == DrbdVolume::repl_state::ESTABLISHED);
        if (!hide_norm_peer_volumes || !disk_state_norm || !repl_state_norm)
        {
            uint16_t vol_nr = vol.get_volume_nr();
            const char* disk_label = vol.get_disk_state_label();
            const char* repl_label = vol.get_replication_state_label();

            const char* disk_format = FORMAT_ALERT;
            if (disk_state == DrbdVolume::disk_state::UP_TO_DATE)
            {
                disk_format = VOL_FORMAT_STATE_NORM;
            }
            else
            if (disk_state == DrbdVolume::disk_state::DISKLESS)
            {
                disk_format = VOL_FORMAT_STATE_CLIENT;
            }

            bool show_replication = true;
            const char* repl_format = FORMAT_ALERT;
            switch (repl_state)
            {
                case DrbdVolume::repl_state::ESTABLISHED:
                    show_replication = false;
                    break;
                case DrbdVolume::repl_state::SYNC_SOURCE:
                    // fall-through
                case DrbdVolume::repl_state::SYNC_TARGET:
                    // fall-through
                    repl_format = VOL_FORMAT_REPL_NORM;
                    break;
                case DrbdVolume::repl_state::STARTING_SYNC_SOURCE:
                    // fall-through
                case DrbdVolume::repl_state::STARTING_SYNC_TARGET:
                    // fall-through
                case DrbdVolume::repl_state::WF_BITMAP_SOURCE:
                    // fall-through
                case DrbdVolume::repl_state::WF_BITMAP_TARGET:
                    // fall-through
                case DrbdVolume::repl_state::WF_SYNC_UUID:
                    // fall-through
                case DrbdVolume::repl_state::AHEAD:
                    // fall-through
                case DrbdVolume::repl_state::BEHIND:
                    repl_format = FORMAT_WARN;
                    break;
                case DrbdVolume::repl_state::OFF:
                    // fall-through
                case DrbdVolume::repl_state::PAUSED_SYNC_SOURCE:
                    // fall-through
                case DrbdVolume::repl_state::PAUSED_SYNC_TARGET:
                    // fall-through
                case DrbdVolume::repl_state::VERIFY_SOURCE:
                    // fall-through
                case DrbdVolume::repl_state::VERIFY_TARGET:
                    // fall-through
                case DrbdVolume::repl_state::UNKNOWN:
                    // fall-through
                default:
                    // all other states are marked as an alert
                    break;
            }

            out << "        " <<
                VOL_LABEL << VOL_FORMAT_NR <<
                std::right << std::setw(2) << static_cast<long> (vol_nr) <<
                FORMAT_RESET << " " <<
                disk_format << std::left << std::setw(15) << disk_label <<
                FORMAT_RESET << " ";

            if (show_replication)
            {
                out << repl_format << std::left << std::setw(15) << repl_label;
            }
            else
            {
                out << std::setw(15) << "";
            }

            out << FORMAT_RESET << std::endl;
        }
    }
}

void Display::set_terminal_size(uint16_t size_x, uint16_t size_y)
{
    if (size_x >= MIN_SIZE_X)
    {
        if (size_x <= MAX_SIZE_X)
        {
            term_x = size_x;
        }
        else
        {
            term_x = MAX_SIZE_X;
        }
    }
    else
    {
        term_x = MIN_SIZE_X;
    }

    if (size_y >= MIN_SIZE_Y)
    {
        if (size_y <= MAX_SIZE_Y)
        {
            term_y = size_y;
        }
        else
        {
            term_y = MAX_SIZE_Y;
        }
    }
    else
    {
        term_y = MIN_SIZE_Y;
    }
}

void Display::key_pressed(const char key)
{
}
