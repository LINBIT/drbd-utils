#include <new>
#include <ios>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <cstdlib>

extern "C"
{
    #include <errno.h>
    #include <signal.h>
    #include <config.h>
    #include <drbd_buildtag.h>
}

#include <map_types.h>
// https://github.com/raltnoeder/cppdsaext
#include <dsaext.h>
#include <integerparse.h>

#include <DrbdMon.h>
#include <utils.h>
#include <CompactDisplay.h>
#include <ConfigOption.h>
#include <Args.h>
#include <comparators.h>

const std::string DrbdMon::PROGRAM_NAME = "DRBD DrbdMon";
const std::string DrbdMon::VERSION = PACKAGE_VERSION;

const std::string DrbdMon::OPT_HELP_KEY = "help";
const std::string DrbdMon::OPT_VERSION_KEY = "version";
const std::string DrbdMon::OPT_FREQ_LMT_KEY = "freqlmt";
const ConfigOption DrbdMon::OPT_HELP(true, OPT_HELP_KEY);
const ConfigOption DrbdMon::OPT_VERSION(true, OPT_VERSION_KEY);
const ConfigOption DrbdMon::OPT_FREQ_LMT(false, OPT_FREQ_LMT_KEY);

const char* DrbdMon::ENV_COLOR_MODE         = "DRBDMON_COLORS";
const char* DrbdMon::COLOR_MODE_EXTENDED    = "extended";
const char* DrbdMon::COLOR_MODE_BASIC       = "basic";

const std::string DrbdMon::UNIT_SFX_SECONDS = "s";
const std::string DrbdMon::UNIT_SFX_MILLISECONDS = "ms";
const uint16_t DrbdMon::MAX_INTERVAL = 10000;
const uint16_t DrbdMon::DFLT_INTERVAL = 40;

const std::string DrbdMon::TOKEN_DELIMITER = " ";

const char DrbdMon::DEBUG_SEQ_PFX = '~';

const std::string DrbdMon::MODE_EXISTS  = "exists";
const std::string DrbdMon::MODE_CREATE  = "create";
const std::string DrbdMon::MODE_CHANGE  = "change";
const std::string DrbdMon::MODE_DESTROY = "destroy";

const std::string DrbdMon::TYPE_RESOURCE    = "resource";
const std::string DrbdMon::TYPE_CONNECTION  = "connection";
const std::string DrbdMon::TYPE_DEVICE      = "device";
const std::string DrbdMon::TYPE_PEER_DEVICE = "peer-device";
const std::string DrbdMon::TYPE_SEPARATOR   = "-";

const char DrbdMon::HOTKEY_QUIT       = 'q';
const char DrbdMon::HOTKEY_REPAINT    = 'r';
const char DrbdMon::HOTKEY_REINIT     = 'R';
const char DrbdMon::HOTKEY_VERSION    = 'V';

const std::string DrbdMon::DESC_QUIT      = "Quit";
const std::string DrbdMon::DESC_REPAINT   = "Repaint";
const std::string DrbdMon::DESC_CLEAR_MSG = "Clear messages";
const std::string DrbdMon::DESC_REINIT    = "Reinitialize";

static bool string_ends_with(const std::string& text, const std::string& suffix);

// @throws std::bad_alloc
DrbdMon::DrbdMon(
    int         argc,
    char*       argv[],
    MessageLog& log_ref,
    MessageLog& debug_log_ref,
    fail_info&  fail_data_ref,
    const std::string* const node_name_ref,
    const color_mode colors
):
    drbdmon_colors(colors),
    arg_count(argc),
    arg_values(argv),
    resources_map(new ResourcesMap(&comparators::compare_string)),
    hotkeys_info(new HotkeysMap(&dsaext::generic_compare<char>)),
    fail_data(fail_data_ref),
    log(log_ref),
    debug_log(debug_log_ref),
    node_name(node_name_ref)
{
}

DrbdMon::~DrbdMon() noexcept
{
    // Cleanup resources map
    {
        ResourcesMap::NodesIterator dtor_iter(*resources_map);
        // Free all DrbdResource mappings
        while (dtor_iter.has_next())
        {
            ResourcesMap::Node* node = dtor_iter.next();
            delete node->get_key();
            delete node->get_value();
        }
        resources_map->clear();
    }

    // Cleanup hotkeys map
    // Keys/values in this map are static members of the DrbdMon class
    hotkeys_info->clear();
}

// @throws std::bad_alloc
void DrbdMon::run()
{
    std::unique_ptr<PropsMap>               event_props;
    std::unique_ptr<TermSize>               term_size;
    std::unique_ptr<EventsSourceSpawner>    events_source;
    std::unique_ptr<EventsIo>               events_io;

    try
    {
        setup_hotkeys_info();
        try
        {
            event_props   = std::unique_ptr<PropsMap>(new PropsMap(&comparators::compare_string));
            term_size     = std::unique_ptr<TermSize>(new TermSize());

            // Create the display and keep a pointer of the implementation's type for later use
            // as a configurable object
            // Do not add anything that might throw between display_impl allocation and
            // display interface unique_ptr initialization to ensure deallocation of the display
            // object if the current scope is left
            CompactDisplay* display_impl = new CompactDisplay(
                *this, *resources_map, log, *hotkeys_info, node_name, drbdmon_colors
            );
            display = std::unique_ptr<GenericDisplay>(dynamic_cast<GenericDisplay*> (display_impl));

            configurables = std::unique_ptr<Configurable*[]>(new Configurable*[3]);
            configurables[0] = dynamic_cast<Configurable*> (this);
            configurables[1] = dynamic_cast<Configurable*> (display_impl);
            configurables[2] = nullptr;
            configure_options();

            events_source = std::unique_ptr<EventsSourceSpawner>(new EventsSourceSpawner(log));

            if (term_size->probe_terminal_size())
            {
                display->set_terminal_size(term_size->get_size_x(), term_size->get_size_y());
            }

            events_source->spawn_source();
            events_io = std::unique_ptr<EventsIo>(
                new EventsIo(events_source->get_events_out_fd(), events_source->get_events_err_fd())
            );

            // Cleanup any zombies that might not have been collected,
            // because SIGCHLD is blocked during reinitialization
            events_source->cleanup_child_processes();

            try
            {
                events_io->adjust_terminal();
            }
            catch (EventsIoException&)
            {
                log.add_entry(MessageLog::log_level::WARN, "Adjusting the terminal mode failed");
            }

            // Show an initial display while reading the initial DRBD status
            display->initial_display();

            if (use_dflt_freq_lmt && interval_timer_mgr.get() == nullptr)
            {
                interval_timer_mgr = std::unique_ptr<IntervalTimer>(new IntervalTimer(log, DFLT_INTERVAL));
            }

            bool debug_key {false};
            bool timer_available = (interval_timer_mgr.get() != nullptr);
            bool timer_armed {false};
            while (!shutdown)
            {
                EventsIo::event event_id = events_io->wait_event();
                switch (event_id)
                {
                    case EventsIo::event::EVENT_LINE:
                        {
                            std::string* event_line = events_io->get_event_line();
                            if (event_line != nullptr)
                            {
                                uint32_t update_counter {0};
                                while (event_line != nullptr)
                                {
                                    tokenize_event_message(*event_line, *event_props);
                                    events_io->free_event_line();
                                    clear_event_props(*event_props);
                                    event_line = events_io->get_event_line();
                                    if (have_initial_state)
                                    {
                                        if (update_counter >= MAX_EVENT_BUNDLE)
                                        {
                                            if (timer_available)
                                            {
                                                cond_display_update(timer_available, timer_armed);
                                            }
                                            else
                                            {
                                                display->status_display();
                                            }
                                            update_counter = 0;
                                        }
                                        else
                                        {
                                            ++update_counter;
                                        }
                                    }
                                }
                            }
                            else
                            {
                                std::string debug_info("EventsIo::get_event_line() returned nullptr");
                                throw EventMessageException(nullptr, &debug_info, nullptr);
                            }
                            if (have_initial_state)
                            {
                                if (timer_available)
                                {
                                    cond_display_update(timer_available, timer_armed);
                                }
                                else
                                {
                                    display->status_display();
                                }
                            }
                        }
                        break;
                    case EventsIo::event::SIGNAL:
                        {
                            int signal_id = events_io->get_signal();
                            switch (signal_id)
                            {
                                case SIGHUP:
                                    // fall-through
                                case SIGINT:
                                    // fall-through
                                case SIGTERM:
                                    // terminate main loop
                                    fin_action = DrbdMon::finish_action::TERMINATE;
                                    shutdown = true;
                                    break;
                                case SIGWINCH:
                                    if (term_size->probe_terminal_size())
                                    {
                                        display->set_terminal_size(term_size->get_size_x(), term_size->get_size_y());
                                        display->status_display();
                                    }
                                    break;
                                case SIGCHLD:
                                    {
                                        // Throws an EventsSourceException if the currently tracked events source
                                        // process exits, thereby triggering reinitialization
                                        events_source->cleanup_child_processes();
                                    }
                                    break;
                                case SIGUSR1:
                                    {
                                        fin_action = DrbdMon::finish_action::DEBUG_MODE;
                                        shutdown = true;
                                    }
                                    break;
                                case SIGALRM:
                                    {
                                        display->status_display();
                                        update_timestamp(timer_available, timer_armed);
                                        timer_armed = false;
                                    }
                                default:
                                    // Unexpected signals ignored
                                    break;
                            }
                            break;
                        }
                    case EventsIo::event::STDIN:
                        {
                            // Consume input from stdin
                            char c = 0;
                            int read_count = 0;
                            do
                            {
                                errno = 0;
                                read_count = read(STDIN_FILENO, &c, 1);
                            }
                            while (read_count == -1 && errno == EINTR);

                            if (read_count == 1)
                            {
                                if (debug_key)
                                {
                                    if (c == 'd' || c == 'D')
                                    {
                                        fin_action = DrbdMon::finish_action::DEBUG_MODE;
                                        shutdown = true;
                                    }
                                    debug_key = false;
                                }
                                else
                                if (c == HOTKEY_REPAINT)
                                {
                                    display->status_display();
                                }
                                else
                                if (c == HOTKEY_REINIT)
                                {
                                    fin_action = DrbdMon::finish_action::RESTART_IMMED;
                                    shutdown = true;
                                }
                                else
                                if (c == HOTKEY_QUIT)
                                {
                                    fin_action = DrbdMon::finish_action::TERMINATE;
                                    shutdown = true;
                                }
                                else
                                if (c == HOTKEY_VERSION)
                                {
                                    std::string version_info = PROGRAM_NAME + " v" + VERSION +
                                        " (" + GITHASH + ")";
                                    log.add_entry(MessageLog::log_level::INFO, version_info);
                                    display->status_display();
                                }
                                else
                                if (c == DEBUG_SEQ_PFX)
                                {
                                    debug_key = true;
                                }
                                else
                                {
                                    display->key_pressed(c);
                                }
                            }
                        }
                        break;
                    case EventsIo::event::NONE:
                        // Not supposed to happen
                        // fall-through
                    default:
                        log.add_entry(
                            MessageLog::log_level::ALERT,
                            "Internal error: Unexpected event type returned by EventsIo"
                        );
                        break;
                }
            }
        }
        catch (std::ios_base::failure&)
        {
            log.add_entry(
                MessageLog::log_level::ALERT,
                "The connection to the DRBD events source failed due to an I/O error"
            );
            fin_action = DrbdMon::finish_action::RESTART_DELAYED;
            fail_data = DrbdMon::fail_info::EVENTS_IO;
        }
        catch (EventsSourceException& src_exc)
        {
            log.add_entry(
                MessageLog::log_level::ALERT,
                "The external process that provides DRBD events exited"
            );
            const std::string* const error_msg = src_exc.get_error_msg();
            if (error_msg != nullptr)
            {
                log.add_entry(
                    MessageLog::log_level::ALERT,
                    *error_msg
                );
            }
            const std::string* const debug_info = src_exc.get_debug_info();
            if (debug_info != nullptr)
            {
                debug_log.add_entry(
                    MessageLog::log_level::ALERT,
                    *debug_info
                );
            }
            fin_action = DrbdMon::finish_action::RESTART_DELAYED;
            fail_data = DrbdMon::fail_info::EVENTS_SOURCE;
        }
        catch (EventsIoException& io_exc)
        {
            log.add_entry(
                MessageLog::log_level::ALERT,
                "DRBD events source I/O error"
            );
            const std::string* const error_msg = io_exc.get_error_msg();
            if (error_msg != nullptr)
            {
                log.add_entry(
                    MessageLog::log_level::ALERT,
                    *error_msg
                );
            }
            const std::string* const debug_info = io_exc.get_debug_info();
            if (debug_info != nullptr)
            {
                debug_log.add_entry(
                    MessageLog::log_level::ALERT,
                    *debug_info
                );
            }
            fin_action = DrbdMon::finish_action::RESTART_DELAYED;
            fail_data = DrbdMon::fail_info::EVENTS_IO;
        }
        catch (EventException& event_exc)
        {
            const std::string* const error_msg = event_exc.get_error_msg();
            const std::string* const debug_info = event_exc.get_debug_info();
            const std::string* const event_line = event_exc.get_event_line();

            // Log an error alert
            if (error_msg != nullptr)
            {
                log.add_entry(
                    MessageLog::log_level::ALERT,
                    *error_msg
                );
            }
            else
            {
                log.add_entry(
                    MessageLog::log_level::ALERT,
                    "Received an unparsable DRBD event line from the DRBD events source"
                );
            }

            // Log any available debug information
            if (debug_info != nullptr)
            {
                debug_log.add_entry(
                    MessageLog::log_level::ALERT,
                    *debug_info
                );
            }
            if (event_line != nullptr)
            {
                debug_log.add_entry(
                    MessageLog::log_level::INFO,
                    *event_line
                );
            }

            fin_action = DrbdMon::finish_action::RESTART_DELAYED;
            fail_data = DrbdMon::fail_info::GENERIC;
        }
        catch (ConfigurationException&)
        {
            // A ConfigurationException is thrown to abort and exit
            // while configuring options
            // (--help uses this too)
            fin_action = DrbdMon::finish_action::TERMINATE_NO_CLEAR;
        }
    }
    catch (std::bad_alloc&)
    {
        cleanup(event_props.get(), events_io.get());
        throw;
    }

    cleanup(event_props.get(), events_io.get());
}

// @throws std::bad_alloc, EventsMessageException, EventObjectException
void DrbdMon::tokenize_event_message(std::string& event_line, PropsMap& event_props)
{
    try
    {
        StringTokenizer tokens(event_line, TOKEN_DELIMITER);
        if (tokens.has_next())
        {
            std::string event_mode = tokens.next();
            if (tokens.has_next())
            {
                std::string event_type = tokens.next();
                parse_event_props(tokens, event_props, event_line);

                process_event_message(event_mode, event_type, event_props, event_line);
            }
        }
    }
    catch (EventMessageException& event_exc)
    {
        // Update information for the debug log
        if (event_exc.get_debug_info() == nullptr)
        {
            std::string debug_info("Event line caused an EventMessageException (no debug information available)");
            event_exc.set_debug_info(&debug_info);
        }
        if (event_exc.get_event_line() == nullptr)
        {
            event_exc.set_event_line(&event_line);
        }
        throw;
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::process_event_message(
    std::string& event_mode,
    std::string& event_type,
    PropsMap& event_props,
    std::string& event_line
)
{
    bool is_exists_event = event_mode == MODE_EXISTS;
    if (is_exists_event || event_mode == MODE_CREATE)
    {
        if (is_exists_event)
        {
            if (have_initial_state)
            {
                // Received an 'exists' event after the 'exists -' line that finishes current state reporting
                std::string error_msg("The events source generated an out-of-sync 'exists' event");
                std::string debug_info("'exists' event was received out-of-sync (after 'exists -')");
                throw EventMessageException(&error_msg, &debug_info, &event_line);
            }
        }
        else
        {
            if (!have_initial_state)
            {
                // Received a 'create' event before all 'exists' events have been received
                std::string error_msg("The events source generated an out-of-sync 'create' event");
                std::string debug_info("'create' event was received out-of-sync (before 'exists -')");
                throw EventMessageException(&error_msg, &debug_info, &event_line);
            }
        }

        if (event_type == TYPE_CONNECTION)
        {
            create_connection(event_props, event_line);
        }
        else
        if (event_type == TYPE_DEVICE)
        {
            create_device(event_props, event_line);
        }
        else
        if (event_type == TYPE_PEER_DEVICE)
        {
            create_peer_device(event_props, event_line);
        }
        else
        if (event_type == TYPE_RESOURCE)
        {
            create_resource(event_props, event_line);
        }
        else
        if (event_type == TYPE_SEPARATOR && event_mode == MODE_EXISTS)
        {
            // "exists -" line from drbdsetup
            // Report recovering from errors that triggered reinitialization
            // of the DrbdMon instance

            switch (fail_data)
            {
                case DrbdMon::fail_info::NONE:
                    // no-op
                    break;
                case DrbdMon::fail_info::OUT_OF_MEMORY:
                    log.add_entry(
                        MessageLog::log_level::INFO,
                        "Status tracking reestablished after out-of-memory condition"
                    );
                    break;
                case DrbdMon::fail_info::EVENTS_IO:
                    log.add_entry(
                        MessageLog::log_level::INFO,
                        "Events source I/O reestablished"
                    );
                    break;
                case DrbdMon::fail_info::EVENTS_SOURCE:
                    log.add_entry(
                        MessageLog::log_level::INFO,
                        "Events source process respawned"
                    );
                    break;
                case DrbdMon::fail_info::GENERIC:
                    // fall-through
                default:
                    log.add_entry(
                        MessageLog::log_level::INFO,
                        "Status tracking reestablished"
                    );
                    break;
            }

            // Indicate that the initial state is available now
            // (This can be used to disable display updates until an initial state is available)
            have_initial_state = true;
            display->status_display();

            // In case that multiple "exists -" lines are received,
            // which is actually not supposed to happen, avoid spamming
            // the message log
            fail_data = fail_info::NONE;
        }
    }
    else
    if (event_mode == MODE_CHANGE)
    {
        if (!have_initial_state)
        {
            // Received a 'change' event before all 'exists' events have been received
            // This is a known bug in some versions of drbdsetup
            std::string error_msg("The events source generated an out-of-sync 'change' event");
            std::string debug_info("'change' event was received out-of-sync (before 'exists -')");
            throw EventMessageException(&error_msg, &debug_info, &event_line);
        }

        if (event_type == TYPE_CONNECTION)
        {
            update_connection(event_props, event_line);
        }
        else
        if (event_type == TYPE_DEVICE)
        {
            update_device(event_props, event_line);
        }
        else
        if (event_type == TYPE_PEER_DEVICE)
        {
            update_peer_device(event_props, event_line);
        }
        else
        if (event_type == TYPE_RESOURCE)
        {
            update_resource(event_props, event_line);
        }
        // unknown object types are skipped
    }
    else
    if (event_mode == MODE_DESTROY)
    {
        if (!have_initial_state)
        {
            // Received a 'destroy' event before all 'exists' events have been received
            std::string error_msg("The events source generated an out-of-sync 'destroy' event");
            std::string debug_info("'destroy' event was received out-of-sync (before 'exists -')");
            throw EventMessageException(&error_msg, &debug_info, &event_line);
        }

        if (event_type == TYPE_CONNECTION)
        {
            destroy_connection(event_props, event_line);
        }
        else
        if (event_type == TYPE_DEVICE)
        {
            destroy_device(event_props, event_line);
        }
        else
        if (event_type == TYPE_PEER_DEVICE)
        {
            destroy_peer_device(event_props, event_line);
        }
        else
        if (event_type == TYPE_RESOURCE)
        {
            destroy_resource(event_props, event_line);
        }
        // unknown object types are skipped
    }
    // unknown message modes are skipped
}

/**
 * Returns the action requested to be taken upon return from this class' run() method.
 * This method should be called only after run() has returned.
 */
DrbdMon::finish_action DrbdMon::get_fin_action() const
{
    return fin_action;
}

/**
 *  Loads the event_props map with the properties contained in tokens
 *
 * @param: tokens StringTokenizer that returns the 'key:value' tokens from a 'drbdsetup event2' line
 * @param: event_props Property map to load the key == value mappings into
 */
// @throws std::bad_alloc, EventMessageException
void DrbdMon::parse_event_props(StringTokenizer& tokens, PropsMap& event_props, std::string& event_line)
{
    while (tokens.has_next())
    {
        std::string prop_entry = tokens.next();

        size_t split_index = prop_entry.find(":");
        if (split_index != std::string::npos)
        {
            std::unique_ptr<std::string> key(new std::string(prop_entry, 0, split_index));
            ++split_index;
            std::unique_ptr<std::string> value(
                new std::string(prop_entry, split_index, prop_entry.length() - split_index)
            );
            try
            {
                event_props.insert(key.get(), value.get());
                static_cast<void> (key.release());
                static_cast<void> (value.release());
            }
            catch (dsaext::DuplicateInsertException& dup_exc)
            {
                // Duplicate key, malformed event line
                std::string error_msg("Received an events line with a duplicate key");

                std::string debug_info("Duplicate key ");
                debug_info += *(key.get());
                debug_info += "on event line";

                throw EventMessageException(&error_msg, &debug_info, &event_line);
            }
        }
    }
}

/**
 * Clears the event_props map and frees all its entries
 */
void DrbdMon::clear_event_props(PropsMap& event_props)
{
    PropsMap::NodesIterator clear_iter(event_props);
    while (clear_iter.has_next())
    {
        PropsMap::Node* node = clear_iter.next();
        delete node->get_key();
        delete node->get_value();
    }
    event_props.clear();
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::create_connection(PropsMap& event_props, const std::string& event_line)
{
    try
    {
        DrbdResource& res = get_resource(event_props, event_line);

        std::unique_ptr<DrbdConnection> conn(DrbdConnection::new_from_props(event_props));
        conn->update(event_props);
        static_cast<void> (conn->update_state_flags());
        res.add_connection(conn.get());
        StateFlags::state res_last_state = res.get_state();
        StateFlags::state res_new_state = res.child_state_flags_changed();
        problem_counter_update(res_last_state, res_new_state);
        static_cast<void> (conn.release());
    }
    catch (dsaext::DuplicateInsertException& dup_exc)
    {
        std::string error_msg("Invalid DRBD event: Duplicate connection creation");
        std::string debug_info("Duplicate connection creation");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::create_device(PropsMap& event_props, const std::string& event_line)
{
    try
    {
        DrbdResource& res = get_resource(event_props, event_line);

        std::unique_ptr<DrbdVolume> vol(DrbdVolume::new_from_props(event_props));
        vol->update(event_props);
        static_cast<void> (vol->update_state_flags());
        res.add_volume(vol.get());
        StateFlags::state res_last_state = res.get_state();
        StateFlags::state res_new_state = res.child_state_flags_changed();
        problem_counter_update(res_last_state, res_new_state);
        static_cast<void> (vol.release());
    }
    catch (dsaext::DuplicateInsertException& dup_exc)
    {
        std::string error_msg("Invalid DRBD event: Duplicate volume creation");
        std::string debug_info("Duplicate device (volume) creation");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::create_peer_device(PropsMap& event_props, const std::string& event_line)
{
    try
    {
        DrbdResource& res = get_resource(event_props, event_line);
        DrbdConnection& conn = get_connection(res, event_props, event_line);

        std::unique_ptr<DrbdVolume> vol(DrbdVolume::new_from_props(event_props));
        vol->update(event_props);
        vol->set_connection(&conn);
        static_cast<void> (vol->update_state_flags());
        conn.add_volume(vol.get());
        static_cast<void> (conn.child_state_flags_changed());
        StateFlags::state res_last_state = res.get_state();
        StateFlags::state res_new_state = res.child_state_flags_changed();
        problem_counter_update(res_last_state, res_new_state);
        static_cast<void> (vol.release());
    }
    catch (dsaext::DuplicateInsertException& dup_exc)
    {
        std::string error_msg("Invalid DRBD event: Duplicate peer volume creation");
        std::string debug_info("Duplicate peer device (peer volume) creation");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
}

// @throws std::bad_alloc, EventMessageException
void DrbdMon::create_resource(PropsMap& event_props, const std::string& event_line)
{
    try
    {
        std::unique_ptr<DrbdResource> res(DrbdResource::new_from_props(event_props));
        res->update(event_props);
        static_cast<void> (res->update_state_flags());
        std::unique_ptr<std::string> res_name(new std::string(res->get_name()));
        resources_map->insert(res_name.get(), res.get());
        if (res->has_mark_state() && problem_count < ~static_cast<uint64_t> (0))
        {
            ++problem_count;
        }
        static_cast<void> (res.release());
        static_cast<void> (res_name.release());
    }
    catch (dsaext::DuplicateInsertException& dup_exc)
    {
        std::string error_msg("Invalid DRBD event: Duplicate resource creation");
        std::string debug_info("Duplicate resource creation");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::update_connection(PropsMap& event_props, const std::string& event_line)
{
    DrbdResource& res = get_resource(event_props, event_line);
    DrbdConnection& conn = get_connection(res, event_props, event_line);
    conn.update(event_props);

    // Adjust connection state flags
    StateFlags::state conn_last_state = conn.get_state();
    StateFlags::state conn_new_state = conn.update_state_flags();
    if (conn_last_state != conn_new_state &&
        (conn_last_state == StateFlags::state::NORM || conn_new_state == StateFlags::state::NORM))
    {
        // Connection state flags changed, adjust resource state flags
        StateFlags::state res_last_state = res.get_state();
        StateFlags::state res_new_state = res.child_state_flags_changed();
        problem_counter_update(res_last_state, res_new_state);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::update_device(PropsMap& event_props, const std::string& event_line)
{
    DrbdResource& res = get_resource(event_props, event_line);
    DrbdVolume& vol = get_device(dynamic_cast<VolumesContainer&> (res), event_props, event_line);
    vol.update(event_props);

    // Adjust volume state flags
    static_cast<void> (vol.update_state_flags());

    // Volume state flags changed, adjust resource state flags
    StateFlags::state res_last_state = res.get_state();
    StateFlags::state res_new_state = res.child_state_flags_changed();
    problem_counter_update(res_last_state, res_new_state);
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::update_peer_device(PropsMap& event_props, const std::string& event_line)
{
    DrbdResource& res = get_resource(event_props, event_line);
    DrbdConnection& conn = get_connection(res, event_props, event_line);
    DrbdVolume& vol = get_device(dynamic_cast<VolumesContainer&> (conn), event_props, event_line);
    vol.update(event_props);

    // Adjust volume state flags
    StateFlags::state vol_last_state = vol.get_state();
    StateFlags::state vol_new_state = vol.update_state_flags();
    if (vol_last_state != vol_new_state &&
        (vol_last_state == StateFlags::state::NORM || vol_new_state == StateFlags::state::NORM))
    {
        // Volume state flags changed, adjust connection state flags
        StateFlags::state conn_last_state = conn.get_state();
        StateFlags::state conn_new_state = conn.child_state_flags_changed();
        if (conn_last_state != conn_new_state &&
            (conn_last_state == StateFlags::state::NORM || conn_new_state == StateFlags::state::NORM))
        {
            // Connection state flags changed, adjust resource state flags
            StateFlags::state res_last_state = res.get_state();
            StateFlags::state res_new_state = res.child_state_flags_changed();
            problem_counter_update(res_last_state, res_new_state);
        }
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::update_resource(PropsMap& event_props, const std::string& event_line)
{
    DrbdResource& res = get_resource(event_props, event_line);
    res.update(event_props);

    StateFlags::state res_last_state = res.get_state();
    StateFlags::state res_new_state = res.update_state_flags();
    problem_counter_update(res_last_state, res_new_state);
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::destroy_connection(PropsMap& event_props, const std::string& event_line)
{
    DrbdResource& res = get_resource(event_props, event_line);
    bool conn_marked = false;
    {
        DrbdConnection& conn = get_connection(res, event_props, event_line);
        conn_marked = conn.has_mark_state();
        res.remove_connection(conn.get_name());
    }

    if (conn_marked)
    {
        StateFlags::state res_last_state = res.get_state();
        StateFlags::state res_new_state = res.child_state_flags_changed();
        problem_counter_update(res_last_state, res_new_state);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::destroy_device(PropsMap& event_props, const std::string& event_line)
{
    DrbdResource& res = get_resource(event_props, event_line);
    bool vol_marked = false;
    {
        DrbdVolume& vol = get_device(dynamic_cast<VolumesContainer&> (res), event_props, event_line);
        vol_marked = vol.has_mark_state();
        res.remove_volume(vol.get_volume_nr());
    }

    if (vol_marked)
    {
        StateFlags::state res_last_state = res.get_state();
        StateFlags::state res_new_state = res.child_state_flags_changed();
        problem_counter_update(res_last_state, res_new_state);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::destroy_peer_device(PropsMap& event_props, const std::string& event_line)
{
    DrbdResource& res = get_resource(event_props, event_line);
    DrbdConnection& conn = get_connection(res, event_props, event_line);
    bool peer_vol_marked = false;
    {
        // May report non-existing volume if required
        DrbdVolume& peer_vol =  get_device(dynamic_cast<VolumesContainer&> (conn), event_props, event_line);
        peer_vol_marked = peer_vol.has_mark_state();
    }

    std::string* vol_nr_str = event_props.get(&DrbdVolume::PROP_KEY_VOL_NR);
    if (vol_nr_str != nullptr)
    {
        try
        {
            uint16_t vol_nr = DrbdVolume::parse_volume_nr(*vol_nr_str);
            conn.remove_volume(vol_nr);
            if (peer_vol_marked)
            {
                static_cast<void> (conn.child_state_flags_changed());
                StateFlags::state res_last_state = res.get_state();
                StateFlags::state res_new_state = res.child_state_flags_changed();
                problem_counter_update(res_last_state, res_new_state);
            }
        }
        catch (dsaext::NumberFormatException&)
        {
            std::string error_msg("Invalid DRBD event: Unparsable volume number");
            std::string debug_info("Unparsable volume number");
            throw EventMessageException(&error_msg, &debug_info, &event_line);
        }
    }
    else
    {
        std::string error_msg("Invalid DRBD event: Missing volume number");
        std::string debug_info("Missing volume number");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::destroy_resource(PropsMap& event_props, const std::string& event_line)
{
    std::string* res_name = event_props.get(&DrbdResource::PROP_KEY_RES_NAME);
    if (res_name != nullptr)
    {
        ResourcesMap::Node* node = resources_map->get_node(res_name);
        if (node != nullptr)
        {
            DrbdResource* res = node->get_value();
            if (res->has_mark_state() && problem_count > 0)
            {
                --problem_count;
            }
            delete node->get_key();
            delete node->get_value();
            resources_map->remove_node(node);
        }
        else
        {
            std::string error_msg("DRBD event line references a non-existent resource");
            std::string debug_info("DRBD event line references a non-existent resource");
            throw EventObjectException(&error_msg, &debug_info, &event_line);
        }
    }
    else
    {
        std::string error_msg("Received DRBD event line does not contain a resource name");
        std::string debug_info("DRBD event line contains no resource name");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
DrbdConnection& DrbdMon::get_connection(DrbdResource& res, PropsMap& event_props, const std::string& event_line)
{
    DrbdConnection* conn {nullptr};
    std::string* conn_name = event_props.get(&DrbdConnection::PROP_KEY_CONN_NAME);
    if (conn_name != nullptr)
    {
        conn = res.get_connection(*conn_name);
    }
    else
    {
        std::string error_msg("Invalid DRBD event: Missing connection information");
        std::string debug_info("Missing 'connection' field");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
    if (conn == nullptr)
    {
        std::string error_msg("Non-existent connection referenced by the DRBD events source");
        std::string debug_info("DRBD event line references a non-existent connection");
        throw EventObjectException(&error_msg, &debug_info, &event_line);
    }
    return *conn;
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
DrbdVolume& DrbdMon::get_device(VolumesContainer& vol_con, PropsMap& event_props, const std::string& event_line)
{
    DrbdVolume* vol {nullptr};
    std::string* vol_nr_str = event_props.get(&DrbdVolume::PROP_KEY_VOL_NR);
    if (vol_nr_str != nullptr)
    {
        try
        {
            uint16_t vol_nr = DrbdVolume::parse_volume_nr(*vol_nr_str);
            vol = vol_con.get_volume(vol_nr);
        }
        catch (dsaext::NumberFormatException&)
        {
            std::string error_msg("Invalid DRBD event: Invalid volume number");
            std::string debug_info("Unparsable volume number");
            throw EventMessageException(&error_msg, &debug_info, &event_line);
        }
    }
    else
    {
        std::string error_msg("Invalid DRBD event: Missing volume number");
        std::string debug_info("Missing volume number");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
    if (vol == nullptr)
    {
        std::string error_msg("Non-existent volume id referenced by the DRBD events source");
        std::string debug_info("DRBD event line references a non-existent volume id");
        throw EventObjectException(&error_msg, &debug_info, &event_line);
    }
    return *vol;
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
DrbdResource& DrbdMon::get_resource(PropsMap& event_props, const std::string& event_line)
{
    DrbdResource* res {nullptr};
    std::string* res_name = event_props.get(&DrbdResource::PROP_KEY_RES_NAME);
    if (res_name != nullptr)
    {
        res = resources_map->get(res_name);
    }
    else
    {
        std::string error_msg("Received DRBD event line does not contain a resource name");
        std::string debug_info("DRBD event line contains no resource name");
        throw EventMessageException(&error_msg, &debug_info, &event_line);
    }
    if (res == nullptr)
    {
        std::string error_msg("Non-existent resource referenced by the DRBD events source");
        std::string debug_info("DRBD event line references a non-existent resource");
        throw EventObjectException(&error_msg, &debug_info, &event_line);
    }
    return *res;
}

// @throws std::bad_alloc
void DrbdMon::setup_hotkeys_info()
{
    hotkeys_info->append(&HOTKEY_QUIT, &DESC_QUIT);
    hotkeys_info->append(&HOTKEY_REPAINT, &DESC_REPAINT);
}

// Frees resources
// @throws std::bad_alloc
void DrbdMon::cleanup(
    PropsMap*               event_props,
    EventsIo*               events_io
)
{
    if (event_props != nullptr)
    {
        clear_event_props(*event_props);
    }

    if (events_io != nullptr)
    {
        try
        {
            events_io->restore_terminal();
        }
        catch (EventsIoException&)
        {
            log.add_entry(
                MessageLog::log_level::WARN,
                "Restoring the terminal mode failed"
            );
        }
    }
}

// @throws std::bad_alloc
void DrbdMon::configure_options()
{
    options = std::unique_ptr<OptionsMap>(new OptionsMap(&comparators::compare_string));

    try
    {
        Configurator& collector = dynamic_cast<Configurator&> (*this);
        size_t slot = 0;
        while (configurables[slot] != nullptr)
        {
            configurables[slot]->announce_options(collector);
            ++slot;
        }

        {
            std::unique_ptr<Args> arg_list(new Args(arg_count, arg_values));
            while (arg_list->has_next())
            {
                std::string option_key(arg_list->next());
                if (option_key.find("--") == 0)
                {
                    option_key = option_key.substr(2, std::string::npos);
                    option_entry* entry = options->get(&option_key);
                    if (entry != nullptr)
                    {
                        if (entry->option.is_flag)
                        {
                            entry->owner.set_flag(option_key);
                        }
                        else
                        {
                            char* arg = arg_list->next();
                            if (arg != nullptr)
                            {
                                std::string option_value(arg);
                                entry->owner.set_option(option_key, option_value);
                            }
                            else
                            {
                                std::string error_message = "Missing value for argument key '--";
                                error_message += option_key;
                                error_message += "'";
                                log.add_entry(MessageLog::log_level::ALERT, error_message);
                            }
                        }
                    }
                    else
                    {
                        std::string error_message = "Unknown argument key '--";
                        error_message += option_key;
                        error_message += "'";
                        log.add_entry(MessageLog::log_level::ALERT, error_message);
                    }
                }
                else
                {
                    std::string error_message = "Malformed argument '";
                    error_message += option_key;
                    error_message += "' ignored";
                    log.add_entry(MessageLog::log_level::ALERT, error_message);
                }
            }
        }
    }
    catch (ConfigurationException&)
    {
        options_cleanup();
        throw;
    }
    catch (std::bad_alloc&)
    {
        options_cleanup();
        throw;
    }

    options_cleanup();
}

void DrbdMon::options_cleanup() noexcept
{
    if (options != nullptr)
    {
        OptionsMap::ValuesIterator cleanup_iter(*options);
        while (cleanup_iter.has_next())
        {
            delete cleanup_iter.next();
        }
    }
}

// @throws std::bad_alloc
void DrbdMon::add_config_option(Configurable& owner, const ConfigOption& conf_option)
{
    if (options != nullptr)
    {
        try
        {
            std::unique_ptr<DrbdMon::option_entry> entry(
                new DrbdMon::option_entry { owner, conf_option }
            );
            options->insert(&conf_option.key, entry.get());
            static_cast<void> (entry.release());
        }
        catch (dsaext::DuplicateInsertException&)
        {
            std::string error_message = "Duplicate configuration option '--";
            error_message += conf_option.key;
            error_message += "'";
            log.add_entry(MessageLog::log_level::ALERT, error_message);
        }
    }
}

// @throws std::bad_alloc
void DrbdMon::announce_options(Configurator& collector)
{
    Configurable& owner = dynamic_cast<Configurable&> (*this);
    collector.add_config_option(owner, OPT_HELP);
    collector.add_config_option(owner, OPT_VERSION);
    collector.add_config_option(owner, OPT_FREQ_LMT);
}

void DrbdMon::options_help() noexcept
{
    std::cerr.clear();
    std::cerr << DrbdMon::PROGRAM_NAME << " configuration options:\n";
    std::cerr << "  --version        Display version information\n";
    std::cerr << "  --help           Display help\n";
    std::cerr << "  --freqlmt <interval>     Set a frequency limit for display updates\n";
    std::cerr << "    <interval>             Minimum delay between display updates [integer]\n";
    std::cerr << "    Supported unit suffixes: s (seconds), ms (milliseconds)\n";
    std::cerr << "    Default unit: s (seconds)\n";
    std::cerr << '\n';
    std::cerr << "Environment variables:\n";
    std::cerr << "  " << std::left << std::setw(25) << ENV_COLOR_MODE << " Select color mode\n";
    std::cerr << "    " << std::left << std::setw(23) << COLOR_MODE_EXTENDED << " Extended color mode (256 colors)\n";
    std::cerr << "    " << std::left << std::setw(23) << COLOR_MODE_BASIC << " Basic color mode (16 colors)\n";
    std::cerr << std::endl;
}

// @throws std::bad_alloc, ConfigurationException
void DrbdMon::set_flag(std::string& key)
{
    if (key == OPT_HELP.key)
    {
        size_t slot = 0;
        while (configurables[slot] != nullptr)
        {
            configurables[slot]->options_help();
            ++slot;
        }
        throw ConfigurationException();
    }
    else
    if (key == OPT_VERSION.key)
    {
        std::cout << PROGRAM_NAME << " v" << VERSION << " (" << GITHASH << ")" << std::endl;
        throw ConfigurationException();
    }
}

// @throws std::bad_alloc, ConfigurationException
void DrbdMon::set_option(std::string& key, std::string& value)
{
    if (key == OPT_FREQ_LMT.key)
    {
        uint16_t factor = 1000;
        std::string number;
        if (string_ends_with(value, UNIT_SFX_MILLISECONDS))
        {
            factor = 1;
            number = value.substr(0, value.length() - UNIT_SFX_MILLISECONDS.length());
        }
        else
        if (string_ends_with(value, UNIT_SFX_SECONDS))
        {
            number = value.substr(0, value.length() - UNIT_SFX_SECONDS.length());
        }
        else
        {
            number = value;
        }

        try
        {
            uint16_t interval = dsaext::parse_unsigned_int16(number);
            if ((factor > 1 && MAX_INTERVAL / factor < interval) || interval > MAX_INTERVAL)
            {
                std::string error_message("Interval ");
                error_message += value;
                error_message += " for configuration option ";
                error_message += OPT_FREQ_LMT.key;
                error_message += " is out of range";
                log.add_entry(MessageLog::log_level::ALERT, error_message);
                throw ConfigurationException();
            }

            // Valid user configured interval, or frequency limiting disabled by the user,
            // do not configure the default interval
            use_dflt_freq_lmt = false;

            interval *= factor;
            if (interval > 0 && interval_timer_mgr == nullptr)
            {
                interval_timer_mgr = std::unique_ptr<IntervalTimer>(
                    new IntervalTimer(log, interval)
                );
            }
        }
        catch (dsaext::NumberFormatException&)
        {
            std::string error_message("Invalid interval ");
            error_message += value;
            error_message += " for configuration option ";
            error_message += OPT_FREQ_LMT.key;
            log.add_entry(MessageLog::log_level::ALERT, error_message);
            throw ConfigurationException();
        }
    }
}

uint64_t DrbdMon::get_problem_count() const noexcept
{
    return problem_count;
}

void DrbdMon::problem_counter_update(StateFlags::state res_last_state, StateFlags::state res_new_state) noexcept
{
    if (res_last_state == StateFlags::state::NORM && res_new_state != StateFlags::state::NORM &&
        problem_count < ~static_cast<uint64_t> (0))
    {
        ++problem_count;
    }
    else
    if (res_last_state != StateFlags::state::NORM && res_new_state == StateFlags::state::NORM &&
        problem_count > 0)
    {
        --problem_count;
    }
}

void DrbdMon::disable_interval_timer(bool& timer_available, bool& timer_armed) noexcept
{
    // If the timer failed, disable limited frequency display updates
    // and update the display immediately
    timer_available = false;
    timer_armed = false;
    interval_timer_mgr = nullptr;
    log.add_entry(
        MessageLog::log_level::ALERT,
        "System timer failed, display update frequency limiting has been disabled"
    );
}

// @throws TimerException
inline bool DrbdMon::is_interval_exceeded()
{
    if (clock_gettime(CLOCK_MONOTONIC, &cur_timestamp) != 0)
    {
        throw TimerException();
    }
    time_t secs_diff = (cur_timestamp.tv_sec - prev_timestamp.tv_sec);
    long nanosecs_diff = (cur_timestamp.tv_nsec - prev_timestamp.tv_nsec);

    struct timespec interval = interval_timer_mgr->get_interval();
    bool exceeded = (
        secs_diff > interval.tv_sec ||
        (secs_diff == interval.tv_sec && nanosecs_diff >= interval.tv_nsec)
    );
    return exceeded;
}

inline void DrbdMon::cond_display_update(bool& timer_available, bool& timer_armed) noexcept
{
    try
    {
        if (!timer_armed)
        {
            if (is_interval_exceeded())
            {
                display->status_display();
                prev_timestamp = cur_timestamp;
            }
            else
            {
                interval_timer_mgr->arm_timer();
                timer_armed = true;
            }
        }
    }
    catch (TimerException&)
    {
        disable_interval_timer(timer_available, timer_armed);
        display->status_display();
    }
}

inline void DrbdMon::update_timestamp(bool& timer_available, bool& timer_armed) noexcept
{
    if (clock_gettime(CLOCK_MONOTONIC, &prev_timestamp) != 0)
    {
        disable_interval_timer(timer_available, timer_armed);
    }
}

static bool string_ends_with(const std::string& text, const std::string& suffix)
{
    const size_t pos = text.rfind(suffix);
    return (pos != std::string::npos && (pos == text.length() - suffix.length()));
}
