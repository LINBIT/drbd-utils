#include <MessageLog.h>

#include <iostream>

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

    log_entries = std::unique_ptr<entry*[]>(new entry*[capacity]);
    date_buffer = std::unique_ptr<char[]>(new char[DATE_BUFFER_SIZE]);
}

MessageLog::~MessageLog() noexcept
{
    clear_impl();
}

bool MessageLog::has_entries() const
{
    return (filled || index > 0);
}

// @throws std::bad_alloc
void MessageLog::add_entry(const log_level level, const std::string& message)
{
    std::unique_ptr<std::string> message_copy;
    if (format_date())
    {
        message_copy = std::unique_ptr<std::string>(new std::string(date_buffer.get()));
        *message_copy += message;
    }
    else
    {
        message_copy = std::unique_ptr<std::string>(new std::string(message));
    }

    if (filled)
    {
        delete log_entries[index]->message;
        log_entries[index]->message = message_copy.release();
        log_entries[index]->level = level;
    }
    else
    {
        // Keep unique_ptr ownership of message_copy while constructing the new
        // entry, otherwise message_copy would not be deallocated if
        // entry construction throws
        log_entries[index] = new MessageLog::entry {level, message_copy.get()};
        static_cast<void> (message_copy.release());
    }

    ++index;
    if (index >= capacity)
    {
        filled = true;
        index = 0;
    }
}

// @throws std::bad_alloc
void MessageLog::add_entry(const log_level level, const char* message)
{
    std::string message_str(message);
    add_entry(level, message_str);
}

void MessageLog::clear()
{
    clear_impl();
    filled = false;
    index = 0;
}

void MessageLog::clear_impl() noexcept
{
    size_t limit = filled ? capacity : index;
    for (size_t slot = 0; slot < limit; ++slot)
    {
        delete log_entries[slot]->message;
        delete log_entries[slot];
    }
}

void MessageLog::display_messages(std::ostream& out) const
{
    out.clear();
    if (filled || index > 0)
    {
        out << MESSAGES_HEADER << '\n';

        MessageLog::EntriesIterator iter(*this);
        size_t count = iter.get_size();
        for (size_t slot = 0; slot < count; ++slot)
        {
            MessageLog::entry* log_entry = iter.next();

            const char* mark = F_ALERT_MARK;
            const char* format = F_ALERT;
            switch (log_entry->level)
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

            out << "    " << mark << " " << format << *(log_entry->message) << F_RESET << '\n';
        }
        out << std::flush;
    }
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

MessageLog::EntriesIterator::EntriesIterator(const MessageLog& log_ref):
    log_obj(log_ref),
    circular_mode(log_ref.filled)
{
    if (circular_mode)
    {
        iter_index = log_obj.index;
    }
}

MessageLog::EntriesIterator::~EntriesIterator() noexcept
{
}

MessageLog::entry* MessageLog::EntriesIterator::next()
{
    MessageLog::entry* item {nullptr};
    if (circular_mode || iter_index < log_obj.index)
    {
        item = log_obj.log_entries[iter_index];
        ++iter_index;
        if (iter_index >= log_obj.capacity)
        {
            circular_mode = false;
            iter_index = 0;
        }
    }
    return item;
}

size_t MessageLog::EntriesIterator::get_size() const
{
    return log_obj.filled ? log_obj.capacity : log_obj.index;
}

bool MessageLog::EntriesIterator::has_next() const
{
    return circular_mode || iter_index < log_obj.index;
}
