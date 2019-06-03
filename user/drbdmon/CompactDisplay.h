#ifndef COMPACTDISPLAY_H
#define	COMPACTDISPLAY_H

#include <new>
#include <memory>
#include <stdexcept>
#include <string>
#include <cstdint>

extern "C"
{
    #include <unistd.h>
    #include <time.h>
}

#include <GenericDisplay.h>
#include <map_types.h>
#include <DrbdResource.h>
#include <DrbdConnection.h>
#include <DrbdVolume.h>
#include <DrbdRole.h>
#include <MessageLog.h>
#include <Configurable.h>
#include <ConfigOption.h>
#include <colormodes.h>

class DrbdMon;

class CompactDisplay : public GenericDisplay, public Configurable
{
  public:
    static const std::string OPT_NO_HEADER_KEY;
    static const std::string OPT_NO_HOTKEYS_KEY;
    static const std::string OPT_ASCII_KEY;
    static const std::string OPT_PROBLEMS_KEY;

    static const std::string HDR_SPACER;
    static const std::string NODE_DSP_PREFIX;
    static const std::string TRUNC_MARKER;

    static const ConfigOption OPT_NO_HEADER;
    static const ConfigOption OPT_NO_HOTKEYS;
    static const ConfigOption OPT_ASCII;
    static const ConfigOption OPT_PROBLEMS;

    static const int QUORUM_ALERT_WIDTH;


    static const int   PAGE_POS_R;
    static const int   GOTO_PAGE_POS_R;
    static const int   GOTO_PAGE_CURSOR_POS;

    static const char* UTF8_PRIMARY;
    static const char* UTF8_SECONDARY;
    static const char* UTF8_CONN_GOOD;
    static const char* UTF8_CONN_BAD;
    static const char* UTF8_DISK_GOOD;
    static const char* UTF8_DISK_BAD;
    static const char* UTF8_MARK_OFF;
    static const char* UTF8_MARK_ON;

    static const char* ASCII_PRIMARY;
    static const char* ASCII_SECONDARY;
    static const char* ASCII_CONN_GOOD;
    static const char* ASCII_CONN_BAD;
    static const char* ASCII_DISK_GOOD;
    static const char* ASCII_DISK_BAD;
    static const char* ASCII_MARK_OFF;
    static const char* ASCII_MARK_ON;

    static const char* ANSI_CLEAR;
    static const char* ANSI_CLEAR_LINE;
    static const char* ANSI_CURSOR_OFF;
    static const char* ANSI_CURSOR_ON;
    static const char* ANSI_ALTBFR_ON;
    static const char* ANSI_ALTBFR_OFF;

    static const char HOTKEY_MESSAGES;
    static const char HOTKEY_CLEAR_MSG;
    static const char HOTKEY_PGUP;
    static const char HOTKEY_PGDN;
    static const char HOTKEY_PGONE;

    static const char KEY_BACKSPACE;
    static const char KEY_CTRL_H;
    static const char KEY_NEWLINE;
    static const char KEY_ENTER;
    static const char KEY_ESC;

    static const std::string LABEL_MESSAGES;
    static const std::string LABEL_CLEAR_MSG;
    static const std::string LABEL_MONITOR;
    static const std::string LABEL_PROBLEMS;
    static const std::string LABEL_STATUS;
    static const std::string LABEL_PGUP;
    static const std::string LABEL_PGDN;
    static const std::string LABEL_PGZERO;

    static const uint16_t INDENT_STEP_SIZE;
    static const uint16_t OUTPUT_BUFFER_SIZE;

    static const uint16_t MIN_SIZE_X;
    static const uint16_t MAX_SIZE_X;
    static const uint16_t MIN_SIZE_Y;
    static const uint16_t MAX_SIZE_Y;

    static const int RES_NAME_WIDTH;
    static const int ROLE_WIDTH;
    static const int VOL_NR_WIDTH;
    static const int MINOR_NR_WIDTH;
    static const int CONN_NAME_WIDTH;
    static const int CONN_STATE_WIDTH;
    static const int DISK_STATE_WIDTH;
    static const int REPL_STATE_WIDTH;

    static const uint16_t MIN_NODENAME_DSP_LENGTH;
    static const uint32_t MAX_YIELD_LOOP;

    // @throws std::bad_alloc
    CompactDisplay(
        DrbdMon& drbdmon_ref,
        ResourcesMap& resources_map_ref,
        MessageLog&   log_ref,
        HotkeysMap&   hotkeys_info_ref,
        const std::string* const node_name_ref,
        const color_mode colors
    );
    CompactDisplay(const CompactDisplay& orig) = delete;
    CompactDisplay& operator=(const CompactDisplay& orig) = delete;
    CompactDisplay(CompactDisplay&& orig) = delete;
    CompactDisplay& operator=(CompactDisplay&& orig) = delete;
    virtual ~CompactDisplay() noexcept override;

    virtual void clear();
    virtual void initial_display() override;
    virtual void status_display() override;
    virtual void display_header() const override;
    virtual void display_hotkeys_info() const;
    virtual void display_counts() const;

    virtual void set_terminal_size(uint16_t size_x, uint16_t size_y) override;
    virtual void set_utf8(bool enable);

    // @throws std::bad_alloc
    virtual void announce_options(Configurator& collector) override;

    virtual void options_help() noexcept override;

    // @throws std::bad_alloc
    virtual void set_flag(std::string& key) override;

    // @throws std::bad_alloc
    virtual void set_option(std::string& key, std::string& value) override;

    virtual void key_pressed(const char key) override;

  private:
    enum class input_mode : uint16_t
    {
        HOTKEYS,
        PAGE_NR
    };

    class DisplayFormats
    {
      public:
        const char* const F_NORM;
        const char* const F_WARN;
        const char* const F_ALERT;
        const char* const F_RESET;

        const char* const F_MARK;

        const char* const F_HEADER;

        const char* const F_RES_NORM;
        const char* const F_VOL_NORM;
        const char* const F_VOL_CLIENT;
        const char* const F_VOL_MINOR;
        const char* const F_CONN_NORM;
        const char* const F_PRIMARY;
        const char* const F_SECONDARY;
        const char* const F_SYNC_PERC;
        const char* const QUORUM_ALERT;

        // Foreground color for the 'Primary' role highlighting
        // of connections (peer resource role)
        const char* const F_CONN_PRI_FG;

        const char* const F_RES_NAME;
        const char* const F_RES_COUNT;
        const char* const F_PRB_COUNT;

        const char* const F_CURSOR_POS;
        const char* const F_HOTKEY;
        const char* const F_PRB_HOTKEY;
        const char* const F_ALERT_HOTKEY;

        const char* const F_PAGE;
        const char* const F_GOTO_PAGE;

        const char* const F_SYNC_BLK;
        const char* const F_UNSYNC_BLK;

        DisplayFormats(
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
        );
        DisplayFormats(const DisplayFormats& orig) = default;
        DisplayFormats(DisplayFormats&& orig) = default;
        DisplayFormats& operator=(const DisplayFormats& orig) = delete;
        DisplayFormats& operator=(DisplayFormats&& orig) = delete;
    };

    class ProgressBar
    {
      public:
        static const uint16_t MIN_SYNC_BAR_SIZE;
        static const uint16_t MAX_SYNC_BAR_SIZE;

        static const uint16_t MAX_PERC;
        static const uint16_t MAX_SYNC_PERC;

        static const char* UTF8_SYNC_BLK;
        static const char* UTF8_UNSYNC_BLK;

        static const char* ASCII_SYNC_BLK;
        static const char* ASCII_UNSYNC_BLK;

      private:
        CompactDisplay* dsp;
        size_t sync_blk_width {0};
        size_t unsync_blk_width {0};
        const char* sync_blk;
        const char* unsync_blk;
        std::unique_ptr<char[]> sync_blk_buffer_mgr {nullptr};
        std::unique_ptr<char[]> unsync_blk_buffer_mgr {nullptr};
        const char* sync_blk_buffer {nullptr};
        const char* unsync_blk_buffer {nullptr};

      public:
        ProgressBar(CompactDisplay* const dsp_ref, const bool utf8_mode);
        virtual ~ProgressBar() noexcept;

        ProgressBar(const ProgressBar& orig) = delete;
        virtual ProgressBar& operator=(const ProgressBar& orig) = delete;
        ProgressBar(ProgressBar&& orig);
        virtual ProgressBar& operator=(ProgressBar&& orig);

        virtual void display_progress_bar(const uint16_t width, const uint16_t sync_perc) const;
    };

    static const DisplayFormats DSP_FMT_16COLOR;
    static const DisplayFormats DSP_FMT_256COLOR;

    DrbdMon& drbdmon;
    ResourcesMap& resources_map;
    MessageLog& log;
    HotkeysMap& hotkeys_info;

    int output_fd {STDOUT_FILENO};

    bool alt_bfr_active {false};
    bool dsp_msg_active {false};
    bool dsp_problems_active {false};
    bool problem_alert {false};
    bool debug_view {false};
    uint16_t page {0};
    uint32_t page_start {0};
    uint32_t page_end {0};
    uint32_t goto_page {0};

    uint16_t term_x {80};
    uint16_t term_y {25};
    uint16_t node_dsp_length {0};
    uint16_t current_x {0};
    uint32_t current_y {0};
    bool show_header {true};
    bool show_hotkeys {true};
    bool enable_term_size {true};

    input_mode mode { input_mode::HOTKEYS };

    bool enable_utf8 {true};
    const char* pri_icon  {UTF8_PRIMARY};
    const char* sec_icon  {UTF8_SECONDARY};
    const char* conn_good {UTF8_CONN_GOOD};
    const char* conn_bad  {UTF8_CONN_BAD};
    const char* disk_good {UTF8_DISK_GOOD};
    const char* disk_bad  {UTF8_DISK_BAD};
    const char* mark_off  {UTF8_MARK_OFF};
    const char* mark_on   {UTF8_MARK_ON};

    uint16_t indent_level {0};
    char* indent_buffer {nullptr};
    std::unique_ptr<char[]> indent_buffer_mgr {nullptr};
    char* output_buffer {nullptr};
    std::unique_ptr<char[]> output_buffer_mgr {nullptr};

    std::unique_ptr<std::string> node_label {nullptr};
    const std::string* const node_name;

    std::unique_ptr<CompactDisplay::ProgressBar> sync_progress_mgr {nullptr};
    const CompactDisplay::ProgressBar* sync_progress {nullptr};

    // 20 ms delay
    struct timespec write_retry_delay {0, 20000000};

    const DisplayFormats* dsp_fmt;

    void page_nav_display() const;
    void page_nav_cursor() const;
    bool list_resources();
    void list_connections(DrbdResource& res);
    void list_volumes(DrbdResource& res);
    void show_volume(DrbdVolume& vol, bool peer_volume, bool long_format);
    void list_peer_volumes(DrbdConnection& conn);

    void increase_indent();
    void decrease_indent();
    void reset_indent();
    void reset_positions();
    void indent();
    bool next_column(uint16_t length);
    void next_line();
    void problem_check();

    void write_char(const char ch) const noexcept;
    void write_text(const char* text) const noexcept;
    void write_fmt(const char* format, ...) const noexcept;
    void write_string_field(const std::string& text, size_t field_width, bool fill) const noexcept;

    void write_buffer(const char* buffer, const size_t write_length) const noexcept;
};

#endif	/* COMPACTDISPLAY_H */
