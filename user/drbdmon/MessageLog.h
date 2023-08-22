#ifndef MESSAGELOG_H
#define MESSAGELOG_H

#include <default_types.h>
#include <new>
#include <memory>
#include <string>
#include <mutex>
#include <stdexcept>

#include <MessageLogObserver.h>
// https://github.com/raltnoeder/cppdsaext
#include <dsaext.h>
#include <QTree.h>

extern "C"
{
    #include <sys/time.h>
    #include <time.h>
}

class MessageLog
{
  public:
    static const uint64_t   ID_NONE;

    mutable std::recursive_mutex    queue_lock;

    enum class log_level : uint32_t
    {
        INFO,
        WARN,
        ALERT
    };

    class Entry
    {
      public:
        Entry(
            const uint64_t      entry_id,
            const log_level     entry_level,
            const char* const   date_buffer,
            const char* const   log_message
        );
        virtual ~Entry() noexcept;

        virtual log_level get_log_level() const noexcept;
        virtual const uint64_t& get_id() const noexcept;
        virtual const std::string& get_message() const noexcept;
        virtual const std::string& get_date_label() const noexcept;

      private:
        uint64_t    id      {0};
        log_level   level   {log_level::ALERT};
        std::string date_label;
        std::string message;
    };

    using Queue = QTree<uint64_t, Entry>;

    static const char* MESSAGES_HEADER;

    static const char* F_ALERT_MARK;
    static const char* F_WARN_MARK;
    static const char* F_INFO_MARK;
    static const char* F_ALERT;
    static const char* F_WARN;
    static const char* F_INFO;
    static const char* F_RESET;

    static const size_t DATE_BUFFER_SIZE;
    static const char*  DATE_FORMAT;
    static const size_t DATE_LENGTH;

    static const std::string CAPACITY_ERROR;

    // @throws std::bad_alloc, std::out_of_range
    explicit MessageLog(const size_t entries);
    MessageLog(const MessageLog& orig) = delete;
    MessageLog& operator=(const MessageLog& orig) = delete;
    MessageLog(MessageLog&& orig) = default;
    MessageLog& operator=(MessageLog&& orig) = delete;
    virtual ~MessageLog() noexcept;

    virtual Queue::ValuesIterator iterator();
    virtual Queue::ValuesIterator iterator(const uint64_t entry_id);

    // @throws std::bad_alloc
    virtual bool has_entries() const;
    virtual uint64_t add_entry(log_level level, const std::string& message);
    virtual uint64_t add_entry(log_level level, const char* message);
    virtual bool delete_entry(const uint64_t entry_id);
    virtual Entry* get_previous_entry(const uint64_t entry_id);
    virtual Entry* get_entry(const uint64_t entry_id);
    virtual Entry* get_next_entry(const uint64_t entry_id);
    virtual Entry* get_entry_near(const uint64_t entry_id);
    virtual void clear();
    virtual void display_messages(std::ostream& out) const;

    virtual void set_observer(MessageLogObserver* const new_observer);
    virtual void cancel_observer() noexcept;

  private:
    size_t capacity     {0};
    uint64_t next_id    {0};

    struct timeval  utc_time;
    struct tm       time_fields;

    std::unique_ptr<Queue>  msg_queue;
    std::unique_ptr<char[]> date_buffer;

    MessageLogObserver*     observer;

    void clear_impl() noexcept;
    bool format_date() noexcept;
};

#endif    /* MESSAGELOG_H */
