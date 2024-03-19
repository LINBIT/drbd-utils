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
}

#include <map_types.h>
// https://github.com/raltnoeder/cppdsaext
#include <dsaext.h>
#include <integerparse.h>
#include <utils.h>
#include <comparators.h>
#include <DrbdMon.h>
#include <DrbdMonConsts.h>
#include <CoreIo.h>
#include <ConfigOption.h>
#include <Args.h>
#include <bounds.h>
#include <terminal/DisplayController.h>
#include <terminal/KeyCodes.h>
#include <subprocess/SubProcessObserver.h>
#include <MessageLogNotification.h>
#include <SubProcessNotification.h>
#include <EventsIoWakeup.h>
#include <exceptions.h>

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
const std::string DrbdMon::MODE_RENAME  = "rename";
const std::string DrbdMon::MODE_DESTROY = "destroy";

const std::string DrbdMon::TYPE_RESOURCE    = "resource";
const std::string DrbdMon::TYPE_CONNECTION  = "connection";
const std::string DrbdMon::TYPE_DEVICE      = "device";
const std::string DrbdMon::TYPE_PEER_DEVICE = "peer-device";
const std::string DrbdMon::TYPE_SEPARATOR   = "-";

static bool string_ends_with(const std::string& text, const std::string& suffix);

// @throws std::bad_alloc
DrbdMon::DrbdMon(
    int                         argc,
    char*                       argv[],
    MonitorEnvironment&         mon_env_ref
):
    mon_env(mon_env_ref),
    arg_count(argc),
    arg_values(argv),
    log(*(mon_env_ref.log)),
    debug_log(*(mon_env_ref.debug_log)),
    config(*(mon_env_ref.config))
{
    mon_env.fin_action = DrbdMonCore::finish_action::RESTART_IMMED;
    rsc_dir_mgr = std::unique_ptr<ResourceDirectory>(
        new ResourceDirectory(*(mon_env_ref.log), *(mon_env_ref.debug_log))
    );
    rsc_dir = rsc_dir_mgr.get();
}

DrbdMon::~DrbdMon() noexcept
{
}

SystemApi& DrbdMon::get_system_api() const noexcept
{
    return *(mon_env.sys_api);
}

// @throws std::bad_alloc
void DrbdMon::run()
{
    std::unique_ptr<PropsMap>               event_props;
    std::unique_ptr<EventsSourceSpawner>    events_source;
    std::unique_ptr<EventsIo>               events_io;
    std::unique_ptr<MessageLogNotification> msg_log_notifier;
    std::unique_ptr<SubProcessNotification> sub_proc_notifier;
    std::unique_ptr<GenericDisplay>         display;

    try
    {
        try
        {
            event_props   = std::unique_ptr<PropsMap>(new PropsMap(&comparators::compare_string));

            events_source = std::unique_ptr<EventsSourceSpawner>(new EventsSourceSpawner(log));

            events_source->spawn_source();
            events_io = std::unique_ptr<EventsIo>(
                new EventsIo(events_source->get_events_out_fd(), events_source->get_events_err_fd())
            );

            msg_log_notifier = std::unique_ptr<MessageLogNotification>(
                new MessageLogNotification(log, dynamic_cast<EventsIoWakeup*> (events_io.get()))
            );

            sub_proc_notifier = std::unique_ptr<SubProcessNotification>(
                new SubProcessNotification(dynamic_cast<EventsIoWakeup*> (events_io.get()))
            );

            // Interval timer initialization
            // Must be constructed before configuring the Configurable instances
            interval_timer_mgr = std::unique_ptr<IntervalTimer>(
                new IntervalTimer(log, bounds<uint16_t>(0, config.dsp_interval, MAX_INTERVAL))
            );
            timer_available = config.dsp_interval != 0;
            timer_armed = false;

            configurables = std::unique_ptr<Configurable*[]>(new Configurable*[3]);
            configurables[0] = dynamic_cast<Configurable*> (this);
            // configurables[1] = dynamic_cast<Configurable*> (display_impl);
            configurables[1] = nullptr;
            configurables[2] = nullptr;
            configure_options();

            // Cleanup any zombies that might not have been collected,
            // because SIGCHLD is blocked during reinitialization
            events_source->cleanup_child_processes();

            // Create the display and keep a pointer of the implementation's type for later use
            // as a configurable object
            // Do not add anything that might throw between display_impl allocation and
            // display interface unique_ptr initialization to ensure deallocation of the display
            // object if the current scope is left
            DisplayController* display_impl = nullptr;
            {
                DrbdMonCore* const core_instance    = dynamic_cast<DrbdMonCore*> (this);
                SubProcessObserver* const sub_proc_obs = dynamic_cast<SubProcessObserver*> (sub_proc_notifier.get());
                ResourcesMap& rsc_map               = rsc_dir->get_resources_map();
                ResourcesMap& prb_rsc_map           = rsc_dir->get_problem_resources_map();
                display_impl = new DisplayController(
                    *core_instance,
                    mon_env,
                    *sub_proc_obs,
                    rsc_map,
                    prb_rsc_map
                );
            }

            display = std::unique_ptr<GenericDisplay>(dynamic_cast<GenericDisplay*> (display_impl));
            display->initialize();

            // Show an initial display while reading the initial DRBD status
            display->enter_initial_display();

            while (!shutdown_flag)
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
                                tokenize_event_message(display.get(), *event_line, *event_props);
                                events_io->free_event_line();
                                clear_event_props(*event_props);
                                event_line = events_io->get_event_line();
                                if (have_initial_state)
                                {
                                    if (update_counter >= MAX_EVENT_BUNDLE)
                                    {
                                        if (timer_available)
                                        {
                                            cond_display_update(display.get());
                                        }
                                        else
                                        {
                                            display->notify_drbd_changed();
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
                                cond_display_update(display.get());
                            }
                            else
                            {
                                display->notify_drbd_changed();
                            }
                        }
                        break;
                    }
                    case EventsIo::event::SIGNAL:
                    {
                        CoreIo::signal_type signal_id = events_io->get_signal();
                        switch (signal_id)
                        {
                            case CoreIo::signal_type::SIGNAL_EXIT:
                            {
                                // Terminate main loop
                                mon_env.fin_action = DrbdMonCore::finish_action::TERMINATE;
                                shutdown_flag = true;
                                break;
                            }
                            case CoreIo::signal_type::SIGNAL_WINDOW_SIZE:
                            {
                                display->terminal_size_changed();
                                break;
                            }
                            case CoreIo::signal_type::SIGNAL_CHILD_PROC:
                            {
                                // Throws an EventsSourceException if the currently tracked events source
                                // process exits, thereby triggering reinitialization
                                events_source->cleanup_child_processes();
                                break;
                            }
                            case CoreIo::signal_type::SIGNAL_DEBUG:
                            {
                                mon_env.fin_action = DrbdMonCore::finish_action::DEBUG_MODE;
                                shutdown_flag = true;
                                break;
                            }
                            case CoreIo::signal_type::SIGNAL_TIMER:
                            {
                                display->notify_drbd_changed();
                                update_timestamp();
                                timer_armed = false;
                                break;
                            }
                            case CoreIo::signal_type::SIGNAL_GENERIC:
                                // fall-through
                            default:
                            {
                                // No-op
                                break;
                            }
                        }
                        break;
                    }
                    case EventsIo::event::STDIN:
                    {
                        const uint32_t key = events_io->get_key_pressed();
                        if (key != KeyCodes::NONE)
                        {
                            if (!have_initial_state)
                            {
                                have_initial_state = true;
                                display->exit_initial_display();
                            }

                            if (key != KeyCodes::MOUSE_EVENT)
                            {
                                display->key_pressed(key);
                            }
                            else
                            {
                                MouseEvent& mouse = events_io->get_mouse_action();
                                display->mouse_action(mouse);
                            }
                        }
                        break;
                    }
                    case EventsIo::event::WAKEUP:
                    {
                        // Wakeup signal
                        // Check data shared between threads for changes
                        // TODO: Adapt cond_display_update to handle different kinds of updates
                        const uint64_t notification_mask = sub_proc_notifier->get_notification_mask();
                        if ((notification_mask & SubProcessNotification::MASK_OUT_OF_MEMORY) != 0)
                        {
                            throw std::bad_alloc();
                        }
                        else
                        if ((notification_mask & SubProcessNotification::MASK_SUBPROCESS_CHANGED) != 0)
                        {
                            display->notify_task_queue_changed();
                        }

                        if (msg_log_notifier->query_log_changed())
                        {
                            display->notify_message_log_changed();
                        }

                        break;
                    }
                    case EventsIo::event::NONE:
                    {
                        // Not supposed to happen
                        // fall-through
                    }
                    default:
                    {
                        log.add_entry(
                            MessageLog::log_level::ALERT,
                            "Internal error: Unexpected event type returned by EventsIo"
                        );
                        break;
                    }
                }
            }
        }
        catch (std::ios_base::failure&)
        {
            log.add_entry(
                MessageLog::log_level::ALERT,
                "The connection to the DRBD events source failed due to an I/O error"
            );
            mon_env.fin_action = DrbdMonCore::finish_action::RESTART_DELAYED;
            mon_env.fail_data = DrbdMonCore::fail_info::EVENTS_IO;
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
            mon_env.fin_action = DrbdMonCore::finish_action::RESTART_DELAYED;
            mon_env.fail_data = DrbdMonCore::fail_info::EVENTS_SOURCE;
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
            mon_env.fin_action = DrbdMonCore::finish_action::RESTART_DELAYED;
            mon_env.fail_data = DrbdMonCore::fail_info::EVENTS_IO;
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

            mon_env.fin_action = DrbdMonCore::finish_action::RESTART_DELAYED;
            mon_env.fail_data = DrbdMonCore::fail_info::GENERIC;
        }
        catch (ConfigurationException&)
        {
            // A ConfigurationException is thrown to abort and exit
            // while configuring options
            // (--help uses this too)
            mon_env.fin_action = DrbdMonCore::finish_action::TERMINATE_NO_CLEAR;
        }
    }
    catch (std::bad_alloc&)
    {
        cleanup(event_props.get(), events_io.get());
        throw;
    }

    cleanup(event_props.get(), events_io.get());
}

void DrbdMon::shutdown(const DrbdMonCore::finish_action action) noexcept
{
    mon_env.fin_action = action;
    shutdown_flag = true;
}

// @throws std::bad_alloc, EventsMessageException, EventObjectException
void DrbdMon::tokenize_event_message(GenericDisplay* const display_ptr, std::string& event_line, PropsMap& event_props)
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

                process_event_message(display_ptr, event_mode, event_type, event_props, event_line);
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
    GenericDisplay* const display,
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
            rsc_dir->create_connection(event_props, event_line);
        }
        else
        if (event_type == TYPE_DEVICE)
        {
            rsc_dir->create_device(event_props, event_line);
        }
        else
        if (event_type == TYPE_PEER_DEVICE)
        {
            rsc_dir->create_peer_device(event_props, event_line);
        }
        else
        if (event_type == TYPE_RESOURCE)
        {
            rsc_dir->create_resource(event_props, event_line);
        }
        else
        if (event_type == TYPE_SEPARATOR && event_mode == MODE_EXISTS)
        {
            // "exists -" line from drbdsetup
            // Report recovering from errors that triggered reinitialization
            // of the DrbdMon instance

            switch (mon_env.fail_data)
            {
                case DrbdMonCore::fail_info::NONE:
                    // no-op
                    break;
                case DrbdMonCore::fail_info::OUT_OF_MEMORY:
                    log.add_entry(
                        MessageLog::log_level::INFO,
                        "Status tracking reestablished after out-of-memory condition"
                    );
                    break;
                case DrbdMonCore::fail_info::EVENTS_IO:
                    log.add_entry(
                        MessageLog::log_level::INFO,
                        "Events source I/O reestablished"
                    );
                    break;
                case DrbdMonCore::fail_info::EVENTS_SOURCE:
                    log.add_entry(
                        MessageLog::log_level::INFO,
                        "Events source process respawned"
                    );
                    break;
                case DrbdMonCore::fail_info::GENERIC:
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
            display->exit_initial_display();

            // In case that multiple "exists -" lines are received,
            // which is actually not supposed to happen, avoid spamming
            // the message log
            mon_env.fail_data = fail_info::NONE;
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
            rsc_dir->update_connection(event_props, event_line);
        }
        else
        if (event_type == TYPE_DEVICE)
        {
            rsc_dir->update_device(event_props, event_line);
        }
        else
        if (event_type == TYPE_PEER_DEVICE)
        {
            rsc_dir->update_peer_device(event_props, event_line);
        }
        else
        if (event_type == TYPE_RESOURCE)
        {
            rsc_dir->update_resource(event_props, event_line);
        }
        // unknown object types are skipped
    }
    else
    if (event_mode == MODE_RENAME)
    {
        if (!have_initial_state)
        {
            // Received a 'rename' event before all 'exists' events have been received
            std::string error_msg("The events source generated an out-of-sync 'rename' event");
            std::string debug_info("'rename' event was received out-of-sync (before 'exists -')");
            throw EventMessageException(&error_msg, &debug_info, &event_line);
        }

        if (event_type == TYPE_RESOURCE)
        {
            rsc_dir->rename_resource(event_props, event_line);
        }
        else
        {
            // Received a 'rename' event for something else than a resource
            std::string error_msg("The events source generated a rename event for a non-resource object");
            std::string debug_info("'rename' event for an illegal object type:");
            throw EventMessageException(&error_msg, &debug_info, &event_line);
        }
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
            rsc_dir->destroy_connection(event_props, event_line);
        }
        else
        if (event_type == TYPE_DEVICE)
        {
            rsc_dir->destroy_device(event_props, event_line);
        }
        else
        if (event_type == TYPE_PEER_DEVICE)
        {
            rsc_dir->destroy_peer_device(event_props, event_line);
        }
        else
        if (event_type == TYPE_RESOURCE)
        {
            rsc_dir->destroy_resource(event_props, event_line);
        }
        // unknown object types are skipped
    }
    // unknown message modes are skipped
}

/**
 * Returns the action requested to be taken upon return from this class' run() method.
 * This method should be called only after run() has returned.
 */
DrbdMonCore::finish_action DrbdMon::get_fin_action() const
{
    return mon_env.fin_action;
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
                debug_info += " on event line";

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
    std::cerr << DrbdMonConsts::PROGRAM_NAME << " configuration options:\n";
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
        std::cout << DrbdMonConsts::PROGRAM_NAME << " v" << DrbdMonConsts::UTILS_VERSION <<
                     " (" << DrbdMonConsts::BUILD_HASH << ")" << std::endl;
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
            if (interval > 0)
            {
                interval_timer_mgr->set_interval(interval);
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

uint32_t DrbdMon::get_problem_count() const noexcept
{
    return rsc_dir->get_problem_count();
}

void DrbdMon::notify_config_changed()
{
    // Apply interval timer change
    if (interval_timer_mgr != nullptr)
    {
        const uint16_t dsp_interval = config.dsp_interval;
        timer_available = dsp_interval != 0;
        if (timer_available)
        {
            interval_timer_mgr->set_interval(bounds<uint16_t>(0, dsp_interval, MAX_INTERVAL));
        }
    }
}

void DrbdMon::disable_interval_timer() noexcept
{
    // If the timer failed, disable limited frequency display updates
    // and update the display immediately
    timer_available = false;
    timer_armed = false;
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

inline void DrbdMon::cond_display_update(GenericDisplay* const display_ptr)
{
    try
    {
        if (!timer_armed)
        {
            if (is_interval_exceeded())
            {
                display_ptr->notify_drbd_changed();
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
        disable_interval_timer();
        display_ptr->notify_drbd_changed();
    }
}

inline void DrbdMon::update_timestamp() noexcept
{
    if (clock_gettime(CLOCK_MONOTONIC, &prev_timestamp) != 0)
    {
        disable_interval_timer();
    }
}

static bool string_ends_with(const std::string& text, const std::string& suffix)
{
    const size_t pos = text.rfind(suffix);
    return (pos != std::string::npos && (pos == text.length() - suffix.length()));
}
