#include <CompactDisplay.h>
#include <DrbdMon.h>
#include <ConfigOption.h>
#include <iostream>

#include <cstring>
#include <cctype>
#include <cstdarg>
#include <cstdio>

extern "C"
{
    #include <pthread.h>
    #include <errno.h>
}

const std::string CompactDisplay::OPT_NO_HEADER_KEY("no-header");
const std::string CompactDisplay::OPT_NO_HOTKEYS_KEY("no-hotkeys");
const std::string CompactDisplay::OPT_ASCII_KEY("ascii");
const std::string CompactDisplay::OPT_PROBLEMS_KEY("problems");

const std::string CompactDisplay::HDR_SPACER(" | ");
const std::string CompactDisplay::NODE_DSP_PREFIX("Node ");
const std::string CompactDisplay::TRUNC_MARKER("...");

const ConfigOption CompactDisplay::OPT_NO_HEADER(true, OPT_NO_HEADER_KEY);
const ConfigOption CompactDisplay::OPT_NO_HOTKEYS(true, OPT_NO_HOTKEYS_KEY);
const ConfigOption CompactDisplay::OPT_ASCII(true, OPT_ASCII_KEY);
const ConfigOption CompactDisplay::OPT_PROBLEMS(true, OPT_PROBLEMS_KEY);

// The F_QUORUM_FORMAT specification of all display formats (DisplayFormats class) must match the
// reserved width for the quorum alert field
const int CompactDisplay::QUORUM_ALERT_WIDTH = 11;

// The F_PAGE and F_GOTO_PAGE specifications of all the display formats (DisplayFormats class) must match the
// settings for page navigation positioning and field width
const int   CompactDisplay::PAGE_POS_R   = 15;
const int   CompactDisplay::GOTO_PAGE_POS_R = 37;
const int   CompactDisplay::GOTO_PAGE_CURSOR_POS = 17;

const char* CompactDisplay::UTF8_PRIMARY        = "\xE2\x9A\xAB";
const char* CompactDisplay::UTF8_SECONDARY      = " "; // otherwise UTF-8 E2 9A AA
const char* CompactDisplay::UTF8_CONN_GOOD      = "\xE2\x87\x84";
const char* CompactDisplay::UTF8_CONN_BAD       = "\xE2\x86\xAF";
const char* CompactDisplay::UTF8_DISK_GOOD      = "\xE2\x97\x8E";
const char* CompactDisplay::UTF8_DISK_BAD       = "\xE2\x9C\x97";
const char* CompactDisplay::UTF8_MARK_OFF       = "\xE2\x9A\xAC";
const char* CompactDisplay::UTF8_MARK_ON        = "\xE2\xA4\xB7";

const char* CompactDisplay::ASCII_PRIMARY       = "*";
const char* CompactDisplay::ASCII_SECONDARY     = " ";
const char* CompactDisplay::ASCII_CONN_GOOD     = "+";
const char* CompactDisplay::ASCII_CONN_BAD      = "/";
const char* CompactDisplay::ASCII_DISK_GOOD     = "+";
const char* CompactDisplay::ASCII_DISK_BAD      = "/";
const char* CompactDisplay::ASCII_MARK_OFF      = " ";
const char* CompactDisplay::ASCII_MARK_ON       = "+";

const char* CompactDisplay::ANSI_CLEAR      = "\x1B[H\x1B[2J";
const char* CompactDisplay::ANSI_CLEAR_LINE = "\x1B[K";

const char* CompactDisplay::ANSI_CURSOR_OFF = "\x1B[?25l";
const char* CompactDisplay::ANSI_CURSOR_ON  = "\x1B[?25h";

const char* CompactDisplay::ANSI_ALTBFR_ON  = "\x1B[?1049h";
const char* CompactDisplay::ANSI_ALTBFR_OFF = "\x1B[?1049l";

const char CompactDisplay::HOTKEY_MESSAGES  = 'm';
const char CompactDisplay::HOTKEY_CLEAR_MSG = 'c';
const char CompactDisplay::HOTKEY_PGUP      = '<';
const char CompactDisplay::HOTKEY_PGDN      = '>';
const char CompactDisplay::HOTKEY_PGONE     = '!';

const char CompactDisplay::KEY_BACKSPACE = 127;
const char CompactDisplay::KEY_CTRL_H    = 8;
const char CompactDisplay::KEY_NEWLINE   = '\n';
const char CompactDisplay::KEY_ENTER     = '\r';
const char CompactDisplay::KEY_ESC       = 0x1B;

const std::string CompactDisplay::LABEL_MESSAGES    = "Messages";
const std::string CompactDisplay::LABEL_CLEAR_MSG   = "Clear messages";
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

const int CompactDisplay::RES_NAME_WIDTH   = 48;
const int CompactDisplay::ROLE_WIDTH       = 10;
const int CompactDisplay::VOL_NR_WIDTH     = 5;
const int CompactDisplay::MINOR_NR_WIDTH   = 7;
const int CompactDisplay::CONN_NAME_WIDTH  = 48;
const int CompactDisplay::CONN_STATE_WIDTH = 20;
const int CompactDisplay::DISK_STATE_WIDTH = 20;
const int CompactDisplay::REPL_STATE_WIDTH = 20;

const uint16_t CompactDisplay::INDENT_STEP_SIZE     = 4;
const uint16_t CompactDisplay::OUTPUT_BUFFER_SIZE   = 1024;

const uint16_t CompactDisplay::MIN_NODENAME_DSP_LENGTH = 4;
const uint32_t CompactDisplay::MAX_YIELD_LOOP = 10;

const uint16_t CompactDisplay::ProgressBar::MIN_SYNC_BAR_SIZE   = 20;
const uint16_t CompactDisplay::ProgressBar::MAX_SYNC_BAR_SIZE   = 1000;

const uint16_t CompactDisplay::ProgressBar::MAX_PERC            = 100;
const uint16_t CompactDisplay::ProgressBar::MAX_SYNC_PERC       = 10000;

const char* CompactDisplay::ProgressBar::ASCII_SYNC_BLK     = "X";
const char* CompactDisplay::ProgressBar::ASCII_UNSYNC_BLK   = "-";

const char* CompactDisplay::ProgressBar::UTF8_SYNC_BLK      = "\xE2\x96\x88";
const char* CompactDisplay::ProgressBar::UTF8_UNSYNC_BLK    = "\xE2\x96\x92";

const CompactDisplay::DisplayFormats CompactDisplay::DSP_FMT_16COLOR(
    // F_NORM
    "\x1B[0;32m",
    // F_WARN
    "\x1B[1;33;45m",
    // F_ALERT
    "\x1B[1;33;41m",
    // F_RESET
    "\x1B[0m",
    // F_MARK
    "\x1B[1;33;41m",
    // F_HEADER
    "\x1B[1;37;44m",
    // F_RES_NORM
    "\x1B[0;30;42m",
    // F_VOL_NORM
    "\x1B[0;30;46m",
    // F_VOL_CLIENT
    "\x1B[0;1;37;44m",
    // F_VOL_MINOR
    "\x1B[0;4;36m",
    // F_CONN_NORM
    "\x1B[0;37;44m",
    // F_PRIMARY
    "\x1B[1;36m",
    // F_SECONDARY
    "\x1B[0;36m",
    // F_SYNC_PERC
    "\x1B[1;31m",
    // QUORUM_ALERT
    "\x1B[1;33;41mQUORUM LOST\x1B[0m",
    // F_CONN_PRI_FG
    "\x1B[1;36;44m",
    // F_RES_NAME
    "\x1B[0;4;32m",
    // F_RES_COUNT
    "\x1B[0;32mResources: %6llu\x1B[0m",
    // F_PRB_COUNT
    "\x1B[0;1;31m (%llu degraded)\x1B[0m",
    // F_CURSOR_POS
    "\x1B[%u;%uH",
    // F_HOTKEY
    "\x1B[0;30;47m%c\x1B[0;37;44m %s \x1B[0m",
    // F_PRB_HOTKEY
    "\x1B[0;30;47m%c\x1B[0;30;42m %s \x1B[0m",
    // F_ALERT_HOTKEY
    "\x1B[0;30;47m%c\x1B[1;33;41m %s \x1B[0m",
    // F_PAGE
    "Page: %5llu\n",
    // F_GOTO_PAGE
    "\x1B[1;37m[Go to page: %5llu]\x1B[0m",
    // F_SYNC_BLK
    "\x1B[0;32m",
    // F_UNSYNC_BLK
    "\x1B[1;31m"
);

const CompactDisplay::DisplayFormats CompactDisplay::DSP_FMT_256COLOR(
    // F_NORM
    "\x1B[0;38;5;34m",
    // F_WARN
    "\x1B[0;38;5;52;48;5;220m",
    // F_ALERT
    "\x1B[1;38;5;226;48;5;196m",
    // F_RESET
    "\x1B[0m",
    // F_MARK
    "\x1B[0;38;5;226;48;5;196m",
    // F_HEADER
    "\x1B[0;38;5;39;48;5;19m",
    // F_RES_NORM
    "\x1B[0;38;5;16;48;5;34m",
    // F_VOL_NORM
    "\x1B[0;38;5;16;48;5;36m",
    // F_VOL_CLIENT
    "\x1B[0;38;5;16;48;5;33m",
    // F_VOL_MINOR
    "\x1B[0;4;38;5;250m",
    // F_CONN_NORM
    "\x1B[0;38;5;39;48;5;19m",
    // F_PRIMARY
    "\x1B[1;38;5;231m",
    // F_SECONDARY
    "\x1B[0;38;5;250m",
    // F_SYNC_PERC
    "\x1B[1;38;5;196m",
    // QUORUM_ALERT
    "\x1B[1;38;5;226;48;5;196mQUORUM LOST\x1B[0m",
    // F_CONN_PRI_FG
    "\x1B[1;38;5;231m",
    // F_RES_NAME
    "\x1B[0;4;38;5;34m",
    // F_RES_COUNT
    "\x1B[0;38;5;34mResources: %6llu\x1B[0m",
    // F_PRB_COUNT
    "\x1B[0;1;38;5;196m (%llu degraded)\x1B[0m",
    // F_CURSOR_POS
    "\x1B[%u;%uH",
    // F_HOTKEY
    "\x1B[0;38;5;16;48;5;250m%c\x1B[0;38;5;39;48;5;19m %s \033[0m",
    // F_PRB_HOTKEY
    "\x1B[0;38;5;16;48;5;250m%c\x1B[0;38;5;42;48;5;22m %s \033[0m",
    // F_ALERT_HOTKEY
    "\x1B[0;38;5;16;48;5;250m%c\x1B[1;38;5;226;48;5;196m %s \033[0m",
    // F_PAGE
    "Page: %5llu\n",
    // F_GOTO_PAGE
    "\x1B[0;38;5;231m[Go to page: %5llu]\x1B[0m",
    // F_SYNC_BLK
    "\x1B[0;38;5;34m",
    // F_UNSYNC_BLK
    "\x1B[1;38;5;226m"
);

// @throws std::bad_alloc
CompactDisplay::CompactDisplay(
    DrbdMon& drbdmon_ref,
    ResourcesMap& resources_map_ref,
    MessageLog&   log_ref,
    HotkeysMap&   hotkeys_info_ref,
    const std::string* const node_name_ref,
    const color_mode colors
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

    hotkeys_info.append(&HOTKEY_PGONE, &LABEL_PGZERO);
    hotkeys_info.append(&HOTKEY_PGUP, &LABEL_PGUP);
    hotkeys_info.append(&HOTKEY_PGDN, &LABEL_PGDN);

    node_label = std::unique_ptr<std::string>(new std::string(HDR_SPACER));
    *node_label += NODE_DSP_PREFIX;

    dsp_fmt = colors == color_mode::EXTENDED ? &DSP_FMT_256COLOR : &DSP_FMT_16COLOR;
}

CompactDisplay::~CompactDisplay() noexcept
{
    // Show the cursor
    write_text(ANSI_CURSOR_ON);

    if (alt_bfr_active)
    {
        // Switch back to the normal screen buffer
        write_fmt(ANSI_ALTBFR_OFF);
    }
}

void CompactDisplay::clear()
{
    if (mode == input_mode::PAGE_NR)
    {
        // Page navigation may have turned the cursor on
        write_text(ANSI_CURSOR_OFF);
    }
    write_text(ANSI_CLEAR);

    current_x = 0;
    current_y = 0;
}

void CompactDisplay::initial_display()
{
    // Switch to the alternate screen buffer
    write_fmt(ANSI_ALTBFR_ON);
    alt_bfr_active = true;

    clear();
    display_header();

    if (log.has_entries() && dsp_msg_active)
    {
        log.display_messages(std::cout);
    }
    else
    {
        if (!log.has_entries())
        {
            dsp_msg_active = false;
        }

        write_fmt("%sReading initial DRBD status...%s\n", dsp_fmt->F_RES_NAME, dsp_fmt->F_RESET);
    }

    // End line
    next_line();

    display_hotkeys_info();
    if (mode == input_mode::PAGE_NR)
    {
        page_nav_cursor();
    }
}

void CompactDisplay::status_display()
{
    uint32_t current_page = page;
    page_start = (term_y - 4) * current_page;
    page_end   = (term_y - 4) * (current_page + 1) - 1;

    if (sync_progress == nullptr)
    {
        sync_progress_mgr = std::unique_ptr<CompactDisplay::ProgressBar>(
            new CompactDisplay::ProgressBar(this, enable_utf8)
        );
        sync_progress = sync_progress_mgr.get();
    }

    problem_check();
    clear();
    display_header();

    if (log.has_entries() && dsp_msg_active)
    {
        log.display_messages(std::cout);
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
                write_fmt("%sNo resources with problems to display.%s\n", dsp_fmt->F_RES_NAME, dsp_fmt->F_RESET);
            }
            else
            {
                write_fmt("%sNo active DRBD resources.%s\n", dsp_fmt->F_RES_NAME, dsp_fmt->F_RESET);
            }
        }
    }

    // End line
    next_line();

    display_counts();

    display_hotkeys_info();
    if (mode == input_mode::PAGE_NR)
    {
        page_nav_cursor();
    }
}

void CompactDisplay::display_header() const
{
    if (show_header)
    {
        write_fmt("%s%s%s v%s", dsp_fmt->F_HEADER, ANSI_CLEAR_LINE,
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
        if (enable_term_size)
        {
            if (mode == input_mode::PAGE_NR)
            {
                page_nav_display();
            }
            write_fmt(dsp_fmt->F_CURSOR_POS, 2, static_cast<int> (term_x) - PAGE_POS_R);
            write_text(dsp_fmt->F_RESET);
            write_fmt(dsp_fmt->F_PAGE, static_cast<unsigned long long> (page) + 1);
        }
        else
        {
            write_char('\n');
        }
    }
}

void CompactDisplay::page_nav_display() const
{
    if (enable_term_size)
    {
        write_fmt(dsp_fmt->F_CURSOR_POS, 2, static_cast<int> (term_x) - GOTO_PAGE_POS_R);
        write_text(dsp_fmt->F_RESET);
        write_fmt(dsp_fmt->F_GOTO_PAGE, static_cast<unsigned long long> (goto_page) > 0 ? goto_page : 1);
    }
}

void CompactDisplay::page_nav_cursor() const
{
    if (enable_term_size)
    {
        write_fmt(dsp_fmt->F_CURSOR_POS, 2, static_cast<int> (term_x) - GOTO_PAGE_POS_R + GOTO_PAGE_CURSOR_POS);
        write_fmt(ANSI_CURSOR_ON);
    }
}

void CompactDisplay::display_hotkeys_info() const
{
    if (show_hotkeys)
    {
        if (enable_term_size)
        {
            write_fmt(dsp_fmt->F_CURSOR_POS, static_cast<int> (term_y), 1);
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
            write_fmt(dsp_fmt->F_HOTKEY, hotkey, description.c_str());
            ++index;
        }

        if (index > 0)
        {
            write_char(' ');
        }
        if (dsp_problems_active)
        {
            write_fmt(dsp_fmt->F_HOTKEY, 'p', LABEL_STATUS.c_str());
        }
        else
        {
            const char* format = problem_alert ? dsp_fmt->F_ALERT_HOTKEY : dsp_fmt->F_PRB_HOTKEY;
            write_fmt(format, 'p', LABEL_PROBLEMS.c_str());
        }

        if (dsp_msg_active)
        {
            write_char(' ');
            write_fmt(dsp_fmt->F_HOTKEY, 'm', LABEL_MONITOR.c_str());
            write_char(' ');
            write_fmt(dsp_fmt->F_HOTKEY, 'c', LABEL_CLEAR_MSG.c_str());
        }
        else
        if (log.has_entries())
        {
            write_char(' ');
            write_fmt(dsp_fmt->F_ALERT_HOTKEY, 'm', LABEL_MESSAGES.c_str());
        }
        write_fmt(dsp_fmt->F_CURSOR_POS, 1, 1);
    }
}

void CompactDisplay::display_counts() const
{
    if (enable_term_size)
    {
        write_fmt(dsp_fmt->F_CURSOR_POS, static_cast<int> (term_y - 1), 1);
    }
    write_fmt(dsp_fmt->F_RES_COUNT, static_cast<unsigned long long> (resources_map.get_size()));

    if (problem_alert)
    {
        write_fmt(dsp_fmt->F_PRB_COUNT, static_cast<unsigned long long> (drbdmon.get_problem_count()));
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
            const char* f_res = dsp_fmt->F_RES_NORM;
            if (res.has_alert_state())
            {
                f_res = dsp_fmt->F_ALERT;
            }
            else
            if (res.has_warn_state())
            {
                f_res = dsp_fmt->F_WARN;
            }

            // Setup view of the sub-object warning mark
            const char* f_mark = dsp_fmt->F_RES_NORM;
            const char* mark_icon = mark_off;
            if (res.has_mark_state())
            {
                f_mark = dsp_fmt->F_MARK;
                mark_icon = mark_on;
            }

            // Setup view of the resource's role
            const char* f_role = dsp_fmt->F_SECONDARY;
            const char* role_icon = sec_icon;
            if (res.has_role_alert())
            {
                f_role = dsp_fmt->F_ALERT;
                role_icon = " ";
            }
            else
            if (res.get_role() == DrbdRole::resource_role::PRIMARY)
            {
                f_role = dsp_fmt->F_PRIMARY;
                role_icon = pri_icon;
            }

            next_line();
            // Marker (1) + "RES:" (4) + " " (1) + RES_NAME_WIDTH + " " (1) + role icon (1) + ROLE_WIDTH
            if (next_column(7 + RES_NAME_WIDTH + ROLE_WIDTH))
            {
                write_fmt(
                    "%s%s%sRES:%s ",
                    f_mark, mark_icon, f_res, dsp_fmt->F_RES_NAME
                );
                write_string_field(res.get_name(), RES_NAME_WIDTH, true);
                write_fmt(
                    "%s %s%s%-*s%s",
                    dsp_fmt->F_RESET, f_role, role_icon, ROLE_WIDTH, res.get_role_label(), dsp_fmt->F_RESET
                );
            }

            // Show quorum alert warning
            if (res.has_quorum_alert())
            {
                if (next_column(1 + QUORUM_ALERT_WIDTH))
                {
                    write_fmt(" %s", dsp_fmt->QUORUM_ALERT);
                }
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
                const char* f_conn = dsp_fmt->F_CONN_NORM;

                // Setup role icon & 'Primary' role highlighting format
                const char* f_role = dsp_fmt->F_CONN_NORM;
                const char* role_icon = sec_icon;
                if (conn.get_role() == DrbdRole::resource_role::PRIMARY)
                {
                    f_role = dsp_fmt->F_CONN_PRI_FG;
                    role_icon = pri_icon;
                }

                const std::string& conn_name = conn.get_name();

                // Mark (1) + connection icon (1) + role icon (1) + min(name length, CONN_NAME_WIDTH)
                if (
                    next_column(
                        3 +
                        std::min(static_cast<uint16_t> (conn_name.length()), static_cast<uint16_t> (CONN_NAME_WIDTH))
                    )
                )
                {
                    write_fmt(
                        "%s%s%s%s%s",
                        f_conn, mark_off, conn_good, f_role, role_icon
                    );
                    write_string_field(conn_name, CONN_NAME_WIDTH, false);
                    write_text(dsp_fmt->F_RESET);
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
                const char* f_conn = dsp_fmt->F_CONN_NORM;
                if (conn.has_alert_state())
                {
                    f_conn = dsp_fmt->F_ALERT;
                }
                else
                if (conn.has_warn_state())
                {
                    f_conn = dsp_fmt->F_WARN;
                }

                // Setup connection state view format
                const char* f_conn_state = dsp_fmt->F_NORM;
                const char* conn_icon = conn_good;
                if (conn.has_connection_alert())
                {
                    f_conn_state = dsp_fmt->F_ALERT;
                    conn_icon = conn_bad;
                }

                // Setup role state view format
                const char* f_role = dsp_fmt->F_SECONDARY;
                const char* role_icon = sec_icon;
                if (conn.has_role_alert())
                {
                    f_role = dsp_fmt->F_ALERT;
                    role_icon = " ";
                }
                else
                if (conn.get_role() == DrbdRole::resource_role::PRIMARY)
                {
                    f_role = dsp_fmt->F_PRIMARY;
                    role_icon = pri_icon;
                }

                next_line();
                // Mark (1) + connection icon (1) + role icon (1) + CONN_NAME_WIDTH +
                // " " (1) + CONN_STATE_WIDTH + " " (1) + role icon (1) + ROLE_WIDTH
                if (next_column(6 + CONN_NAME_WIDTH + CONN_STATE_WIDTH + ROLE_WIDTH))
                {
                    write_fmt(
                        "%s%s%s%s%s",
                        dsp_fmt->F_MARK, mark_on, f_conn, conn_icon, role_icon
                    );
                    write_string_field(conn.get_name(), CONN_NAME_WIDTH, true);
                    write_fmt(
                        "%s %s%-*s%s %s%s%-*s%s",
                        dsp_fmt->F_RESET, f_conn_state, CONN_STATE_WIDTH,
                        conn.get_connection_state_label(), dsp_fmt->F_RESET, f_role, role_icon,
                        ROLE_WIDTH, conn.get_role_label(), dsp_fmt->F_RESET
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
        const char* f_disk = dsp_fmt->F_NORM;
        if (vol.has_disk_alert())
        {
            f_disk = dsp_fmt->F_ALERT;
        }

        // Set replication view format
        const char* f_repl = dsp_fmt->F_NORM;
        if (vol.has_replication_alert())
        {
            f_repl = dsp_fmt->F_ALERT;
        }
        else
        if (vol.has_replication_warning())
        {
            f_repl = dsp_fmt->F_WARN;
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

        bool quorum_alert = vol.has_quorum_alert();

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

        if (quorum_alert && debug_view)
        {
            vol_entry_width += 1 + QUORUM_ALERT_WIDTH;
        }

        bool show = next_column(vol_entry_width);

        if (show)
        {
            write_fmt(
                "%s%s%*ld",
                dsp_fmt->F_ALERT, disk_bad, VOL_NR_WIDTH, static_cast<long> (vol.get_volume_nr())
            );
        }

        // Hide the minor number of peer volumes, as it is always unknown
        if (!peer_volume && show)
        {
            // Display minor number
            write_fmt(":%s%*ld", dsp_fmt->F_VOL_MINOR, MINOR_NR_WIDTH, static_cast<long> (vol.get_minor_nr()));
        }

        // Display disk state
        if (show)
        {
            write_fmt(" %s %-*s%s", f_disk, DISK_STATE_WIDTH, vol.get_disk_state_label(), dsp_fmt->F_RESET);
        }

        if (show)
        {
            if (show_replication)
            {
                // Display replication state
                write_fmt(
                    " %s%-*s%s", f_repl, REPL_STATE_WIDTH,
                    vol.get_replication_state_label(), dsp_fmt->F_RESET
                );
            }

            if (quorum_alert && debug_view)
            {
                write_fmt(" %s", dsp_fmt->QUORUM_ALERT);
            }
        }

        uint16_t sync_perc = vol.get_sync_perc();
        if (sync_perc != DrbdVolume::MAX_SYNC_PERC)
        {
            next_line();
            increase_indent();
            // "xxx.xx% " -> 8 bytes column length
            if (next_column(8))
            {
                write_text(dsp_fmt->F_SYNC_PERC);
                write_fmt(
                    "%3u.%02u%% ",
                    static_cast<unsigned int> (sync_perc / 100),
                    static_cast<unsigned int> (sync_perc % 100)
                );
                write_text(dsp_fmt->F_RESET);

                // Display a sync progress bar if there is enough space left
                // (leave 4 characters space as margin to the right edge of the terminal)
                uint16_t sync_bar_width = std::max(term_x - current_x, 2) - 4;
                if (sync_bar_width >= CompactDisplay::ProgressBar::MIN_SYNC_BAR_SIZE)
                {
                    sync_progress->display_progress_bar(sync_bar_width, sync_perc);
                }
            }
            decrease_indent();
        }
    }
    else
    {
        // Short format view of volumes that have a good state
        const char* f_vol = dsp_fmt->F_VOL_NORM;
        if (vol.get_disk_state() == DrbdVolume::disk_state::DISKLESS)
        {
            f_vol = dsp_fmt->F_VOL_CLIENT;
        }

        // Disk icon (1) + VOL_NR_WIDTH + ":" (1) + MINOR_NR_WIDTH
        if (next_column(2 + VOL_NR_WIDTH + MINOR_NR_WIDTH))
        {
            write_fmt(
                "%s%s%s%s%*ld:%s%*ld%s",
                dsp_fmt->F_NORM, disk_good, dsp_fmt->F_RESET,
                f_vol, VOL_NR_WIDTH, static_cast<long> (vol.get_volume_nr()),
                dsp_fmt->F_VOL_MINOR, MINOR_NR_WIDTH, static_cast<long> (vol.get_minor_nr()), dsp_fmt->F_RESET
            );
        }
    }
}

void CompactDisplay::set_terminal_size(uint16_t size_x, uint16_t size_y)
{
    term_x = dsaext::generic_bounds<uint16_t>(MIN_SIZE_X, size_x, MAX_SIZE_X);
    term_y = dsaext::generic_bounds<uint16_t>(MIN_SIZE_Y, size_y, MAX_SIZE_Y);
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
    collector.add_config_option(owner, OPT_PROBLEMS);
}

void CompactDisplay::options_help() noexcept
{
    std::cerr.clear();
    std::cerr << DrbdMon::PROGRAM_NAME << " display configuration options:\n";
    std::cerr << "  --ascii          Use only ASCII characters (no Unicode)\n";
    std::cerr << "  --no-header      Do not display the header line\n";
    std::cerr << "  --no-hotkeys     Do not display the hotkeys line\n";
    std::cerr << "  --problems       Start with the problems view\n";
    std::cerr << std::endl;
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
    else
    if (key == OPT_PROBLEMS.key)
    {
        dsp_problems_active = true;
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
        case HOTKEY_PGDN:
            ++page;
            status_display();
            break;
        case HOTKEY_PGUP:
            --page;
            status_display();
            break;
        case '%':
            // Toggle debug view
            debug_view = !debug_view;
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
        case HOTKEY_CLEAR_MSG:
            if (dsp_msg_active)
            {
                log.clear();
                dsp_msg_active = false;
                status_display();
            }
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
        case HOTKEY_PGONE:
            page = 0;
            status_display();
            break;
        default:
            if (isdigit(key) != 0)
            {
                uint32_t digit = static_cast<unsigned char> (key) - static_cast<unsigned char> ('0');
                if (mode == input_mode::HOTKEYS && enable_term_size)
                {
                    goto_page = digit;
                    mode = input_mode::PAGE_NR;
                    page_nav_display();
                    page_nav_cursor();
                }
                else
                if (mode == input_mode::PAGE_NR)
                {
                    uint32_t new_goto_page = (goto_page * 10) + digit;
                    if (new_goto_page <= static_cast<uint32_t> (1) << 16)
                    {
                        goto_page = new_goto_page;
                        page_nav_display();
                        page_nav_cursor();
                    }
                }
            }
            else
            if (mode == input_mode::PAGE_NR)
            {
                if (key == KEY_BACKSPACE || key == KEY_CTRL_H)
                {
                    goto_page = goto_page / 10;
                    page_nav_display();
                    page_nav_cursor();
                }
                else
                if (key == KEY_NEWLINE || key == KEY_ENTER)
                {
                    if (goto_page > 0)
                    {
                        --goto_page;
                    }
                    if (goto_page < static_cast<uint32_t> (1) << 16)
                    {
                        page = goto_page;
                        goto_page = 0;
                    }
                    mode = input_mode::HOTKEYS;
                    write_text(ANSI_CURSOR_OFF);
                    status_display();
                }
                else
                if (key == KEY_ESC)
                {
                    goto_page = 0;
                    mode = input_mode::HOTKEYS;
                    write_text(ANSI_CURSOR_OFF);
                    status_display();
                }
            }
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

void CompactDisplay::write_string_field(
    const std::string& text,
    const size_t field_width,
    const bool fill
) const noexcept
{
    const size_t text_length = text.length();
    if (text_length <= field_width)
    {
        write_buffer(text.c_str(), text_length);
        if (text_length < field_width)
        {
            const size_t fill_length = std::min(
                static_cast<size_t> (field_width - text_length),
                static_cast<size_t> (OUTPUT_BUFFER_SIZE)
            );
            if (fill)
            {
                for (size_t idx = 0; idx < fill_length; ++idx)
                {
                    output_buffer[idx] = ' ';
                }
                write_buffer(output_buffer, fill_length);
            }
        }
    }
    else
    {
        // Print truncation indicator only in fields with a length of at least 6 bytes
        if (field_width >= 6)
        {
            // Print truncated text and truncation indicator
            write_buffer(text.c_str(), field_width - 3);
            write_buffer("...", 3);
        }
        else
        {
            // Print truncated text without truncation indicator
            write_buffer(text.c_str(), field_width);
        }
    }
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
void CompactDisplay::write_buffer(const char* buffer, const size_t write_length) const noexcept
{
    size_t length = write_length;
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

CompactDisplay::DisplayFormats::DisplayFormats(
    const char* const f_norm_ref,
    const char* const f_warn_ref,
    const char* const f_alert_ref,
    const char* const f_reset_ref,
    const char* const f_mark_ref,
    const char* const f_header_ref,
    const char* const f_res_norm_ref,
    const char* const f_vol_norm_ref,
    const char* const f_vol_client_ref,
    const char* const f_vol_minor_ref,
    const char* const f_conn_norm_ref,
    const char* const f_primary_ref,
    const char* const f_secondary_ref,
    const char* const f_sync_perc_ref,
    const char* const quorum_alert_ref,
    const char* const f_conn_pri_fg_ref,
    const char* const f_res_name_ref,
    const char* const f_res_count_ref,
    const char* const f_prb_count_ref,
    const char* const f_cursor_pos_ref,
    const char* const f_hotkey_ref,
    const char* const f_prb_hotkey_ref,
    const char* const f_alert_hotkey_ref,
    const char* const f_page_ref,
    const char* const f_goto_page_ref,
    const char* const f_sync_blk_ref,
    const char* const f_unsync_blk_ref
):
    F_NORM(f_norm_ref),
    F_WARN(f_warn_ref),
    F_ALERT(f_alert_ref),
    F_RESET(f_reset_ref),
    F_MARK(f_mark_ref),
    F_HEADER(f_header_ref),
    F_RES_NORM(f_res_norm_ref),
    F_VOL_NORM(f_vol_norm_ref),
    F_VOL_CLIENT(f_vol_client_ref),
    F_VOL_MINOR(f_vol_minor_ref),
    F_CONN_NORM(f_conn_norm_ref),
    F_PRIMARY(f_primary_ref),
    F_SECONDARY(f_secondary_ref),
    F_SYNC_PERC(f_sync_perc_ref),
    QUORUM_ALERT(quorum_alert_ref),
    F_CONN_PRI_FG(f_conn_pri_fg_ref),
    F_RES_NAME(f_res_name_ref),
    F_RES_COUNT(f_res_count_ref),
    F_PRB_COUNT(f_prb_count_ref),
    F_CURSOR_POS(f_cursor_pos_ref),
    F_HOTKEY(f_hotkey_ref),
    F_PRB_HOTKEY(f_prb_hotkey_ref),
    F_ALERT_HOTKEY(f_alert_hotkey_ref),
    F_PAGE(f_page_ref),
    F_GOTO_PAGE(f_goto_page_ref),
    F_SYNC_BLK(f_sync_blk_ref),
    F_UNSYNC_BLK(f_unsync_blk_ref)
{
}

CompactDisplay::ProgressBar::ProgressBar(
    CompactDisplay* const dsp_ref,
    const bool utf8_mode
):
    dsp(dsp_ref)
{
    if (utf8_mode)
    {
        sync_blk = UTF8_SYNC_BLK;
        unsync_blk = UTF8_UNSYNC_BLK;
    }
    else
    {
        sync_blk = ASCII_SYNC_BLK;
        unsync_blk = ASCII_UNSYNC_BLK;
    }

    // Create the progress bar rendering source buffers
    sync_blk_width = std::strlen(UTF8_SYNC_BLK);
    unsync_blk_width = std::strlen(UTF8_UNSYNC_BLK);

    sync_blk_buffer_mgr = std::unique_ptr<char[]>(new char[MAX_SYNC_BAR_SIZE * sync_blk_width]);
    unsync_blk_buffer_mgr = std::unique_ptr<char[]>(new char[MAX_SYNC_BAR_SIZE * unsync_blk_width]);

    char* const init_sync_blk_buffer = sync_blk_buffer_mgr.get();
    char* const init_unsync_blk_buffer = unsync_blk_buffer_mgr.get();

    for (size_t idx = 0; idx < MAX_SYNC_BAR_SIZE; ++idx)
    {
        size_t utf8_offset = idx * sync_blk_width;
        for (size_t byte_offset = 0; byte_offset < sync_blk_width; ++byte_offset)
        {
            init_sync_blk_buffer[utf8_offset + byte_offset] = UTF8_SYNC_BLK[byte_offset];
        }
    }

    for (size_t idx = 0; idx < MAX_SYNC_BAR_SIZE; ++idx)
    {
        size_t utf8_offset = idx * unsync_blk_width;
        for (size_t byte_offset = 0; byte_offset < unsync_blk_width; ++byte_offset)
        {
            init_unsync_blk_buffer[utf8_offset + byte_offset] = UTF8_UNSYNC_BLK[byte_offset];
        }
    }

    sync_blk_buffer = sync_blk_buffer_mgr.get();
    unsync_blk_buffer = unsync_blk_buffer_mgr.get();
}

CompactDisplay::ProgressBar::~ProgressBar() noexcept
{
}

CompactDisplay::ProgressBar::ProgressBar(ProgressBar&& orig):
    dsp(orig.dsp),
    sync_blk(orig.sync_blk),
    unsync_blk(orig.unsync_blk),
    sync_blk_buffer_mgr(std::move(orig.sync_blk_buffer_mgr)),
    unsync_blk_buffer_mgr(std::move(orig.unsync_blk_buffer_mgr)),
    sync_blk_buffer(orig.sync_blk_buffer),
    unsync_blk_buffer(orig.unsync_blk_buffer)
{
    orig.dsp = nullptr;
    orig.sync_blk_buffer = nullptr;
    orig.unsync_blk_buffer = nullptr;
}

CompactDisplay::ProgressBar& CompactDisplay::ProgressBar::operator=(CompactDisplay::ProgressBar&& orig)
{
    if (this != &orig)
    {
        dsp = orig.dsp;

        sync_blk = orig.sync_blk;
        unsync_blk = orig.unsync_blk;

        sync_blk_buffer_mgr = std::move(orig.sync_blk_buffer_mgr);
        unsync_blk_buffer_mgr = std::move(orig.unsync_blk_buffer_mgr);

        sync_blk_buffer = orig.sync_blk_buffer;
        unsync_blk_buffer = orig.unsync_blk_buffer;

        orig.dsp = nullptr;
        orig.sync_blk_buffer = nullptr;
        orig.unsync_blk_buffer = nullptr;
    }
    return *this;
}

void CompactDisplay::ProgressBar::display_progress_bar(const uint16_t width, const uint16_t sync_perc) const
{
    const uint16_t safe_width = dsaext::generic_bounds<const uint16_t>(MIN_SYNC_BAR_SIZE, width, MAX_SYNC_BAR_SIZE);
    const uint16_t safe_sync_perc = std::min<const uint16_t>(MAX_SYNC_PERC, sync_perc);
    const uint16_t sync_bar_length = static_cast<const uint16_t> (
        static_cast<uint32_t> (safe_sync_perc) * safe_width / MAX_SYNC_PERC
    );
    const uint16_t unsync_bar_length = safe_width - sync_bar_length;

    dsp->write_text(dsp->dsp_fmt->F_SYNC_BLK);
    dsp->write_buffer(sync_blk_buffer, sync_bar_length * sync_blk_width);
    dsp->write_text(dsp->dsp_fmt->F_UNSYNC_BLK);
    dsp->write_buffer(unsync_blk_buffer, unsync_bar_length * unsync_blk_width);
    dsp->write_text(dsp->dsp_fmt->F_RESET);
}
