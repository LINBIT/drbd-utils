#ifndef LIVESTATUS_H
#define	LIVESTATUS_H

#include <default_types.h>
#include <new>
#include <memory>

#include <DrbdMonCore.h>
#include <MonitorEnvironment.h>
#include <map_types.h>
// https://github.com/raltnoeder/cppdsaext
#include <dsaext.h>
#include <objects/ResourceDirectory.h>
#include <objects/DrbdResource.h>
#include <objects/DrbdConnection.h>
#include <objects/DrbdVolume.h>
#include <objects/VolumesContainer.h>
#include <StringTokenizer.h>
#include <terminal/GenericDisplay.h>
#include <subprocess/EventsSourceSpawner.h>
#include <MessageLog.h>
#include <Configurable.h>
#include <Configurator.h>
#include <comparators.h>
#include <IntervalTimer.h>

// FIXME: Move to system_api
#include <platform/Linux/EventsIo.h>

extern "C"
{
    #include <time.h>
}

class DrbdMon : public DrbdMonCore, public Configurable, public Configurator
{
  public:
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
    static const std::string MODE_RENAME;
    static const std::string MODE_DESTROY;

    static const size_t MAX_LINE_LENGTH {1024};

    // DrbdMon' normal behavior is to update the display only after
    // all events have been read and no more events are pending.
    // If new event lines arrive faster than DrbdMon can read them,
    // so that there are always events pending, do still update the
    // display every MAX_EVENT_BUNDLE events
    static const uint32_t MAX_EVENT_BUNDLE {911};

    // @throws std::bad_alloc
    DrbdMon(
        int                         argc,
        char*                       argv[],
        MonitorEnvironment&         mon_env_ref
    );
    DrbdMon(const DrbdMon& orig) = delete;
    DrbdMon& operator=(const DrbdMon& orig) = delete;
    DrbdMon(DrbdMon&& orig) = default;
    DrbdMon& operator=(DrbdMon&& orig) = delete;
    virtual ~DrbdMon() noexcept;

    virtual SystemApi& get_system_api() const noexcept override;

    // @throws std::bad_alloc
    virtual void run();

    virtual void shutdown(const DrbdMonCore::finish_action action) noexcept override;

    // @throws EventMessageException, EventObjectException
    virtual void tokenize_event_message(GenericDisplay* const display, std::string& event_line, PropsMap& event_props);

    // @throws EventMessageException, EventObjectException
    virtual void process_event_message(
        GenericDisplay* const display,
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

    virtual uint32_t get_problem_count() const noexcept override;

    virtual void notify_config_changed() override;

  private:
    typedef struct option_entry_s
    {
        Configurable&       owner;
        const ConfigOption& option;
    }
    option_entry;

    MonitorEnvironment& mon_env;

    using OptionsMap = QTree<const std::string, DrbdMon::option_entry>;

    const int    arg_count;
    char** const arg_values;

    std::unique_ptr<ResourceDirectory>  rsc_dir_mgr;
    ResourceDirectory*                  rsc_dir;

    std::unique_ptr<OptionsMap>   options;

    MessageLog&                 log;
    MessageLog&                 debug_log;
    Configuration&              config;

    bool    shutdown_flag       {false};
    bool    have_initial_state  {false};
    bool    timer_available     {false};
    bool    timer_armed         {false};

    std::unique_ptr<Configurable*[]> configurables {nullptr};

    std::unique_ptr<IntervalTimer> interval_timer_mgr {nullptr};
    struct timespec prev_timestamp {0, 0};
    struct timespec cur_timestamp {0, 0};
    bool use_dflt_freq_lmt {true};

    // Loads the event_props map with the properties contained in tokens
    //
    // @param: tokens StringTokenizer that returns the 'key:value' tokens from a 'drbdsetup event2' line
    // @param: event_props Property map to load the key == value mappings into
    // @throws std::bad_alloc
    void parse_event_props(StringTokenizer& tokens, PropsMap& event_props, std::string& event_line);
    // Clears the event_props map and frees all its entries
    void clear_event_props(PropsMap& event_props);

    // Configures options (command line arguments)
    // configurables is a nullptr-terminated array of pointers to Configurable instances
    // @throws std::bad_alloc
    void configure_options();

    // Frees the options map
    void options_cleanup() noexcept;

    void disable_interval_timer() noexcept;

    // @throws TimerException
    inline bool is_interval_exceeded();

    inline void cond_display_update(GenericDisplay* const display);
    inline void update_timestamp() noexcept;

    // Frees resources
    // @throws std::bad_alloc
    void cleanup(
        PropsMap*               event_props,
        EventsIo*               events_io
    );
};

#endif	/* LIVESTATUS_H */
