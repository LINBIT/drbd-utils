#include <new>
#include <ios>
#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <cstdio>

extern "C"
{
    #include <errno.h>
    #include <config.h>
}

#include <map_types.h>
// https://github.com/raltnoeder/cppdsaext
#include <dsaext.h>
#include <DrbdMon.h>
#include <utils.h>
#include <CompactDisplay.h>
#include <ConfigOption.h>
#include <Args.h>

const std::string DrbdMon::PROGRAM_NAME = "DRBD DrbdMon";
const std::string DrbdMon::VERSION = PACKAGE_VERSION;

const std::string DrbdMon::OPT_HELP_KEY = "help";
const ConfigOption DrbdMon::OPT_HELP(true, OPT_HELP_KEY);

const std::string DrbdMon::TOKEN_DELIMITER = " ";

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
const char DrbdMon::HOTKEY_CLEAR_MSG  = 'c';
const char DrbdMon::HOTKEY_REINIT     = 'R';

const std::string DrbdMon::DESC_QUIT      = "Quit";
const std::string DrbdMon::DESC_REPAINT   = "Repaint";
const std::string DrbdMon::DESC_CLEAR_MSG = "Clear messages";
const std::string DrbdMon::DESC_REINIT    = "Reinitialize";


// @throws std::bad_alloc
DrbdMon::DrbdMon(int argc, char* argv[], MessageLog& log_ref, fail_info& fail_data_ref):
    arg_count(argc),
    arg_values(argv),
    fail_data(fail_data_ref),
    log(log_ref)
{
    try
    {
        resources_map = new ResourcesMap(&comparators::compare_string);
        hotkeys_info  = new HotkeysMap(&comparators::compare_char);
    }
    catch (std::bad_alloc&)
    {
        delete resources_map;
        delete hotkeys_info;
        throw;
    }
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
        // Free the map itself
        delete resources_map;
    }

    // Cleanup hotkeys map
    // Keys/values in this map are static members of the DrbdMon class
    hotkeys_info->clear();
    delete hotkeys_info;
}

// @throws std::bad_alloc
void DrbdMon::run()
{
    PropsMap*               event_props     {nullptr};
    TermSize*               term_size       {nullptr};
    CompactDisplay*         display_impl    {nullptr};
    GenericDisplay*         display         {nullptr};
    EventsSourceSpawner*    events_source   {nullptr};
    EventsIo*               events_io       {nullptr};

    try
    {
        setup_hotkeys_info();
        try
        {
            event_props   = new PropsMap(&comparators::compare_string);
            term_size     = new TermSize();
            display_impl  = new CompactDisplay(std::cout, *resources_map, log, *hotkeys_info);
            display       = dynamic_cast<GenericDisplay*> (display_impl);

            configurables = new Configurable*[3];
            configurables[0] = dynamic_cast<Configurable*> (this);
            configurables[1] = dynamic_cast<Configurable*> (display_impl);
            configurables[2] = nullptr;
            configure_options();

            events_source = new EventsSourceSpawner();

            if (term_size->probe_terminal_size())
            {
                display->set_terminal_size(term_size->get_size_x(), term_size->get_size_y());
            }

            events_source->spawn_source();
            events_io = new EventsIo(events_source->get_events_source_fd());

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
                                    if (update_counter >= MAX_EVENT_BUNDLE)
                                    {
                                        display->status_display();
                                        update_counter = 0;
                                    }
                                    else
                                    {
                                        ++update_counter;
                                    }
                                }
                            }
                            else
                            {
                                throw EventMessageException();
                            }
                            display->status_display();
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
                                    std::cerr << "DrbdMon: Received signal, exiting" << std::endl;
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
                                if (c == HOTKEY_CLEAR_MSG)
                                {
                                    log.clear();
                                    display->status_display();
                                }
                                else
                                if (c == HOTKEY_QUIT)
                                {
                                    fin_action = DrbdMon::finish_action::TERMINATE;
                                    shutdown = true;
                                    std::cerr << "DrbdMon: Quit command, exiting" << std::endl;
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
                            "DrbdMon: Internal error: Unexpected event type returned by EventsIo"
                        );
                        break;
                }
            }
        }
        catch (std::ios_base::failure&)
        {
            log.add_entry(
                MessageLog::log_level::ALERT,
                "DRBD events source I/O error"
            );
            fin_action = DrbdMon::finish_action::RESTART_DELAYED;
            fail_data = DrbdMon::fail_info::EVENTS_IO;
        }
        catch (EventsSourceException&)
        {
            log.add_entry(
                MessageLog::log_level::ALERT,
                "DRBD events source failed"
            );
            fin_action = DrbdMon::finish_action::RESTART_DELAYED;
            fail_data = DrbdMon::fail_info::EVENTS_SOURCE;
        }
        catch (EventsIoException&)
        {
            log.add_entry(
                MessageLog::log_level::ALERT,
                "DRBD events source I/O error"
            );
            fin_action = DrbdMon::finish_action::RESTART_DELAYED;
            fail_data = DrbdMon::fail_info::EVENTS_IO;
        }
        catch (EventException&)
        {
            log.add_entry(
                MessageLog::log_level::ALERT,
                "Received invalid DRBD events source data"
            );
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
        cleanup(display, term_size, event_props, events_io, events_source);
        throw;
    }

    cleanup(display, term_size, event_props, events_io, events_source);
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
                parse_event_props(tokens, event_props);

                process_event_message(event_mode, event_type, event_props);
            }
        }
    }
    catch (EventMessageException& msg_exc)
    {
        std::cerr << "DEBUG: EventMessageException: Malformed event message:\n" <<
                  event_line << std::endl;
        throw msg_exc;
    }
    catch (EventObjectException& obj_exc)
    {
        std::cerr << "DEBUG: EventObjectException: Event message references nonexistent object:\n" <<
                  event_line << std::endl;
        throw obj_exc;
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::process_event_message(
    std::string& event_mode,
    std::string& event_type,
    PropsMap& event_props
)
{
    if (event_mode == MODE_EXISTS || event_mode == MODE_CREATE)
    {
        if (event_type == TYPE_CONNECTION)
        {
            create_connection(event_props);
        }
        else
        if (event_type == TYPE_DEVICE)
        {
            create_device(event_props);
        }
        else
        if (event_type == TYPE_PEER_DEVICE)
        {
            create_peer_device(event_props);
        }
        else
        if (event_type == TYPE_RESOURCE)
        {
            create_resource(event_props);
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

            // In case that multiple "exists -" lines are received,
            // which is actually not supposed to happen, avoid spamming
            // the message log
            fail_data = fail_info::NONE;
        }
    }
    else
    if (event_mode == MODE_CHANGE)
    {
        if (event_type == TYPE_CONNECTION)
        {
            update_connection(event_props);
        }
        else
        if (event_type == TYPE_DEVICE)
        {
            update_device(event_props);
        }
        else
        if (event_type == TYPE_PEER_DEVICE)
        {
            update_peer_device(event_props);
        }
        else
        if (event_type == TYPE_RESOURCE)
        {
            update_resource(event_props);
        }
        // unknown object types are skipped
    }
    else
    if (event_mode == MODE_DESTROY)
    {
        if (event_type == TYPE_CONNECTION)
        {
            destroy_connection(event_props);
        }
        else
        if (event_type == TYPE_DEVICE)
        {
            destroy_device(event_props);
        }
        else
        if (event_type == TYPE_PEER_DEVICE)
        {
            destroy_peer_device(event_props);
        }
        else
        if (event_type == TYPE_RESOURCE)
        {
            destroy_resource(event_props);
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
// @throws std::bad_alloc
void DrbdMon::parse_event_props(StringTokenizer& tokens, PropsMap& event_props)
{
    while (tokens.has_next())
    {
        std::string prop_entry = tokens.next();

        size_t split_index = prop_entry.find(":");
        if (split_index != std::string::npos)
        {
            std::string* key = new std::string(prop_entry.substr(0, split_index));
            ++split_index;
            std::string* value = new std::string(prop_entry.substr(split_index, prop_entry.length() - split_index));
            try
            {
                event_props.insert(key, value);
            }
            catch (dsaext::DuplicateInsertException& dup_exc)
            {
                // DEBUG: duplicate key, malformed event line
                // Encountering this problem should possibly restart everything from scratch
                std::cerr << "DEBUG: WARNING: Malformed event line, duplicate key" << std::endl;
                delete key;
                delete value;
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

// @throws std::bad_alloc, EventMessageException
void DrbdMon::create_connection(PropsMap& event_props)
{
    try
    {
        DrbdResource& res = get_resource(event_props);

        DrbdConnection* conn {nullptr};
        try
        {
            conn = DrbdConnection::new_from_props(event_props);
            conn->update(event_props);
            res.add_connection(conn);
        }
        catch (dsaext::DuplicateInsertException& dup_exc)
        {
            delete conn;
            log.add_entry(
                MessageLog::log_level::ALERT,
                "Duplicate DRBD connection creation reported by the DRBD events source"
            );
        }
        catch (EventMessageException& msg_exc)
        {
            delete conn;
            throw msg_exc;
        }
        catch (std::bad_alloc& out_of_memory_exc)
        {
            delete conn;
            throw out_of_memory_exc;
        }
    }
    catch (EventObjectException& nonexistent_object_exc)
    {
        // ignored
    }
}

// @throws std::bad_alloc, EventMessageException
void DrbdMon::create_device(PropsMap& event_props)
{
    try
    {
        DrbdResource& res = get_resource(event_props);

        DrbdVolume* vol {nullptr};
        try
        {
            vol = DrbdVolume::new_from_props(event_props);
            vol->update(event_props);
            res.add_volume(vol);
        }
        catch (EventMessageException& msg_exc)
        {
            delete vol;
            throw msg_exc;
        }
        catch (std::bad_alloc& out_of_memory_exc)
        {
            delete vol;
            throw out_of_memory_exc;
        }
    }
    catch (EventObjectException& nonexistent_object_exc)
    {
        // ignored
    }
}

// @throws std::bad_alloc, EventMessageException
void DrbdMon::create_peer_device(PropsMap& event_props)
{
    try
    {
        DrbdResource& res = get_resource(event_props);
        DrbdConnection& conn = get_connection(res, event_props);

        DrbdVolume* vol {nullptr};
        try
        {
            vol = DrbdVolume::new_from_props(event_props);
            vol->update(event_props);
            vol->set_connection(&conn);
            conn.add_volume(vol);
        }
        catch (EventMessageException& msg_exc)
        {
            delete vol;
            throw msg_exc;
        }
        catch (std::bad_alloc& out_of_memory_exc)
        {
            delete vol;
            throw out_of_memory_exc;
        }
    }
    catch (EventObjectException& nonexistent_object_exc)
    {
        // ignored
    }
}

// @throws std::bad_alloc, EventMessageException
void DrbdMon::create_resource(PropsMap& event_props)
{
    DrbdResource* res {nullptr};
    std::string* res_name {nullptr};
    try
    {
        res = DrbdResource::new_from_props(event_props);
        res->update(event_props);
        res_name = new std::string(res->get_name());
        resources_map->insert(res_name, res);
    }
    catch (dsaext::DuplicateInsertException& dup_exc)
    {
        delete res;
        delete res_name;
        log.add_entry(
            MessageLog::log_level::ALERT,
            "Duplicate DRBD resource creation reported by the DRBD events source"
        );
    }
    catch (EventMessageException& msg_exc)
    {
        delete res;
        delete res_name;
        throw msg_exc;
    }
    catch (std::bad_alloc& out_of_memory_exc)
    {
        delete res;
        delete res_name;
        throw out_of_memory_exc;
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::update_connection(PropsMap& event_props)
{
    DrbdResource& res = get_resource(event_props);
    DrbdConnection& conn = get_connection(res, event_props);
    conn.update(event_props);
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::update_device(PropsMap& event_props)
{
    DrbdResource& res = get_resource(event_props);
    DrbdVolume& vol = get_device(dynamic_cast<VolumesContainer&> (res), event_props);
    vol.update(event_props);
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::update_peer_device(PropsMap& event_props)
{
    DrbdResource& res = get_resource(event_props);
    DrbdConnection& conn = get_connection(res, event_props);
    DrbdVolume& vol = get_device(dynamic_cast<VolumesContainer&> (conn), event_props);
    vol.update(event_props);
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
void DrbdMon::update_resource(PropsMap& event_props)
{
    DrbdResource& res = get_resource(event_props);
    res.update(event_props);
}

// @throws std::bad_alloc, EventMessageException
void DrbdMon::destroy_connection(PropsMap& event_props)
{
    try
    {
        DrbdResource& res = get_resource(event_props);
        DrbdConnection& conn = get_connection(res, event_props);
        res.remove_connection(conn.get_name());
    }
    catch (EventObjectException& nonexistent_object_exc)
    {
        // ignored
    }
}

// @throws std::bad_alloc, EventMessageException
void DrbdMon::destroy_device(PropsMap& event_props)
{
    try
    {
        DrbdResource& res = get_resource(event_props);
        DrbdVolume& vol = get_device(dynamic_cast<VolumesContainer&> (res), event_props);
        res.remove_volume(vol.get_volume_nr());
    }
    catch (EventObjectException& nonexistent_object_exc)
    {
        // ignored
    }
}

// @throws std::bad_alloc, EventMessageException
void DrbdMon::destroy_peer_device(PropsMap& event_props)
{
    try
    {
        DrbdResource& res = get_resource(event_props);
        DrbdConnection& conn = get_connection(res, event_props);
        // May report non-existing volume if required
        static_cast<void> (get_device(dynamic_cast<VolumesContainer&> (conn), event_props));

        std::string* vol_nr_str = event_props.get(&DrbdVolume::PROP_KEY_VOL_NR);
        if (vol_nr_str != nullptr)
        {
            try
            {
                uint16_t vol_nr = DrbdVolume::parse_volume_nr(*vol_nr_str);
                conn.remove_volume(vol_nr);
            }
            catch (NumberFormatException& nf_exc)
            {
                throw EventMessageException();
            }
        }
        else
        {
            throw EventMessageException();
        }
    }
    catch (EventObjectException& nonexistent_object_exc)
    {
        // ignored
    }
}

// @throws EventMessageException
void DrbdMon::destroy_resource(PropsMap& event_props)
{
    std::string* res_name = event_props.get(&DrbdResource::PROP_KEY_RES_NAME);
    if (res_name != nullptr)
    {
        ResourcesMap::Node* node = resources_map->get_node(res_name);
        if (node != nullptr)
        {
            delete node->get_key();
            delete node->get_value();
            resources_map->remove_node(node);
        }
    }
    else
    {
        throw EventMessageException();
    }
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
DrbdConnection& DrbdMon::get_connection(DrbdResource& res, PropsMap& event_props)
{
    DrbdConnection* conn {nullptr};
    std::string* conn_name = event_props.get(&DrbdConnection::PROP_KEY_CONN_NAME);
    if (conn_name != nullptr)
    {
        conn = res.get_connection(*conn_name);
    }
    else
    {
        throw EventMessageException();
    }
    if (conn == nullptr)
    {
        std::string error_message = "Non-existent connection ";
        error_message += *conn_name;
        error_message += " referenced by the DRBD events source";
        log.add_entry(
            MessageLog::log_level::ALERT,
            error_message
        );
        throw EventObjectException();
    }
    return *conn;
}

// @throws EventMessageException, EventObjectException
DrbdVolume& DrbdMon::get_device(VolumesContainer& vol_con, PropsMap& event_props)
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
        catch (NumberFormatException& nf_exc)
        {
            throw EventMessageException();
        }
    }
    else
    {
        throw EventMessageException();
    }
    if (vol == nullptr)
    {
        log.add_entry(
            MessageLog::log_level::ALERT,
            "Non-existent volume id referenced by the DRBD events source"
        );
        throw EventObjectException();
    }
    return *vol;
}

// @throws std::bad_alloc, EventMessageException, EventObjectException
DrbdResource& DrbdMon::get_resource(PropsMap& event_props)
{
    DrbdResource* res {nullptr};
    std::string* res_name = event_props.get(&DrbdResource::PROP_KEY_RES_NAME);
    if (res_name != nullptr)
    {
        res = resources_map->get(res_name);
    }
    else
    {
        throw EventMessageException();
    }
    if (res == nullptr)
    {
        std::string error_message = "Non-existent resource ";
        error_message += *res_name;
        error_message += " referenced by the DRBD events source";
        log.add_entry(
            MessageLog::log_level::ALERT,
            error_message
        );
        throw EventObjectException();
    }
    return *res;
}

// @throws std::bad_alloc
void DrbdMon::setup_hotkeys_info()
{
    hotkeys_info->append(&HOTKEY_QUIT, &DESC_QUIT);
    hotkeys_info->append(&HOTKEY_REPAINT, &DESC_REPAINT);
    hotkeys_info->append(&HOTKEY_CLEAR_MSG, &DESC_CLEAR_MSG);
}

// Frees resources
// @throws std::bad_alloc
void DrbdMon::cleanup(
    GenericDisplay*         display,
    TermSize*               term_size,
    PropsMap*               event_props,
    EventsIo*               events_io,
    EventsSourceSpawner*    events_source
)
{
    if (event_props != nullptr)
    {
        clear_event_props(*event_props);
    }

    delete display;
    delete term_size;
    delete event_props;

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
    delete events_io;
    delete events_source;

    delete[] configurables;
}

// @throws std::bad_alloc
void DrbdMon::configure_options()
{
    Args* arg_list {nullptr};
    options = new OptionsMap(&comparators::compare_string);

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
            arg_list = new Args(arg_count, arg_values);
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
        delete arg_list;
        options_cleanup();
        throw;
    }
    catch (std::bad_alloc&)
    {
        delete arg_list;
        options_cleanup();
        throw;
    }

    delete arg_list;
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
        delete options;
        options = nullptr;
    }
}

// @throws std::bad_alloc
void DrbdMon::add_config_option(Configurable& owner, const ConfigOption& conf_option)
{
    if (options != nullptr)
    {
        try
        {
            DrbdMon::option_entry* entry = new DrbdMon::option_entry { owner, conf_option };
            options->insert(&conf_option.key, entry);
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
}

void DrbdMon::options_help() noexcept
{
    // no-op; the DrbdMon instance does not have any configurable options at this time
}

// @throws std::bad_alloc
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
}

// @throws std::bad_alloc
void DrbdMon::set_option(std::string& key, std::string& value)
{
    // no-op; the DrbdMon instance does not have any configurable options at this time
}
