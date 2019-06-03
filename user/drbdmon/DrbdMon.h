#ifndef LIVESTATUS_H
#define	LIVESTATUS_H

#include <new>
#include <memory>
#include <cstdint>

#include <map_types.h>
// https://github.com/raltnoeder/cppdsaext
#include <dsaext.h>
#include <DrbdResource.h>
#include <DrbdConnection.h>
#include <DrbdVolume.h>
#include <VolumesContainer.h>
#include <StringTokenizer.h>
#include <GenericDisplay.h>
#include <EventsSourceSpawner.h>
#include <EventsIo.h>
#include <MessageLog.h>
#include <Configurable.h>
#include <Configurator.h>
#include <comparators.h>
#include <IntervalTimer.h>
#include <colormodes.h>

extern "C"
{
    #include <time.h>
}

class DrbdMon : public Configurable, public Configurator
{
  public:
    static const std::string PROGRAM_NAME;
    static const std::string VERSION;

    static const std::string TOKEN_DELIMITER;

    static const char DEBUG_SEQ_PFX;

    static const std::string OPT_HELP_KEY;
    static const std::string OPT_VERSION_KEY;
    static const std::string OPT_FREQ_LMT_KEY;
    static const ConfigOption OPT_HELP;
    static const ConfigOption OPT_VERSION;
    static const ConfigOption OPT_FREQ_LMT;

    // Environment variable for color mode selection
    static const char* ENV_COLOR_MODE;
    static const char* COLOR_MODE_EXTENDED;
    static const char* COLOR_MODE_BASIC;

    static const std::string UNIT_SFX_SECONDS;
    static const std::string UNIT_SFX_MILLISECONDS;
    static const uint16_t MAX_INTERVAL;
    static const uint16_t DFLT_INTERVAL;

    static const std::string TYPE_RESOURCE;
    static const std::string TYPE_CONNECTION;
    static const std::string TYPE_DEVICE;
    static const std::string TYPE_PEER_DEVICE;
    static const std::string TYPE_SEPARATOR;

    static const std::string MODE_EXISTS;
    static const std::string MODE_CREATE;
    static const std::string MODE_CHANGE;
    static const std::string MODE_DESTROY;

    static const char HOTKEY_QUIT;
    static const char HOTKEY_REPAINT;
    static const char HOTKEY_REINIT;
    static const char HOTKEY_VERSION;

    static const std::string DESC_QUIT;
    static const std::string DESC_REPAINT;
    static const std::string DESC_CLEAR_MSG;
    static const std::string DESC_REINIT;


    static const size_t MAX_LINE_LENGTH {1024};

    // DrbdMon' normal behavior is to update the display only after
    // all events have been read and no more events are pending.
    // If new event lines arrive faster than DrbdMon can read them,
    // so that there are always events pending, do still update the
    // display every MAX_EVENT_BUNDLE events
    static const uint32_t MAX_EVENT_BUNDLE {911};

    enum class fail_info : uint16_t
    {
        NONE,
        GENERIC,
        OUT_OF_MEMORY,
        EVENTS_SOURCE,
        EVENTS_IO
    };

    enum class finish_action : uint16_t
    {
        RESTART_IMMED,
        RESTART_DELAYED,
        TERMINATE,
        TERMINATE_NO_CLEAR,
        DEBUG_MODE
    };

    const color_mode drbdmon_colors;

    // @throws std::bad_alloc
    DrbdMon(
        int argc,
        char* argv[],
        MessageLog& log_ref,
        MessageLog& debug_log_ref,
        fail_info& fail_data_ref,
        const std::string* const node_name_ref,
        const color_mode colors
    );
    DrbdMon(const DrbdMon& orig) = delete;
    DrbdMon& operator=(const DrbdMon& orig) = delete;
    DrbdMon(DrbdMon&& orig) = default;
    DrbdMon& operator=(DrbdMon&& orig) = delete;
    virtual ~DrbdMon() noexcept;

    // @throws std::bad_alloc
    virtual void run();

    // @throws EventMessageException, EventObjectException
    virtual void tokenize_event_message(std::string& event_line, PropsMap& event_props);

    // @throws EventMessageException, EventObjectException
    virtual void process_event_message(
        std::string& mode,
        std::string& type,
        PropsMap& event_props,
        std::string& event_line
    );
    // Returns the action requested to be taken upon return from this class' run() method.
    // This method should be called only after run() has returned.
    virtual finish_action get_fin_action() const;

    // @throws std::bad_alloc
    virtual void add_config_option(Configurable& owner, const ConfigOption& option) override;

    // @throws std::bad_alloc
    virtual void announce_options(Configurator& collector) override;

    virtual void options_help() noexcept override;

    // @throws std::bad_alloc
    virtual void set_flag(std::string& key) override;

    // @throws std::bad_alloc
    virtual void set_option(std::string& key, std::string& value) override;

    virtual uint64_t get_problem_count() const noexcept;

  private:
    typedef struct option_entry_s
    {
        Configurable&       owner;
        const ConfigOption& option;
    }
    option_entry;

    using OptionsMap = QTree<const std::string, DrbdMon::option_entry>;

    const int    arg_count;
    char** const arg_values;

    const std::unique_ptr<ResourcesMap> resources_map;
    const std::unique_ptr<HotkeysMap>   hotkeys_info;

    std::unique_ptr<OptionsMap>   options;

    fail_info&    fail_data;
    finish_action fin_action {DrbdMon::finish_action::RESTART_IMMED};
    MessageLog&   log;
    MessageLog&   debug_log;
    const std::string* const node_name;

    bool          shutdown      {false};
    bool          have_initial_state {false};

    uint64_t problem_count {0};

    std::unique_ptr<GenericDisplay>  display {nullptr};
    std::unique_ptr<Configurable*[]> configurables {nullptr};

    std::unique_ptr<IntervalTimer> interval_timer_mgr {nullptr};
    struct timespec prev_timestamp {0, 0};
    struct timespec cur_timestamp {0, 0};
    bool use_dflt_freq_lmt {true};

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

    // Loads the event_props map with the properties contained in tokens
    //
    // @param: tokens StringTokenizer that returns the 'key:value' tokens from a 'drbdsetup event2' line
    // @param: event_props Property map to load the key == value mappings into
    // @throws std::bad_alloc
    void parse_event_props(StringTokenizer& tokens, PropsMap& event_props, std::string& event_line);
    // Clears the event_props map and frees all its entries
    void clear_event_props(PropsMap& event_props);

    // Sets up the hotkeys information map
    // @throws std::bad_alloc
    void setup_hotkeys_info();

    // Configures options (command line arguments)
    // configurables is a nullptr-terminated array of pointers to Configurable instances
    // @throws std::bad_alloc
    void configure_options();

    // Frees the options map
    void options_cleanup() noexcept;

    void problem_counter_update(StateFlags::state res_last_state, StateFlags::state res_new_state) noexcept;

    void disable_interval_timer(bool& timer_available, bool& timer_armed) noexcept;

    // @throws TimerException
    inline bool is_interval_exceeded();

    inline void cond_display_update(bool& timer_available, bool& timer_armed) noexcept;
    inline void update_timestamp(bool& timer_available, bool& timer_armed) noexcept;

    // Frees resources
    // @throws std::bad_alloc
    void cleanup(
        PropsMap*               event_props,
        EventsIo*               events_io
    );
};

#endif	/* LIVESTATUS_H */
