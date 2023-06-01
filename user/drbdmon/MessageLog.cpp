#include <MessageLog.h>

#include <iostream>
#include <comparators.h>
#include <stdexcept>

const uint64_t MessageLog::ID_NONE    = ~static_cast<uint64_t> (0);

const char* MessageLog::MESSAGES_HEADER = "\x1B[0;30;42m MESSAGES \x1B[0m";

const char* MessageLog::F_ALERT_MARK = "\x1B[1;33;41m ALERT \x1B[0m";
const char* MessageLog::F_WARN_MARK  = "\x1B[1;33;45m WARN  \x1B[0m";
const char* MessageLog::F_INFO_MARK  = "\x1B[0;30;32m INFO  \x1B[0m";
const char* MessageLog::F_ALERT      = "\x1B[1;31m";
const char* MessageLog::F_WARN       = "\x1B[1;33m";
const char* MessageLog::F_INFO       = "\x1B[1;32m";
const char* MessageLog::F_RESET      = "\x1B[0m";

const std::string MessageLog::CAPACITY_ERROR = "MessageLog(): Illegal capacity (entries < 1)";

const size_t MessageLog::DATE_BUFFER_SIZE = 24;
const char*  MessageLog::DATE_FORMAT = "%FT%TZ ";
const size_t MessageLog::DATE_LENGTH = 21;

// @throws std::bad_alloc, std::out_of_range
MessageLog::MessageLog(const size_t entries):
    capacity(entries)
{
    if (capacity < 1)
    {
        throw std::out_of_range(CAPACITY_ERROR);
    }

    msg_queue   = std::unique_ptr<Queue>(new Queue(comparators::compare<uint64_t>));
    date_buffer = std::unique_ptr<char[]>(new char[DATE_BUFFER_SIZE]);
}

MessageLog::~MessageLog() noexcept
{
    clear_impl();
}

bool MessageLog::has_entries() const
{
    std::unique_lock<std::recursive_mutex> lock(queue_lock);

    return msg_queue->get_size() >= 1;
}

uint64_t MessageLog::add_entry(const log_level level, const std::string& message)
{
    return add_entry(level, message.c_str());
}

// @throws std::bad_alloc
uint64_t MessageLog::add_entry(const log_level level, const char* const message)
{
    std::unique_lock<std::recursive_mutex> lock(queue_lock);

    if (msg_queue->get_size() >= capacity)
    {
        Queue::Node* first_node = msg_queue->get_first_node();
        if (first_node != nullptr)
        {
            Entry* const first_entry = first_node->get_value();
            delete first_entry;
            msg_queue->remove_node(first_node);
        }
    }

    const char* date_ptr = nullptr;
    if (format_date())
    {
        date_ptr = date_buffer.get();
    }

    const uint64_t entry_id = next_id;
    std::unique_ptr<Entry> log_entry = std::unique_ptr<Entry>(new Entry(entry_id, level, date_ptr, message));
    ++next_id;

    try
    {
        msg_queue->insert(&log_entry->get_id(), log_entry.get());
    }
    catch (dsaext::DuplicateInsertException&)
    {
        // Not supposed to happen
        throw std::logic_error("DuplicateInsertException for a key that is supposed to be unique");
    }
    log_entry.release();

    if (observer != nullptr)
    {
        observer->notify_log_changed();
    }

    return entry_id;
}

bool MessageLog::delete_entry(const uint64_t entry_id)
{
    std::unique_lock<std::recursive_mutex> lock(queue_lock);

    Queue::Node* const log_entry_node = msg_queue->get_node(&entry_id);
    if (log_entry_node != nullptr)
    {
        Entry* const log_entry = log_entry_node->get_value();
        delete log_entry;
        msg_queue->remove_node(log_entry_node);
    }

    if (observer != nullptr)
    {
        observer->notify_log_changed();
    }

    return log_entry_node != nullptr;
}

MessageLog::Entry* MessageLog::get_previous_entry(const uint64_t entry_id)
{
    std::unique_lock<std::recursive_mutex> lock(queue_lock);

    return msg_queue->get_less_value(&entry_id);
}

MessageLog::Entry* MessageLog::get_entry(const uint64_t entry_id)
{
    std::unique_lock<std::recursive_mutex> lock(queue_lock);

    return msg_queue->get(&entry_id);
}

MessageLog::Entry* MessageLog::get_next_entry(const uint64_t entry_id)
{
    std::unique_lock<std::recursive_mutex> lock(queue_lock);

    return msg_queue->get_greater_value(&entry_id);
}

MessageLog::Entry* MessageLog::get_entry_near(const uint64_t entry_id)
{
    Entry* near_entry = nullptr;

    std::unique_lock<std::recursive_mutex> lock(queue_lock);

    Queue::Node* node = msg_queue->get_ceiling_node(&entry_id);
    if (node != nullptr)
    {
        near_entry = node->get_value();
    }
    else
    {
        node = msg_queue->get_floor_node(&entry_id);
        if (node != nullptr)
        {
            near_entry = node->get_value();
        }
    }

    return near_entry;
}

// Caller must hold queue_lock while working with an iterator
MessageLog::Queue::ValuesIterator MessageLog::iterator()
{
    return Queue::ValuesIterator(*msg_queue);
}

// Caller must hold queue_lock while working with an iterator
MessageLog::Queue::ValuesIterator MessageLog::iterator(const uint64_t entry_id)
{
    Queue& queue_ref = *msg_queue;
    Queue::Node* const start_node = msg_queue->get_node(&entry_id);
    return start_node != nullptr ? Queue::ValuesIterator(queue_ref, *start_node) : Queue::ValuesIterator(queue_ref);
}

void MessageLog::clear()
{
    std::unique_lock<std::recursive_mutex> lock(queue_lock);

    clear_impl();
    next_id = 0;

    if (observer != nullptr)
    {
        observer->notify_log_changed();
    }
}

void MessageLog::clear_impl() noexcept
{
    Queue::ValuesIterator iter(*msg_queue);
    while (iter.has_next())
    {
        Entry* const cur_entry = iter.next();
        delete cur_entry;
    }
    msg_queue->clear();
}

void MessageLog::display_messages(std::ostream& out) const
{
    out.clear();

    if (msg_queue->get_size() >= 1)
    {
        out << MESSAGES_HEADER << '\n';

        Queue::ValuesIterator iter(*msg_queue);
        const size_t count = iter.get_size();
        for (size_t slot = 0; slot < count; ++slot)
        {
            Entry* const log_entry = iter.next();

            const char* mark = F_ALERT_MARK;
            const char* format = F_ALERT;
            switch (log_entry->get_log_level())
            {
                case MessageLog::log_level::INFO:
                    mark = F_INFO_MARK;
                    format = F_INFO;
                    break;
                case MessageLog::log_level::WARN:
                    mark = F_WARN_MARK;
                    format = F_WARN;
                    break;
                case MessageLog::log_level::ALERT:
                    // fall-through
                default:
                    // defaults to the alert format
                    break;
            }

            out << "    " << mark << " " << format << log_entry->get_date_label() << " " <<
                log_entry->get_message() << F_RESET << '\n';
        }
        out << std::flush;
    }
}

void MessageLog::set_observer(MessageLogObserver* const new_observer)
{
    std::unique_lock<std::recursive_mutex> lock(queue_lock);

    observer = new_observer;
}

void MessageLog::cancel_observer() noexcept
{
    std::unique_lock<std::recursive_mutex> lock(queue_lock);

    observer = nullptr;
}

bool MessageLog::format_date() noexcept
{
    bool rc = false;
    if (gettimeofday(&utc_time, NULL) == 0)
    {
        if (gmtime_r(&(utc_time.tv_sec), &time_fields) != nullptr)
        {
            if (strftime(date_buffer.get(), DATE_BUFFER_SIZE, DATE_FORMAT, &time_fields) == DATE_LENGTH)
            {
                rc = true;
            }
        }
    }
    return rc;
}

MessageLog::Entry::Entry(
    const uint64_t      entry_id,
    const log_level     entry_level,
    const char* const   date_buffer,
    const char* const   log_message
)
{
    id          = entry_id;
    level       = entry_level;
    if (date_buffer != nullptr)
    {
        date_label.append(date_buffer);
    }
    if (log_message != nullptr)
    {
        message.append(log_message);
    }
    else
    {
        message.append("<Attempt to log an entry with log_message == nullptr>");
    }
}

MessageLog::Entry::~Entry() noexcept
{
}

const uint64_t& MessageLog::Entry::get_id() const noexcept
{
    return id;
}

MessageLog::log_level MessageLog::Entry::get_log_level() const noexcept
{
    return level;
}

const std::string& MessageLog::Entry::get_message() const noexcept
{
    return message;
}

const std::string& MessageLog::Entry::get_date_label() const noexcept
{
    return date_label;
}
