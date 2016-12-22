#ifndef COMPACTDISPLAY_H
#define	COMPACTDISPLAY_H

#include <new>
#include <stdexcept>
#include <iostream>
#include <iomanip>
#include <string>
#include <cstdint>

#include <GenericDisplay.h>
#include <map_types.h>
#include <DrbdResource.h>
#include <DrbdConnection.h>
#include <DrbdVolume.h>
#include <DrbdRole.h>
#include <MessageLog.h>
#include <Configurable.h>
#include <ConfigOption.h>

class CompactDisplay : public GenericDisplay, public Configurable
{
  public:
    static const std::string OPT_NO_HEADER_KEY;
    static const std::string OPT_NO_HOTKEYS_KEY;
    static const std::string OPT_ASCII_KEY;

    static const ConfigOption OPT_NO_HEADER;
    static const ConfigOption OPT_NO_HOTKEYS;
    static const ConfigOption OPT_ASCII;

    static const char* F_NORM;
    static const char* F_WARN;
    static const char* F_ALERT;
    static const char* F_RESET;

    static const char* F_MARK;

    static const char* F_HEADER;

    static const char* F_RES_NORM;
    static const char* F_RES_NAME_NORM;
    static const char* F_RES_NAME_ALERT;
    static const char* F_VOL_NORM;
    static const char* F_VOL_CLIENT;
    static const char* F_VOL_MINOR;
    static const char* F_CONN_NORM;
    static const char* F_PRIMARY;
    static const char* F_SECONDARY;

    static const char* F_CONN_PRI_FG;

    static const char* F_RES_NAME;

    static const char* F_CURSOR_POS;
    static const char* F_HOTKEY;
    static const char* F_ALERT_HOTKEY;

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

    static const char HOTKEY_PGUP;
    static const char HOTKEY_PGDN;
    static const char HOTKEY_PGZERO;

    static const std::string LABEL_MESSAGES;
    static const std::string LABEL_MONITOR;
    static const std::string LABEL_PGUP;
    static const std::string LABEL_PGDN;
    static const std::string LABEL_PGZERO;

    static const uint16_t INDENT_STEP_SIZE;

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

    // @throws std::bad_alloc
    CompactDisplay(
        std::ostream& out_ref,
        ResourcesMap& resources_map_ref,
        MessageLog&   log_ref,
        HotkeysMap&   hotkeys_info_ref
    );
    CompactDisplay(const CompactDisplay& orig) = delete;
    CompactDisplay& operator=(const CompactDisplay& orig) = delete;
    CompactDisplay(CompactDisplay&& orig) = delete;
    CompactDisplay& operator=(CompactDisplay&& orig) = delete;
    virtual ~CompactDisplay() noexcept override;

    virtual void clear();
    virtual void status_display() override;
    virtual void display_header() const override;
    virtual void display_hotkeys_info() const;

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
    void list_resources();
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
    void prepare_flags();

    ResourcesMap& resources_map;
    std::ostream& out;
    MessageLog& log;
    HotkeysMap& hotkeys_info;

    bool dsp_msg_active {false};
    uint16_t page {0};
    uint32_t page_start {0};
    uint32_t page_end {0};

    uint16_t term_x {80};
    uint16_t term_y {25};
    char* indent_buffer {nullptr};
    uint16_t indent_level {0};
    uint16_t current_x {0};
    uint32_t current_y {0};
    bool show_header {true};
    bool show_hotkeys {true};
    bool enable_term_size {true};

    bool enable_utf8 {true};
    const char* pri_icon  {UTF8_PRIMARY};
    const char* sec_icon  {UTF8_SECONDARY};
    const char* conn_good {UTF8_CONN_GOOD};
    const char* conn_bad  {UTF8_CONN_BAD};
    const char* disk_good {UTF8_DISK_GOOD};
    const char* disk_bad  {UTF8_DISK_BAD};
    const char* mark_off  {UTF8_MARK_OFF};
    const char* mark_on   {UTF8_MARK_ON};
};

#endif	/* COMPACTDISPLAY_H */
