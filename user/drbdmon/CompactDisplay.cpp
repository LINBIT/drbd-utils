#include <iostream>
#include <cstdio>

#include <CompactDisplay.h>
#include <DrbdMon.h>
#include <ConfigOption.h>

#include <cstring>
#include <cstdio>
#include <cstdarg>

extern "C"
{
    #include <pthread.h>
    #include <errno.h>
}

const std::string CompactDisplay::OPT_NO_HEADER_KEY("no-header");
const std::string CompactDisplay::OPT_NO_HOTKEYS_KEY("no-hotkeys");
const std::string CompactDisplay::OPT_ASCII_KEY("ascii");

const std::string CompactDisplay::HDR_SPACER(" | ");
const std::string CompactDisplay::NODE_DSP_PREFIX("Node ");
const std::string CompactDisplay::TRUNC_MARKER("...");

const ConfigOption CompactDisplay::OPT_NO_HEADER(true, OPT_NO_HEADER_KEY);
const ConfigOption CompactDisplay::OPT_NO_HOTKEYS(true, OPT_NO_HOTKEYS_KEY);
const ConfigOption CompactDisplay::OPT_ASCII(true, OPT_ASCII_KEY);

// Generic formats
const char* CompactDisplay::F_NORM  = "\x1b[0;32m";
const char* CompactDisplay::F_WARN  = "\x1b[1;33;45m";
const char* CompactDisplay::F_ALERT = "\x1b[1;33;41m";
const char* CompactDisplay::F_RESET = "\x1b[0m";

const char* CompactDisplay::F_MARK  = "\x1b[1;33;41m";

const char* CompactDisplay::F_HEADER= "\x1b[1;37;44m";

const char* CompactDisplay::F_RES_NORM   = "\x1b[0;30;42m";
const char* CompactDisplay::F_VOL_NORM   = "\x1b[0;30;46m";
const char* CompactDisplay::F_VOL_CLIENT = "\x1b[0;1;37;44m";
const char* CompactDisplay::F_VOL_MINOR  = "\x1b[0;4;36m";
const char* CompactDisplay::F_CONN_NORM  = "\x1b[0;37;44m";
const char* CompactDisplay::F_PRIMARY    = "\x1b[1;36m";
const char* CompactDisplay::F_SECONDARY  = "\x1b[0;36m";

// Foreground color for the 'Primary' role highlighting
// of connections (peer resource role)
const char* CompactDisplay::F_CONN_PRI_FG = "\x1b[1;36;44m";

const char* CompactDisplay::F_RES_NAME   = "\x1b[0;4;32m";
const char* CompactDisplay::F_RES_COUNT  = "\x1b[0;32mResources: %6llu\x1b[0m";
const char* CompactDisplay::F_PRB_COUNT  = "\x1b[0;1;31m (%llu degraded)\x1b[0m";

const char* CompactDisplay::F_CURSOR_POS = "\x1b[%u;%uH";
const char* CompactDisplay::F_HOTKEY     = "\x1b[0;30;47m%c\x1b[0;37;44m %s \033[0m";
const char* CompactDisplay::F_ALERT_HOTKEY = "\x1b[0;30;47m%c\x1b[1;33;41m %s \033[0m";

const char* CompactDisplay::UTF8_PRIMARY   = "\xE2\x9A\xAB";
const char* CompactDisplay::UTF8_SECONDARY = " "; // otherwise UTF-8 E2 9A AA
const char* CompactDisplay::UTF8_CONN_GOOD = "\xE2\x87\x84";
const char* CompactDisplay::UTF8_CONN_BAD  = "\xE2\x86\xAF";
const char* CompactDisplay::UTF8_DISK_GOOD = "\xE2\x97\x8E";
const char* CompactDisplay::UTF8_DISK_BAD  = "\xE2\x9C\x97";
const char* CompactDisplay::UTF8_MARK_OFF  = "\xE2\x9A\xAC";
const char* CompactDisplay::UTF8_MARK_ON   = "\xE2\xA4\xB7";

const char* CompactDisplay::ASCII_PRIMARY   = "*";
const char* CompactDisplay::ASCII_SECONDARY = " ";
const char* CompactDisplay::ASCII_CONN_GOOD = "+";
const char* CompactDisplay::ASCII_CONN_BAD  = "/";
const char* CompactDisplay::ASCII_DISK_GOOD = "+";
const char* CompactDisplay::ASCII_DISK_BAD  = "/";
const char* CompactDisplay::ASCII_MARK_OFF  = " ";
const char* CompactDisplay::ASCII_MARK_ON   = "+";

const char* CompactDisplay::ANSI_CLEAR      = "\x1b[H\x1b[2J";
const char* CompactDisplay::ANSI_CLEAR_LINE = "\x1b[K";

const char* CompactDisplay::ANSI_CURSOR_OFF = "\x1b[?25l";
const char* CompactDisplay::ANSI_CURSOR_ON  = "\x1b[?25h";

const char CompactDisplay::HOTKEY_PGUP = '<';
const char CompactDisplay::HOTKEY_PGDN = '>';
const char CompactDisplay::HOTKEY_PGZERO = '1';

const std::string CompactDisplay::LABEL_MESSAGES    = "Messages";
const std::string CompactDisplay::LABEL_MONITOR     = "Monitor";
const std::string CompactDisplay::LABEL_PROBLEMS    = "Problems";
const std::string CompactDisplay::LABEL_STATUS      = "Status";
const std::string CompactDisplay::LABEL_PGUP        = "PgUp";
const std::string CompactDisplay::LABEL_PGDN        = "PgDn";
const std::string CompactDisplay::LABEL_PGZERO      = "Pg1";

const uint16_t CompactDisplay::MIN_SIZE_X =   40;
const uint16_t CompactDisplay::MAX_SIZE_X = 1024;
const uint16_t CompactDisplay::MIN_SIZE_Y =   15;
const uint16_t CompactDisplay::MAX_SIZE_Y = 1024;

const int CompactDisplay::RES_NAME_WIDTH   = 32;
const int CompactDisplay::ROLE_WIDTH       = 10;
const int CompactDisplay::VOL_NR_WIDTH     = 2;
const int CompactDisplay::MINOR_NR_WIDTH   = 4;
const int CompactDisplay::CONN_NAME_WIDTH  = 20;
const int CompactDisplay::CONN_STATE_WIDTH = 20;
const int CompactDisplay::DISK_STATE_WIDTH = 20;
const int CompactDisplay::REPL_STATE_WIDTH = 20;

const uint16_t CompactDisplay::INDENT_STEP_SIZE     = 4;
const uint16_t CompactDisplay::OUTPUT_BUFFER_SIZE   = 1024;

const uint16_t CompactDisplay::MIN_NODENAME_DSP_LENGTH = 4;
const uint32_t CompactDisplay::MAX_YIELD_LOOP = 10;

// @throws std::bad_alloc
CompactDisplay::CompactDisplay(
    DrbdMon& drbdmon_ref,
    ResourcesMap& resources_map_ref,
    MessageLog&   log_ref,
    HotkeysMap&   hotkeys_info_ref,
    const std::string* const node_name_ref
):
    drbdmon(drbdmon_ref),
    resources_map(resources_map_ref),
    log(log_ref),
    hotkeys_info(hotkeys_info_ref),
    node_name(node_name_ref)
{
    // Allocate and initialize the indent buffer
    indent_buffer_mgr = std::unique_ptr<char[]>(new char[INDENT_STEP_SIZE + 1]);
    indent_buffer = indent_buffer_mgr.get();
    for (size_t index = 0; index < INDENT_STEP_SIZE; ++index)
    {
        indent_buffer[index] = ' ';
    }
    indent_buffer[INDENT_STEP_SIZE] = '\0';

    output_buffer_mgr = std::unique_ptr<char[]>(new char[OUTPUT_BUFFER_SIZE]);
    output_buffer = output_buffer_mgr.get();

    // Hide the cursor
    write_text(ANSI_CURSOR_OFF);

    hotkeys_info.append(&HOTKEY_PGZERO, &LABEL_PGZERO);
    hotkeys_info.append(&HOTKEY_PGUP, &LABEL_PGUP);
    hotkeys_info.append(&HOTKEY_PGDN, &LABEL_PGDN);

    node_label = std::unique_ptr<std::string>(new std::string(HDR_SPACER));
    *node_label += NODE_DSP_PREFIX;
}

CompactDisplay::~CompactDisplay() noexcept
{
    // Show the cursor
    write_text(ANSI_CURSOR_ON);
}

void CompactDisplay::clear()
{
    write_text(ANSI_CLEAR);

    current_x = 0;
    current_y = 0;
}

void CompactDisplay::initial_display()
{
    clear();
    display_header();

    if (log.has_entries() && dsp_msg_active)
    {
        log.display_messages(stdout);
    }
    else
    {
        if (!log.has_entries())
        {
            dsp_msg_active = false;
        }

        write_fmt("%sReading initial DRBD status...%s\n", F_RES_NAME, F_RESET);
    }

    // End line
    next_line();

    display_hotkeys_info();
}

void CompactDisplay::status_display()
{
    uint32_t current_page = page;
    page_start = (term_y - 4) * current_page;
    page_end   = (term_y - 4) * (current_page + 1) - 1;

    problem_check();
    clear();
    display_header();

    if (log.has_entries() && dsp_msg_active)
    {
        log.display_messages(stdout);
    }
    else
    {
        if (!log.has_entries())
        {
            dsp_msg_active = false;
        }

        bool resources_listed = list_resources();
        if (!resources_listed)
        {
            if (dsp_problems_active)
            {
                write_fmt("%sNo resources with problems to display.%s\n", F_RES_NAME, F_RESET);
            }
            else
            {
                write_fmt("%sNo active DRBD resources.%s\n", F_RES_NAME, F_RESET);
            }
        }
    }

    // End line
    next_line();

    display_counts();

    display_hotkeys_info();
}

void CompactDisplay::display_header() const
{
    if (show_header)
    {
        write_fmt("%s%s%s v%s", F_HEADER, ANSI_CLEAR_LINE,
                  DrbdMon::PROGRAM_NAME.c_str(), DrbdMon::VERSION.c_str());

        if (node_name != nullptr)
        {
            if (enable_term_size)
            {
                // "%s v%s" format for PROGRAM_NAME and VERSION
                uint16_t x_pos = DrbdMon::PROGRAM_NAME.length() + DrbdMon::VERSION.length() + 2;
                if (term_x >= x_pos)
                {
                    uint16_t free_x = term_x - x_pos;
                    size_t label_length = node_label->length();
                    if (free_x >= label_length)
                    {
                        free_x -= label_length;
                        size_t name_length = node_name->length();
                        if (free_x > name_length)
                        {
                            // Entire node name fits on the header line
                            write_text(node_label->c_str());
                            write_text(node_name->c_str());
                        }
                        else
                        {
                            if (free_x > MIN_NODENAME_DSP_LENGTH + TRUNC_MARKER.length())
                            {
                                free_x -= TRUNC_MARKER.length() + 1;
                                // Minimum node name length and truncation marker fit on the header line
                                write_text(node_label->c_str());
                                size_t write_length = free_x < name_length ? free_x : name_length;
                                write_buffer(node_name->c_str(), write_length);
                                write_text(TRUNC_MARKER.c_str());
                            }
                        }
                    }
                }
            }
            else
            {
                write_text(node_label->c_str());
                write_text(node_name->c_str());
            }
        }
        write_char('\n');

        // Print right-aligned page number if the terminal size is known
        if (enable_term_size && term_y > MIN_SIZE_Y && term_x > 15)
        {
            write_fmt(F_CURSOR_POS, 2, static_cast<int> (term_x) - 15);
            write_text(F_RESET);
            write_fmt("Page %5lu\n", static_cast<long> (page) + 1);
        }
        else
        {
            write_char('\n');
        }
    }
}

void CompactDisplay::display_hotkeys_info() const
{
    if (show_hotkeys)
    {
        if (enable_term_size)
        {
            write_fmt(F_CURSOR_POS, static_cast<int> (term_y), 1);
        }

        HotkeysMap::NodesIterator hotkeys_iter(hotkeys_info);
        size_t count = hotkeys_iter.get_size();
        size_t index = 0;
        while (index < count)
        {
            HotkeysMap::Node* node = hotkeys_iter.next();
            const char hotkey = *(node->get_key());
            const std::string& description = *(node->get_value());
            if (index > 0)
            {
                write_char(' ');
            }
            write_fmt(F_HOTKEY, hotkey, description.c_str());
            ++index;
        }

        if (index > 0)
        {
            write_char(' ');
        }
        if (dsp_problems_active)
        {
            write_fmt(F_HOTKEY, 'p', LABEL_STATUS.c_str());
        }
        else
        {
            const char* format = problem_alert ? F_ALERT_HOTKEY : F_HOTKEY;
            write_fmt(format, 'p', LABEL_PROBLEMS.c_str());
        }

        if (dsp_msg_active)
        {
            write_char(' ');
            write_fmt(F_HOTKEY, 'm', LABEL_MONITOR.c_str());
        }
        else
        if (log.has_entries())
        {
            write_char(' ');
            write_fmt(F_ALERT_HOTKEY, 'm', LABEL_MESSAGES.c_str());
        }
        write_fmt(F_CURSOR_POS, 1, 1);
    }
}

void CompactDisplay::display_counts() const
{
    if (enable_term_size)
    {
        write_fmt(F_CURSOR_POS, static_cast<int> (term_y - 1), 1);
    }
    write_fmt(F_RES_COUNT, static_cast<unsigned long long> (resources_map.get_size()));

    if (problem_alert)
    {
        write_fmt(F_PRB_COUNT, static_cast<unsigned long long> (drbdmon.get_problem_count()));
    }
}

void CompactDisplay::problem_check()
{
    problem_alert = drbdmon.get_problem_count() > 0;
}

bool CompactDisplay::list_resources()
{
    bool resources_listed {false};

    // Reset columns tracking
    reset_positions();

    ResourcesMap::ValuesIterator res_iter(resources_map);
    size_t res_count = res_iter.get_size();
    for (size_t res_index = 0; res_index < res_count; ++res_index)
    {
        DrbdResource& res = *(res_iter.next());

        bool marked = res.has_mark_state();
        // If the problem display is active, show only resources that
        // are marked for having warnings or alerts
        if (!dsp_problems_active || marked)
        {
            resources_listed = true;

            // Setup view of the "RES:" label
            const char* f_res = F_RES_NORM;
            if (res.has_alert_state())
            {
                f_res = F_ALERT;
            }
            else
            if (res.has_warn_state())
            {
                f_res = F_WARN;
            }

            // Setup view of the sub-object warning mark
            const char* f_mark = F_RES_NORM;
            const char* mark_icon = mark_off;
            if (res.has_mark_state())
            {
                f_mark = F_MARK;
                mark_icon = mark_on;
            }

            // Setup view of the resource's role
            const char* f_role = F_SECONDARY;
            const char* role_icon = sec_icon;
            if (res.has_role_alert())
            {
                f_role = F_ALERT;
                role_icon = " ";
            }
            else
            if (res.get_role() == DrbdRole::resource_role::PRIMARY)
            {
                f_role = F_PRIMARY;
                role_icon = pri_icon;
            }

            next_line();
            // Marker (1) + "RES:" (4) + RES_NAME_WIDTH + " " (1) + role icon (1) + ROLE_WIDTH
            if (next_column(6 + RES_NAME_WIDTH + ROLE_WIDTH))
            {
                write_fmt(
                    "%s%s%sRES:%s%-*s%s %s%s%-*s%s",
                    f_mark, mark_icon, f_res, F_RES_NAME, RES_NAME_WIDTH, res.get_name().c_str(), F_RESET,
                    f_role, role_icon, ROLE_WIDTH, res.get_role_label(), F_RESET
                );
            }

            increase_indent();
            list_volumes(res);
            list_connections(res);
            decrease_indent();
        }
    }

    return resources_listed;
}

void CompactDisplay::list_connections(DrbdResource& res)
{
    // The connections list always starts on a new line
    next_line();

    // Short-display all connections that have a good state
    {
        ConnectionsMap::ValuesIterator conn_iter = res.connections_iterator();
        size_t conn_count = conn_iter.get_size();
        for (size_t conn_index = 0; conn_index < conn_count; ++conn_index)
        {
            DrbdConnection& conn = *(conn_iter.next());

            if (!conn.has_mark_state())
            {
                const char* f_conn = F_CONN_NORM;

                // Setup role icon & 'Primary' role highlighting format
                const char* f_role = F_CONN_NORM;
                const char* role_icon = sec_icon;
                if (conn.get_role() == DrbdRole::resource_role::PRIMARY)
                {
                    f_role = F_CONN_PRI_FG;
                    role_icon = pri_icon;
                }

                const std::string& conn_name = conn.get_name();

                // Mark (1) + connection icon (1) + role icon (1) + name length
                if (next_column(3 + conn_name.length()))
                {
                    write_fmt(
                        "%s%s%s%s%s%s%s",
                        f_conn, mark_off, conn_good, f_role, role_icon, conn_name.c_str(), F_RESET
                    );
                }
            }
        }
    }

    {
        ConnectionsMap::ValuesIterator conn_iter = res.connections_iterator();
        size_t conn_count = conn_iter.get_size();
        for (size_t conn_index = 0; conn_index < conn_count; ++conn_index)
        {
            DrbdConnection& conn = *(conn_iter.next());

            if (conn.has_mark_state())
            {
                // Setup connection view format
                const char* f_conn = F_CONN_NORM;
                if (conn.has_alert_state())
                {
                    f_conn = F_ALERT;
                }
                else
                if (conn.has_warn_state())
                {
                    f_conn = F_WARN;
                }

                // Setup connection state view format
                const char* f_conn_state = F_NORM;
                const char* conn_icon = conn_good;
                if (conn.has_connection_alert())
                {
                    f_conn_state = F_ALERT;
                    conn_icon = conn_bad;
                }

                // Setup role state view format
                const char* f_role = F_SECONDARY;
                const char* role_icon = sec_icon;
                if (conn.has_role_alert())
                {
                    f_role = F_ALERT;
                    role_icon = " ";
                }
                else
                if (conn.get_role() == DrbdRole::resource_role::PRIMARY)
                {
                    f_role = F_PRIMARY;
                    role_icon = pri_icon;
                }

                next_line();
                // Mark (1) + connection icon (1) + role icon (1) + CONN_NAME_WIDTH +
                // " " (1) + CONN_STATE_WIDTH + " " (1) + role icon (1) + ROLE_WIDTH
                if (next_column(6 + CONN_NAME_WIDTH + CONN_STATE_WIDTH + ROLE_WIDTH))
                {
                    write_fmt(
                        "%s%s%s%s%s%-*s%s %s%-*s%s %s%s%-*s%s",
                        F_MARK, mark_on, f_conn, conn_icon, role_icon, CONN_NAME_WIDTH,
                        conn.get_name().c_str(), F_RESET, f_conn_state, CONN_STATE_WIDTH,
                        conn.get_connection_state_label(), F_RESET, f_role, role_icon,
                        ROLE_WIDTH, conn.get_role_label(), F_RESET
                    );
                }

                increase_indent();
                list_peer_volumes(conn);
                decrease_indent();
            }
        }
    }
}

void CompactDisplay::list_volumes(DrbdResource& res)
{
    // Short-display all volumes that have a good state
    {
        VolumesMap::ValuesIterator vol_iter = res.volumes_iterator();
        size_t vol_count = vol_iter.get_size();
        for (size_t vol_index = 0; vol_index < vol_count; ++vol_index)
        {
            DrbdVolume& vol = *(vol_iter.next());

            if (!vol.has_warn_state())
            {
                show_volume(vol, false, false);
            }
        }
    }

    // Long-display all volumes that have a faulty state
    {
        VolumesMap::ValuesIterator vol_iter = res.volumes_iterator();
        size_t vol_count = vol_iter.get_size();
        for (size_t vol_index = 0; vol_index < vol_count; ++vol_index)
        {
            DrbdVolume& vol = *(vol_iter.next());

            if (vol.has_warn_state())
            {
                show_volume(vol, false, true);
            }
        }
    }
}

void CompactDisplay::list_peer_volumes(DrbdConnection& conn)
{
    VolumesMap::ValuesIterator peer_vol_iter = conn.volumes_iterator();
    size_t peer_vol_count = peer_vol_iter.get_size();
    for (size_t peer_vol_index = 0; peer_vol_index < peer_vol_count; ++peer_vol_index)
    {
        DrbdVolume& peer_vol = *(peer_vol_iter.next());

        if (peer_vol.has_warn_state())
        {
            show_volume(peer_vol, true, true);
        }
    }
}

void CompactDisplay::show_volume(DrbdVolume& vol, bool peer_volume, bool long_format)
{
    if (long_format)
    {
        // Long format view for volumes that have a faulty state

        // Set disk view format
        const char* f_disk = F_NORM;
        if (vol.has_disk_alert())
        {
            f_disk = F_ALERT;
        }

        // Set replication view format
        const char* f_repl = F_NORM;
        if (vol.has_replication_alert())
        {
            f_repl = F_ALERT;
        }
        else
        if (vol.has_replication_warning())
        {
            f_repl = F_WARN;
        }

        bool show_replication = peer_volume;
        if (peer_volume)
        {
            // Hide the 'Established' replication state
            if (vol.get_replication_state() == DrbdVolume::repl_state::ESTABLISHED)
            {
                show_replication = false;
            }
        }

        next_line();

        // Disk icon (1) + VOL_NR_WIDTH + " " (1) + DISK_STATE_WIDTH
        uint16_t vol_entry_width = 2 + VOL_NR_WIDTH + DISK_STATE_WIDTH;

        if (!peer_volume)
        {
            // Local volume, add in minor number fields
            // ":" (1) + MINOR_NR_WIDTH
            vol_entry_width += 1 + MINOR_NR_WIDTH;
        }

        if (show_replication)
        {
            // Add in replication state fields
            // " " (1) + REPL_STATE_WIDTH
            vol_entry_width += 1 + REPL_STATE_WIDTH;
        }

        bool show = next_column(vol_entry_width);

        if (show)
        {
            write_fmt(
                "%s%s%*ld",
                F_ALERT, disk_bad, VOL_NR_WIDTH, static_cast<long> (vol.get_volume_nr())
            );
        }

        // Hide the minor number of peer volumes, as it is always unknown
        if (!peer_volume && show)
        {
            // Display minor number
            write_fmt(":%s%*ld", F_VOL_MINOR, MINOR_NR_WIDTH, static_cast<long> (vol.get_minor_nr()));
        }

        // Display disk state
        if (show)
        {
            write_fmt(" %s %-*s%s", f_disk, DISK_STATE_WIDTH, vol.get_disk_state_label(), F_RESET);
        }

        if (show_replication && show)
        {
            // Display replication state
            write_fmt(
                " %s%-*s%s", f_repl, REPL_STATE_WIDTH,
                vol.get_replication_state_label(), F_RESET
            );
        }
    }
    else
    {
        // Short format view of volumes that have a good state
        const char* f_vol = F_VOL_NORM;
        if (vol.get_disk_state() == DrbdVolume::disk_state::DISKLESS)
        {
            f_vol = F_VOL_CLIENT;
        }

        // Disk icon (1) + VOL_NR_WIDTH + ":" (1) + MINOR_NR_WIDTH
        if (next_column(2 + VOL_NR_WIDTH + MINOR_NR_WIDTH))
        {
            write_fmt(
                "%s%s%s%s%*ld:%s%*ld%s",
                F_NORM, disk_good, F_RESET,
                f_vol, VOL_NR_WIDTH, static_cast<long> (vol.get_volume_nr()),
                F_VOL_MINOR, MINOR_NR_WIDTH, static_cast<long> (vol.get_minor_nr()), F_RESET
            );
        }
    }
}

void CompactDisplay::set_terminal_size(uint16_t size_x, uint16_t size_y)
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

void CompactDisplay::increase_indent()
{
    ++indent_level;
}

void CompactDisplay::decrease_indent()
{
    if (indent_level >= 1)
    {
        --indent_level;
    }
}

void CompactDisplay::reset_indent()
{
    indent_level = 0;
}

void CompactDisplay::reset_positions()
{
    reset_indent();
    current_x = 0;
    current_y = 0;
}

void CompactDisplay::indent()
{
    if (!(enable_term_size && term_y > MIN_SIZE_Y) ||
        (current_y >= page_start && current_y <= page_end))
    {
        for (uint16_t counter = 0; counter < indent_level; ++counter)
        {
            write_text(indent_buffer);
        }
    }
    current_x += INDENT_STEP_SIZE * indent_level;
}

bool CompactDisplay::next_column(uint16_t length)
{
    bool display_flag = false;
    if (enable_term_size && term_y > MIN_SIZE_Y)
    {
        display_flag = current_y >= page_start && current_y <= page_end;

        // If this column is at the start of a new line,
        // print the indent
        if (current_x == 0)
        {
            indent();
            current_x += length;
        }
        else
        {
            // Check whether the column fits in at the end
            // of the current line
            uint16_t end_x = current_x + length + 1;
            if (end_x < term_x)
            {
                if (display_flag)
                {
                    write_char(' ');
                }
                current_x = end_x;
            }
            else
            {
                next_line();
                display_flag = current_y >= page_start && current_y <= page_end;
                indent();
                current_x += length;
            }
        }
    }
    else
    {
        display_flag = true;
        if (current_x == 0)
        {
            indent();
            current_x += length;
        }
        else
        {
            // No terminal size restrictions,
            // allow an infinite number of columns
            write_char(' ');
        }
    }
    return display_flag;
}

void CompactDisplay::next_line()
{
    if (current_x > 0)
    {
        if (!(enable_term_size && term_y > MIN_SIZE_Y) ||
            (current_y >= page_start && current_y <= page_end))
        {
            write_char('\n');
        }
        current_x = 0;
        ++current_y;
    }
}

void CompactDisplay::set_utf8(bool enable)
{
    enable_utf8 = enable;
    if (enable_utf8)
    {
        pri_icon  = UTF8_PRIMARY;
        sec_icon  = UTF8_SECONDARY;
        conn_good = UTF8_CONN_GOOD;
        conn_bad  = UTF8_CONN_BAD;
        disk_good = UTF8_DISK_GOOD;
        disk_bad  = UTF8_DISK_BAD;
        mark_off  = UTF8_MARK_OFF;
        mark_on   = UTF8_MARK_ON;
    }
    else
    {
        pri_icon  = ASCII_PRIMARY;
        sec_icon  = ASCII_SECONDARY;
        conn_good = ASCII_CONN_GOOD;
        conn_bad  = ASCII_CONN_BAD;
        disk_good = ASCII_DISK_GOOD;
        disk_bad  = ASCII_DISK_BAD;
        mark_off  = ASCII_MARK_OFF;
        mark_on   = ASCII_MARK_ON;
    }
}

// @throws std::bad_alloc
void CompactDisplay::announce_options(Configurator& collector)
{
    Configurable& owner = dynamic_cast<Configurable&> (*this);
    collector.add_config_option(owner, OPT_NO_HEADER);
    collector.add_config_option(owner, OPT_NO_HOTKEYS);
    collector.add_config_option(owner, OPT_ASCII);
}

void CompactDisplay::options_help() noexcept
{
    std::fputs("DrbdMon display configuration options:\n", stderr);
    std::fputs("  --ascii          Use only ASCII characters (no Unicode)\n", stderr);
    std::fputs("  --no-header      Do not display the DrbdMon header line\n", stderr);
    std::fputs("  --no-hotkeys     Do not display the hotkeys line\n", stderr);
    std::fputc('\n', stderr);
    std::fflush(stderr);
}

// @throws std::bad_alloc
void CompactDisplay::set_flag(std::string& key)
{
    if (key == OPT_NO_HEADER.key)
    {
        show_header = false;
    }
    else
    if (key == OPT_NO_HOTKEYS.key)
    {
        show_hotkeys = false;
    }
    else
    if (key == OPT_ASCII.key)
    {
        set_utf8(false);
    }
}

// @throws std::bad_alloc
void CompactDisplay::set_option(std::string& key, std::string& value)
{
    // no-op; the CompactDisplay instance does not have any options
    // with configurable values at this time
}

void CompactDisplay::key_pressed(const char key)
{
    switch (key)
    {
        case '>':
            ++page;
            status_display();
            break;
        case '<':
            --page;
            status_display();
            break;
        case 'm':
            if (dsp_msg_active)
            {
                dsp_msg_active = false;
            }
            else
            if (log.has_entries())
            {
                dsp_msg_active = true;
            }
            status_display();
            break;
        case 'p':
            if (dsp_problems_active)
            {
                dsp_problems_active = false;
            }
            else
            {
                dsp_problems_active = true;
            }
            // fall-through
        case '1':
            page = 0;
            status_display();
            break;
        default:
            // no-op
            break;
    }
}

/**
 * Writes a single character to the output_fd file descriptor
 *
 * @param ch The character to write
 */
void CompactDisplay::write_char(const char ch) const noexcept
{
    // Repeat temporarily failing write() calls until the byte has been written
    uint32_t loop_guard {0};
    while (write(output_fd, static_cast<const void*> (&ch), 1) != 1)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
        {
            break;
        }

        if (loop_guard < MAX_YIELD_LOOP)
        {
            // Attempt to yield to other processes before retrying
            static_cast<void> (sched_yield());
            ++loop_guard;
        }
        else
        {
            // If yielding to other processes did not lead to any progress,
            // suspend for a while
            static_cast<void> (nanosleep(&write_retry_delay, nullptr));
        }
    }
}

/**
 * Writes a text string to the output_fd file descriptor
 *
 * @param text The text string to write
 */
void CompactDisplay::write_text(const char* text) const noexcept
{
    size_t length = std::strlen(text);
    write_buffer(text, length);
}

/**
 * Formats a text string and writes the result to the output_fd file descriptor
 *
 * @param format Format string
 * @param ... Arguments for the format string
 */
void CompactDisplay::write_fmt(const char* format, ...) const noexcept
{
    va_list vars;
    va_start(vars, format);
    size_t safe_length = 0;
    {
        size_t unsafe_length = vsnprintf(output_buffer, OUTPUT_BUFFER_SIZE, format, vars);
        safe_length = unsafe_length < OUTPUT_BUFFER_SIZE ? unsafe_length : OUTPUT_BUFFER_SIZE;
    }
    va_end(vars);
    write_buffer(output_buffer, safe_length);
}

/**
 * Writes buffered data to the output_fd file descriptor
 *
 * Write attempts that fail temporarily or are only partially successful are retried until the
 * all the buffered data has been written.
 *
 * @param buffer The buffered data to write
 * @param length Length of the buffered data in the (possibly larger) buffer
 */
void CompactDisplay::write_buffer(const char* buffer, size_t length) const noexcept
{
    uint32_t loop_guard {0};
    ssize_t written {0};
    while (length > 0)
    {
        // Repeat temporarily failing write() calls until the entire contents of the buffer have been written
        written = write(output_fd, static_cast<const void*> (buffer), length);
        if (written > 0)
        {
            length -= written;
        }
        else
        if (written == -1 && (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
        {
            break;
        }

        if (written <= 0)
        {
            if (loop_guard < MAX_YIELD_LOOP)
            {
                // Attempt to yield to other processes before retrying
                static_cast<void> (sched_yield());
                ++loop_guard;
            }
            else
            {
                // If yielding to other processes did not lead to any progress,
                // suspend for a while
                static_cast<void> (nanosleep(&write_retry_delay, nullptr));
            }
        }
    }
}
